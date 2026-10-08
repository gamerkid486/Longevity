#include "duel.h"

#include "audio.h"
#include "log.h"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <mutex>
#include <random>
#include <unordered_map>

namespace duel {
    namespace {
        struct NpcGuard {
            Guard guard = Guard::top;
            ULONGLONG nextSwitch = 0;
            ULONGLONG lastSeen = 0;
        };

        std::mutex g_lock;
        std::unordered_map<std::uint32_t, NpcGuard> g_npcs;  // by formID
        std::mt19937 g_rng{ 0xF0A1u };

        Guard g_playerGuard = Guard::top;
        std::atomic<bool> g_playerGuarding{ false };
        float g_accX = 0, g_accY = 0;
        ULONGLONG g_lastMove = 0;
        ULONGLONG g_lastSwitch = 0;

        Snapshot g_snap;
        std::mutex g_snapLock;
        Snapshot g_logged;  // last state written to the log (main thread)

        // Lock-on: Tick measures how far the player faces away from the opponent, and the mouse hook
        // turns a share of that each frame. While guarding, the player's own mouse X is zeroed, so the
        // heading only changes through our counts: that measures degrees per count (the player's mouse
        // sensitivity, signed, so an inverted mouse works too).
        std::atomic<float> g_lockErr{ NAN };
        std::atomic<float> g_degPerCount{ float(tun::lock_on_start_deg_per_count) };
        std::atomic<long> g_lockSent{ 0 };
        float g_lockPrevHeading = NAN;
        int g_lockOpposite = 0;  // samples in a row turning the other way than expected
        ULONGLONG g_lockLastLog = 0;
        std::atomic<unsigned> g_frame{ 0 }, g_lockFrame{ 0 };  // turn at most once per game frame

        // The lock-on target is picked when block starts and kept for the whole hold, as the player
        // asked: no switching to someone else (possibly behind) mid-block. If it dies or gets away,
        // there's no new target until block is released and held again.
        std::uint32_t g_lockHandle = 0;
        bool g_lockLost = false;  // this hold's target is gone; wait for the next block

        void TrackLockOn(game::Actor* player, game::Actor* opponent)
        {
            long sent = g_lockSent.exchange(0);
            if (!opponent || !g_playerGuarding || !tun::lock_on_while_guarding) {
                g_lockErr = NAN;
                g_lockPrevHeading = NAN;
                return;
            }
            float heading = game::HeadingDeg(player);
            if (!std::isnan(g_lockPrevHeading) && std::labs(sent) >= 3) {
                float turned = std::remainder(heading - g_lockPrevHeading, 360.0f);
                float sample = turned / float(sent);
                if (std::fabs(sample) > 0.002f && std::fabs(sample) < 2.0f && std::fabs(turned) < 30.0f) {
                    // Size and direction are learned apart: smoothing a signed value through zero
                    // would blow the turn up. Three samples the other way mean an inverted mouse.
                    float k = g_degPerCount;
                    if ((sample > 0) != (k > 0)) {
                        if (++g_lockOpposite >= 3) {
                            g_degPerCount = -k;
                            g_lockOpposite = 0;
                            fhd::Log("lock-on: mouse turns the other way, direction flipped");
                        }
                    } else {
                        g_lockOpposite = 0;
                        g_degPerCount = std::copysign(0.8f * std::fabs(k) + 0.2f * std::fabs(sample), k);
                    }
                }
            }
            g_lockPrevHeading = heading;
            float err = game::YawErrorDeg(player, opponent);
            g_lockErr = err;
            ++g_frame;
            ULONGLONG now = GetTickCount64();
            if (now - g_lockLastLog >= 1000) {
                g_lockLastLog = now;
                fhd::Log("lock-on: facing %.1f deg off, %.4f deg per count", err, g_degPerCount.load());
            }
        }

        Guard RandomGuard()
        {
            return static_cast<Guard>(std::uniform_int_distribution<int>(1, 3)(g_rng));
        }

        // NPC guard: held for a random time, frozen while the NPC is attacking (its attack keeps the
        // direction it started from), and sometimes copies the player's guard to defend.
        Guard NpcGuardOf(game::Actor* a)
        {
            ULONGLONG now = GetTickCount64();
            std::lock_guard lk(g_lock);
            NpcGuard& st = g_npcs[game::FormID(a)];
            st.lastSeen = now;
            if (now >= st.nextSwitch && !game::IsAttacking(a)) {
                bool mirror = std::uniform_real_distribution<float>(0, 1)(g_rng) < tun::npc_mirror_chance;
                st.guard = mirror ? g_playerGuard : RandomGuard();
                st.nextSwitch = now + std::uniform_int_distribution<int>(tun::npc_guard_hold_min_ms, tun::npc_guard_hold_max_ms)(g_rng);
            }
            if (g_npcs.size() > 256) {
                for (auto it = g_npcs.begin(); it != g_npcs.end();) {
                    it = now - it->second.lastSeen > 60000 ? g_npcs.erase(it) : std::next(it);
                }
            }
            return st.guard;
        }

        Guard GuardOf(game::Actor* a)
        {
            return a == game::Player() ? g_playerGuard : NpcGuardOf(a);
        }

        const RuleRow& RuleFor(Rule id)
        {
            for (const auto& r : kRules) {
                if (r.id == id) return r;
            }
            return kRules[0];
        }
    }

    Snapshot Read()
    {
        std::lock_guard lk(g_snapLock);
        return g_snap;
    }

    void Tick()
    {
        game::Actor* player = game::Player();
        if (!player) return;

        Snapshot s;
        s.playerDuelist = game::IsDuelist(player);
        g_playerGuarding = s.playerDuelist && game::IsBlocking(player);
        s.playerGuarding = g_playerGuarding;
        s.playerGuard = g_playerGuard;

        // Candidates: living actors fighting the player within opponent_max_distance. Any of them can
        // be locked on (spellcasters too); only duelists get a guard indicator and duel rules.
        const float maxSq = float(tun::opponent_max_distance) * float(tun::opponent_max_distance);
        float nearestSq = maxSq, bestScore = INFINITY;
        std::uint32_t nearestDuelist = 0, bestFront = 0;
        if (game::HandleArray* arr = game::HighActorHandles(); arr && arr->data) {
            for (std::uint32_t i = 0; i < arr->size; ++i) {
                game::Actor* a = game::LookupActor(arr->data[i]);
                if (!a) continue;
                if (a != player && !game::IsDead(a)) {
                    game::Actor* target = game::LookupActor(game::CombatTargetHandle(a));
                    bool targetsPlayer = target == player;
                    game::Release(target);
                    float d = game::DistanceSq(a, player);
                    if (targetsPlayer && d < maxSq) {
                        // Prefer enemies in front: someone behind counts as up to three times as far.
                        float score = std::sqrt(d) * (1.0f + std::fabs(game::YawErrorDeg(player, a)) / 90.0f);
                        if (score < bestScore) {
                            bestScore = score;
                            bestFront = arr->data[i];
                        }
                        if (d < nearestSq && game::IsDuelist(a)) {
                            nearestSq = d;
                            nearestDuelist = arr->data[i];
                        }
                    }
                }
                game::Release(a);
            }
        }

        std::uint32_t targetHandle = nearestDuelist;
        if (g_playerGuarding) {
            if (!g_lockHandle && !g_lockLost && bestFront) {  // first target of this hold (not a switch)
                g_lockHandle = bestFront;
                if (game::Actor* t = game::LookupActor(g_lockHandle)) {
                    fhd::Log("lock-on: target %08X (%s), %.0f deg off", game::FormID(t), game::IsDuelist(t) ? "duelist" : "not a duelist",
                        game::YawErrorDeg(player, t));
                    game::Release(t);
                }
            } else if (g_lockHandle) {
                game::Actor* t = game::LookupActor(g_lockHandle);
                const float breakSq = float(tun::lock_on_break_distance) * float(tun::lock_on_break_distance);
                if (!t || game::IsDead(t) || game::DistanceSq(t, player) > breakSq) {
                    fhd::Log("lock-on: target lost (%s); no new target until block is held again", !t ? "gone" : game::IsDead(t) ? "dead" : "too far");
                    g_lockHandle = 0;
                    g_lockLost = true;
                }
                game::Release(t);
            }
            targetHandle = g_lockHandle;
        } else {
            g_lockLost = false;
            g_lockHandle = 0;
        }

        game::Actor* target = game::LookupActor(targetHandle);
        if (target && game::IsDuelist(target)) {
            s.hasOpponent = true;
            s.opponentGuard = NpcGuardOf(target);
            s.opponentAttacking = game::IsAttacking(target);
        }
        TrackLockOn(player, target);
        game::Release(target);

        if (s.playerDuelist != g_logged.playerDuelist) {
            fhd::Log("player duelist: %s (%s)", s.playerDuelist ? "yes" : "no", game::DescribeDuelist(player).c_str());
        }
        if (s.playerGuarding != g_logged.playerGuarding) fhd::Log("player guarding: %s", s.playerGuarding ? "yes" : "no");
        if (s.hasOpponent != g_logged.hasOpponent) fhd::Log("opponent: %s", s.hasOpponent ? "found" : "none");
        g_logged = s;

        std::lock_guard lk(g_snapLock);
        g_snap = s;
    }

    bool CameraHeld() { return g_playerGuarding && tun::lock_camera_while_guarding != 0; }

    long LockOnCounts()
    {
        float err = g_lockErr;
        if (!CameraHeld() || std::isnan(err) || std::fabs(err) < tun::lock_on_deadzone_deg) return 0;
        if (g_lockFrame.exchange(g_frame) == g_frame) return 0;  // already turned this frame
        float step = std::clamp(err * float(tun::lock_on_fraction), -float(tun::lock_on_max_deg_per_frame), float(tun::lock_on_max_deg_per_frame));
        float k = g_degPerCount;
        if (std::fabs(k) < 0.002f) k = std::copysign(0.002f, k);
        long counts = std::lround(step / k);
        if (counts == 0) counts = (step > 0) == (k > 0) ? 1 : -1;  // always nudge outside the dead zone
        g_lockSent += counts;
        return counts;
    }

    void OnMouseMove(long dx, long dy)
    {
        if (!g_playerGuarding) {
            g_accX = g_accY = 0;
            return;
        }
        ULONGLONG now = GetTickCount64();
        if (now - g_lastMove > ULONGLONG(tun::flick_decay_ms)) g_accX = g_accY = 0;
        g_lastMove = now;
        g_accX += float(dx);
        g_accY += float(dy);

        float ax = std::fabs(g_accX), ay = std::fabs(g_accY);
        int sx = 0, sy = 0;
        if (ax >= tun::flick_threshold && ax > ay * tun::flick_axis_dominance) sx = g_accX > 0 ? 1 : -1;
        else if (ay >= tun::flick_threshold && ay > ax * tun::flick_axis_dominance) sy = g_accY > 0 ? 1 : -1;
        if ((sx || sy) && now - g_lastSwitch < ULONGLONG(tun::flick_cooldown_ms)) {
            static int logged = 0;
            if (logged++ < 20) fhd::Log("flick %d %d held back by the cooldown (%llu ms after the last switch)", sx, sy, now - g_lastSwitch);
        }
        if ((sx || sy) && now - g_lastSwitch >= ULONGLONG(tun::flick_cooldown_ms)) {
            for (const auto& g : kGuards) {
                if (g.flickDx == sx && g.flickDy == sy && g.id != g_playerGuard) {
                    g_playerGuard = g.id;
                    g_lastSwitch = now;
                    fhd::Log("player guard -> %s", g.label);
                }
            }
            g_accX = g_accY = 0;
        }
    }

    bool OnMeleeHit(game::Actor* victim, game::Actor* aggressor, const void* weapon)
    {
        game::Actor* player = game::Player();
        if (victim != player && aggressor != player) {
            static int logged = 0;
            if (logged++ < 10) fhd::Log("melee hit: npc vs npc, not a duel");
            return RuleFor(Rule::not_a_duel).cancelHit;
        }
        if (!game::IsDuelist(victim) || !game::IsMeleeWeapon(weapon)) {
            fhd::Log("melee hit: not a duel (%s attacks, victim %s, weapon %s)", aggressor == player ? "player" : "npc",
                game::DescribeDuelist(victim).c_str(),
                game::IsMeleeWeapon(weapon) ? "melee" : "not melee");
            return RuleFor(Rule::not_a_duel).cancelHit;
        }
        // The player only defends while holding block; NPCs always hold their guard.
        Guard vg = victim == player && !g_playerGuarding ? Guard::none : GuardOf(victim);
        Guard ag = GuardOf(aggressor);
        bool victimAttacking = game::IsAttacking(victim);

        const RuleRow* hit = &RuleFor(Rule::landed);
        const RuleRow& blocked = RuleFor(Rule::blocked);
        bool guardsMatch = blocked.guards == Cmp::any || (blocked.guards == Cmp::equal) == (vg == ag);
        if (guardsMatch && !(blocked.victimNotAttacking && victimAttacking)) hit = &blocked;

        fhd::Log("melee hit: %s attacks %s, guards %d vs %d%s -> %s", aggressor == player ? "player" : "npc",
            victim == player ? "player" : "npc", int(ag), int(vg), victimAttacking ? " (victim mid-attack)" : "",
            hit->id == Rule::blocked ? "blocked" : "landed");

        if (hit->sound != Snd::none) audio::Play(hit->sound);
        if (hit->attackerAnimEvent) {
            // Comma-separated: the first event the attacker's behavior graph accepts wins.
            std::string list = hit->attackerAnimEvent, used = "none accepted";
            for (std::size_t at = 0; at <= list.size();) {
                std::size_t end = list.find(',', at);
                if (end == std::string::npos) end = list.size();
                std::string ev = list.substr(at, end - at);
                if (!ev.empty() && game::NotifyAnimation(aggressor, ev.c_str())) {
                    used = ev;
                    break;
                }
                at = end + 1;
            }
            fhd::Log("attacker reaction: %s", used.c_str());
        }
        return hit->cancelHit;
    }
}

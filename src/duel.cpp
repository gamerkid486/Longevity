#include "duel.h"

#include "audio.h"
#include "log.h"

#include <windows.h>

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

        // Opponent: nearest living duelist whose combat target is the player.
        const float maxSq = float(tun::opponent_max_distance) * float(tun::opponent_max_distance);
        float bestSq = maxSq;
        std::uint32_t bestHandle = 0;
        if (game::HandleArray* arr = game::HighActorHandles(); arr && arr->data) {
            for (std::uint32_t i = 0; i < arr->size; ++i) {
                game::Actor* a = game::LookupActor(arr->data[i]);
                if (!a) continue;
                if (a != player && !game::IsDead(a) && game::IsDuelist(a)) {
                    game::Actor* target = game::LookupActor(game::CombatTargetHandle(a));
                    bool targetsPlayer = target == player;
                    game::Release(target);
                    float d = game::DistanceSq(a, player);
                    if (targetsPlayer && d < bestSq) {
                        bestSq = d;
                        bestHandle = arr->data[i];
                    }
                }
                game::Release(a);
            }
        }
        if (game::Actor* best = game::LookupActor(bestHandle)) {
            s.hasOpponent = true;
            s.opponentGuard = NpcGuardOf(best);
            s.opponentAttacking = game::IsAttacking(best);
            game::Release(best);
        }

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

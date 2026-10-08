#include "hooks.h"

#include "duel.h"
#include "game.h"
#include "log.h"

#include <windows.h>

#include <cstring>
#include <deque>
#include <mutex>

namespace hooks {
    namespace {
        using ProcessHit_t = void (*)(game::Actor* victim, void* hitData);
        using MouseMove_t = void (*)(void* self, void* event, void* data);
        using Update_t = void (*)(game::Actor* self, float delta);

        ProcessHit_t g_origProcessHit = nullptr;
        MouseMove_t g_origMouseMove = nullptr;
        Update_t g_origUpdate = nullptr;

        std::mutex g_msgLock;
        std::deque<std::string> g_messages;

        // --- hooked functions ---------------------------------------------------------------

        void HitHook(game::Actor* victim, void* hit)
        {
            bool cancel = false;
            game::Actor* aggressor = game::LookupActor(game::Field<std::uint32_t>(hit, lay::hit_aggressor));
            if (victim && aggressor && !game::IsDead(victim)) {
                cancel = duel::OnMeleeHit(victim, aggressor, game::Field<void*>(hit, lay::hit_weapon));
            }
            game::Release(aggressor);
            if (!cancel) g_origProcessHit(victim, hit);
        }

        void MouseMoveHook(void* self, void* event, void* data)
        {
            int dx = game::Field<std::int32_t>(event, lay::mouse_move_x);
            int dy = game::Field<std::int32_t>(event, lay::mouse_move_y);
            if (duel::OnMouseMove(dx, dy)) return;  // guard flick: camera stays still
            g_origMouseMove(self, event, data);
        }

        void UpdateHook(game::Actor* self, float delta)
        {
            g_origUpdate(self, delta);
            if (self != game::Player()) return;
            duel::Tick();
            std::deque<std::string> msgs;
            {
                std::lock_guard lk(g_msgLock);
                msgs.swap(g_messages);
            }
            for (const auto& m : msgs) game::Notification(m.c_str());
        }

        // --- patching helpers ---------------------------------------------------------------

        bool InExeText(std::uintptr_t p)
        {
            auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(game::Base());
            auto* nt = reinterpret_cast<IMAGE_NT_HEADERS64*>(game::Base() + dos->e_lfanew);
            return p >= game::Base() && p < game::Base() + nt->OptionalHeader.SizeOfImage;
        }

        void WriteBytes(std::uintptr_t at, const void* src, std::size_t n)
        {
            DWORD old;
            VirtualProtect(reinterpret_cast<void*>(at), n, PAGE_EXECUTE_READWRITE, &old);
            std::memcpy(reinterpret_cast<void*>(at), src, n);
            VirtualProtect(reinterpret_cast<void*>(at), n, old, &old);
            FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(at), n);
        }

        // 14-byte absolute jump placed within rel32 reach of the game's code.
        void* NearJump(void* target)
        {
            SYSTEM_INFO si;
            GetSystemInfo(&si);
            std::uintptr_t gran = si.dwAllocationGranularity;
            for (std::uintptr_t p = (game::Base() - 0x10000) & ~(gran - 1); p > game::Base() - 0x7FF00000; p -= gran) {
                void* mem = VirtualAlloc(reinterpret_cast<void*>(p), 0x1000, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
                if (!mem) continue;
                unsigned char stub[14] = { 0xFF, 0x25, 0, 0, 0, 0 };  // jmp [rip+0]
                std::memcpy(stub + 6, &target, 8);
                std::memcpy(mem, stub, sizeof(stub));
                return mem;
            }
            return nullptr;
        }

        std::uintptr_t CallTarget(std::uintptr_t site)
        {
            return site + 5 + *reinterpret_cast<std::int32_t*>(site + 1);
        }

        // Finds the call to process_hit inside melee_hit_caller: first at the sheet's hint, then by
        // scanning the function body, so a shifted call in a newer build is still found.
        std::uintptr_t FindHitCallSite()
        {
            std::uintptr_t fn = game::Addr(addr::melee_hit_caller);
            std::uintptr_t callee = game::Addr(addr::process_hit);
            std::uintptr_t hint = fn + addr::kRows[addr::melee_hit_caller].callOffsetHint;
            if (*reinterpret_cast<unsigned char*>(hint) == 0xE8 && CallTarget(hint) == callee) return hint;
            for (std::uintptr_t p = fn; p < fn + 0x1000; ++p) {
                if (*reinterpret_cast<unsigned char*>(p) == 0xE8 && CallTarget(p) == callee) return p;
            }
            return 0;
        }

        bool PatchVFunc(addr::Id vt, void* hook, void** orig)
        {
            auto** table = reinterpret_cast<void**>(game::Addr(vt));
            int index = addr::kRows[vt].vfuncIndex;
            if (!InExeText(reinterpret_cast<std::uintptr_t>(table[index]))) return false;
            *orig = table[index];
            WriteBytes(reinterpret_cast<std::uintptr_t>(&table[index]), &hook, sizeof(void*));
            return true;
        }
    }

    void QueueNotification(const std::string& text)
    {
        std::lock_guard lk(g_msgLock);
        g_messages.push_back(text);
    }

    bool Install(std::string& whyNot)
    {
        // Sanity: the live player object's vtable must be the one the sheet names.
        game::Actor* player = game::Player();
        if (!player || game::VTable(player) != reinterpret_cast<void**>(game::Addr(addr::vtbl_player_character))) {
            whyNot = "player object does not match vtbl_player_character";
            return false;
        }
        std::uintptr_t site = FindHitCallSite();
        if (!site) {
            whyNot = "call to process_hit not found in melee_hit_caller";
            return false;
        }
        void* jump = NearJump(reinterpret_cast<void*>(&HitHook));
        if (!jump) {
            whyNot = "no memory near the game for the hit trampoline";
            return false;
        }
        if (!PatchVFunc(addr::vtbl_look_handler, reinterpret_cast<void*>(&MouseMoveHook), reinterpret_cast<void**>(&g_origMouseMove))) {
            whyNot = "LookHandler vtable entry is not game code";
            return false;
        }
        if (!PatchVFunc(addr::vtbl_player_character, reinterpret_cast<void*>(&UpdateHook), reinterpret_cast<void**>(&g_origUpdate))) {
            whyNot = "PlayerCharacter::Update vtable entry is not game code";
            return false;
        }
        g_origProcessHit = reinterpret_cast<ProcessHit_t>(CallTarget(site));
        unsigned char call[5] = { 0xE8 };
        std::int32_t rel = static_cast<std::int32_t>(reinterpret_cast<std::uintptr_t>(jump) - (site + 5));
        std::memcpy(call + 1, &rel, 4);
        WriteBytes(site, call, sizeof(call));
        fhd::Log("hooks: hit call at +0x%llX, look handler and player update patched",
            static_cast<unsigned long long>(site - game::Addr(addr::melee_hit_caller)));
        return true;
    }
}

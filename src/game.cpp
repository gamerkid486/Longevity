#include "game.h"

#include "log.h"

#include <windows.h>

#include <cmath>
#include <cstdio>
#include <unordered_map>

namespace game {
    namespace {
        std::uintptr_t g_base = 0;
        const addr::VersionTable* g_table = nullptr;

        bool ExeVersion(std::uint16_t v[4])
        {
            wchar_t path[MAX_PATH]{};
            GetModuleFileNameW(nullptr, path, MAX_PATH);
            DWORD dummy = 0;
            DWORD size = GetFileVersionInfoSizeW(path, &dummy);
            if (!size) return false;
            std::string buf(size, '\0');
            if (!GetFileVersionInfoW(path, 0, size, buf.data())) return false;
            VS_FIXEDFILEINFO* info = nullptr;
            UINT len = 0;
            if (!VerQueryValueW(buf.data(), L"\\", reinterpret_cast<void**>(&info), &len) || !info) return false;
            v[0] = HIWORD(info->dwFileVersionMS);
            v[1] = LOWORD(info->dwFileVersionMS);
            v[2] = HIWORD(info->dwFileVersionLS);
            v[3] = LOWORD(info->dwFileVersionLS);
            return true;
        }

        // A BSFixedString is one pointer into the game's string pool. Built once, never released.
        struct FixedString { void* entry = nullptr; };

        const FixedString* Intern(const char* s)
        {
            static std::unordered_map<std::string, FixedString> cache;
            auto it = cache.find(s);
            if (it != cache.end()) return &it->second;
            auto& fs = cache[s];
            using ctor_t = void* (*)(FixedString*, const char*);
            reinterpret_cast<ctor_t>(Addr(addr::fixedstring_ctor))(&fs, s);
            return &fs;
        }
    }

    bool Init(std::string& versionOut)
    {
        g_base = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
        std::uint16_t v[4]{};
        if (!ExeVersion(v)) {
            versionOut = "unknown";
            return false;
        }
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%u.%u.%u.%u", v[0], v[1], v[2], v[3]);
        versionOut = buf;
        for (const auto& t : addr::kVersions) {
            if (t.v[0] == v[0] && t.v[1] == v[1] && t.v[2] == v[2] && t.v[3] == v[3]) {
                g_table = &t;
                return true;
            }
        }
        return false;
    }

    std::uintptr_t Base() { return g_base; }

    std::uintptr_t Addr(addr::Id id) { return g_base + g_table->off[id]; }

    Actor* Player() { return *reinterpret_cast<Actor**>(Addr(addr::player_singleton)); }

    Actor* LookupActor(std::uint32_t handle)
    {
        if (!handle) return nullptr;
        using fn_t = bool (*)(const std::uint32_t*, Actor**);
        Actor* out = nullptr;
        reinterpret_cast<fn_t>(Addr(addr::get_smart_pointer))(&handle, &out);
        return out;
    }

    void Release(Actor* a)
    {
        if (!a) return;
        // NiPointer release on the BSHandleRefObject subobject: refcount in the low 10 bits.
        void* refObj = reinterpret_cast<char*>(a) + lay::refr_handle_refobj;
        auto* count = reinterpret_cast<volatile LONG*>(reinterpret_cast<char*>(refObj) + 8);
        if ((InterlockedDecrement(count) & 0x3FF) == 0) {
            using del_t = void (*)(void*);
            reinterpret_cast<del_t>(VTable(refObj)[1])(refObj);
        }
    }

    bool IsBlocking(Actor* a)
    {
        using fn_t = bool (*)(const Actor*);
        return reinterpret_cast<fn_t>(Addr(addr::actor_is_blocking))(a);
    }

    bool IsAttacking(Actor* a)
    {
        using fn_t = bool (*)(const Actor*);
        return reinterpret_cast<fn_t>(Addr(addr::actor_is_attacking))(a);
    }

    bool IsDead(Actor* a)
    {
        using fn_t = bool (*)(const Actor*, bool);
        return reinterpret_cast<fn_t>(VTable(a)[lay::refr_vfunc_is_dead])(a, true);
    }

    bool IsMeleeWeapon(const void* form)
    {
        if (!form || Field<std::uint8_t>(form, lay::form_type) != lay::weap_form_type_value) return false;
        std::uint8_t anim = Field<std::uint8_t>(form, lay::weap_anim_type);
        return anim >= 1 && anim <= 6;  // swords, daggers, axes, maces, greatswords, battleaxes/warhammers
    }

    // ActorState::actorState2.weaponState (0 sheathed ... 3 drawn, 4-5 sheathing). -1 when the ActorState
    // subobject doesn't start with a vtable in SkyrimSE.exe, i.e. the layout row is wrong for this build.
    int WeaponState(Actor* a)
    {
        const void* st = reinterpret_cast<const char*>(a) + lay::actor_state;
        auto vt = reinterpret_cast<std::uintptr_t>(VTable(st));
        auto* nt = reinterpret_cast<IMAGE_NT_HEADERS64*>(g_base + reinterpret_cast<IMAGE_DOS_HEADER*>(g_base)->e_lfanew);
        if (vt < g_base || vt >= g_base + nt->OptionalHeader.SizeOfImage) {
            static bool logged = false;
            if (!logged) fhd::Log("ActorState at +0x%llX has no game vtable; weapon drawn check is off", static_cast<unsigned long long>(lay::actor_state));
            logged = true;
            return -1;
        }
        return (Field<std::uint32_t>(st, lay::actor_state2) >> 5) & 7;
    }

    bool IsDuelist(Actor* a)
    {
        if (!a) return false;
        void* process = Field<void*>(a, lay::actor_runtime + lay::actor_process);
        if (!process || !IsMeleeWeapon(Field<void*>(process, lay::process_right_hand))) return false;
        int ws = WeaponState(a);
        return ws < 0 || ws >= 3;  // drawn, or can't tell
    }

    std::string DescribeDuelist(Actor* a)
    {
        if (!a) return "no actor";
        void* process = Field<void*>(a, lay::actor_runtime + lay::actor_process);
        if (!process) return "no AIProcess";
        const void* form = Field<void*>(process, lay::process_right_hand);
        if (!form) return "right hand empty";
        char buf[96];
        std::uint8_t type = Field<std::uint8_t>(form, lay::form_type);
        std::snprintf(buf, sizeof(buf), "right hand form %08X type 0x%02X anim %u, weapon state %d", Field<std::uint32_t>(form, lay::form_id),
            type, type == lay::weap_form_type_value ? Field<std::uint8_t>(form, lay::weap_anim_type) : 0u, WeaponState(a));
        return buf;
    }

    std::uint32_t FormID(Actor* a) { return Field<std::uint32_t>(a, lay::form_id); }

    float DistanceSq(Actor* a, Actor* b)
    {
        const float* p = &Field<float>(a, lay::refr_location);
        const float* q = &Field<float>(b, lay::refr_location);
        float dx = p[0] - q[0], dy = p[1] - q[1], dz = p[2] - q[2];
        return dx * dx + dy * dy + dz * dz;
    }

    float YawErrorDeg(Actor* a, Actor* b)
    {
        const float* p = &Field<float>(a, lay::refr_location);
        const float* q = &Field<float>(b, lay::refr_location);
        // Skyrim headings: 0 = north (+Y), growing clockwise toward east (+X).
        float target = std::atan2(q[0] - p[0], q[1] - p[1]);
        float err = std::remainder(target - Field<float>(a, lay::refr_angle_z), 2.0f * 3.14159265f);
        return err * 180.0f / 3.14159265f;
    }

    float HeadingDeg(Actor* a) { return Field<float>(a, lay::refr_angle_z) * 180.0f / 3.14159265f; }

    std::uint32_t CombatTargetHandle(Actor* a)
    {
        return Field<std::uint32_t>(a, lay::actor_runtime + lay::actor_combat_target);
    }

    bool NotifyAnimation(Actor* a, const char* event)
    {
        void* holder = reinterpret_cast<char*>(a) + lay::refr_anim_holder;
        using fn_t = bool (*)(void*, const FixedString*);
        return reinterpret_cast<fn_t>(VTable(holder)[1])(holder, Intern(event));
    }

    void Notification(const char* text)
    {
        using fn_t = void (*)(const char*, const char*, bool);
        reinterpret_cast<fn_t>(Addr(addr::debug_notification))(text, nullptr, true);
    }

    HandleArray* HighActorHandles()
    {
        void* lists = *reinterpret_cast<void**>(Addr(addr::process_lists));
        if (!lists) return nullptr;
        return reinterpret_cast<HandleArray*>(reinterpret_cast<char*>(lists) + lay::lists_high_actors);
    }
}

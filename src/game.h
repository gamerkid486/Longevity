#pragma once
// Thin, version-checked access to Skyrim AE internals. Every address and field offset comes from
// src/generated/sheets.h (sheets/game_addresses.json, sheets/layouts.json).
#include "generated/sheets.h"

#include <cstdint>
#include <string>

namespace game {
    using Actor = void;  // opaque game object; fields are read through lay:: offsets

    // Detects the running SkyrimSE.exe version and picks the matching offset table.
    bool Init(std::string& versionOut);
    std::uintptr_t Base();
    std::uintptr_t Addr(addr::Id id);  // absolute address of a sheet row

    template <class T>
    inline T& Field(const void* obj, std::ptrdiff_t off)
    {
        return *reinterpret_cast<T*>(reinterpret_cast<std::uintptr_t>(obj) + off);
    }
    inline void** VTable(const void* obj) { return *reinterpret_cast<void** const*>(obj); }

    Actor* Player();
    // Resolves an actor handle; the returned actor must be passed to Release().
    Actor* LookupActor(std::uint32_t handle);
    void Release(Actor* a);

    bool IsBlocking(Actor* a);
    bool IsAttacking(Actor* a);
    bool IsDead(Actor* a);
    bool IsMeleeWeapon(const void* form);  // TESObjectWEAP with a melee animation type
    bool IsDuelist(Actor* a);  // has a melee weapon in the right hand, drawn
    std::string DescribeDuelist(Actor* a);  // the IsDuelist chain, for the log
    std::uint32_t FormID(Actor* a);
    float DistanceSq(Actor* a, Actor* b);
    std::uint32_t CombatTargetHandle(Actor* a);

    // Main thread only.
    bool NotifyAnimation(Actor* a, const char* event);  // false when the behavior graph rejects the event
    void Notification(const char* text);

    // ProcessLists::highActorHandles (main thread).
    struct HandleArray { std::uint32_t* data; std::uint32_t capacity; std::uint32_t pad; std::uint32_t size; };
    HandleArray* HighActorHandles();
}

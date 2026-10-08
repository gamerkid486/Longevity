#pragma once
// FOR HONOR duel state: the player's guard, every NPC duelist's guard, and the rules from
// sheets/rules.json applied when a melee hit lands.
#include "game.h"

#include <atomic>

namespace duel {
    // What the overlay draws; written on the main thread, read on the render thread.
    struct Snapshot {
        bool playerDuelist = false;
        bool playerGuarding = false;
        Guard playerGuard = Guard::top;
        bool hasOpponent = false;
        Guard opponentGuard = Guard::top;
        bool opponentAttacking = false;
    };
    Snapshot Read();

    // Main thread, once per frame (PlayerCharacter::Update hook).
    void Tick();
    // From the DirectInput mouse hook (input.cpp), with the mouse movement since the last read.
    void OnMouseMove(long dx, long dy);
    // True while the camera must stay still (the player is guarding).
    bool CameraHeld();
    // From the melee hit hook (weapon = HitData.weapon). Returns true when the hit must be cancelled.
    bool OnMeleeHit(game::Actor* victim, game::Actor* aggressor, const void* weapon);
}

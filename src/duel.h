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
    // Main thread, from LookHandler::ProcessMouseMove. Returns true when the mouse move is consumed
    // as a guard flick (camera stays still).
    bool OnMouseMove(int dx, int dy);
    // From the melee hit hook (weapon = HitData.weapon). Returns true when the hit must be cancelled.
    bool OnMeleeHit(game::Actor* victim, game::Actor* aggressor, const void* weapon);
}

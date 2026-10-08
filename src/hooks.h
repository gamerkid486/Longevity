#pragma once
#include <string>

namespace hooks {
    // Installs the melee-hit and player-update hooks. Returns false (and logs why) when
    // the game's code does not look like the sheet says, in which case nothing is patched.
    bool Install(std::string& whyNot);
    // Messages shown in-game from the main thread on the next frame.
    void QueueNotification(const std::string& text);
}

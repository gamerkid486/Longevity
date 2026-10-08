#pragma once
#include "generated/sheets.h"

#include <string>
#include <vector>

namespace audio {
    // Loads the WAVs prepared from FOR HONOR (one or more per sound row). Missing files are skipped.
    void Load(Snd id, const std::vector<std::wstring>& wavPaths);
    // Plays one of the sound row's variations (any thread). No-op if nothing was loaded.
    void Play(Snd id);
    bool Ready(Snd id);
}

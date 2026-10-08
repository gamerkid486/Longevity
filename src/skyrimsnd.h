#pragma once
#include "generated/sheets.h"

#include <string>

namespace skyrimsnd {
    // Fallback for sound rows FOR HONOR couldn't supply: finds files in the game's own
    // "Skyrim - Sounds.bsa" whose path contains every '|'-separated part of the row's skyrim_match,
    // converts up to kMaxVariations of them to WAV in the cache and loads them into audio.
    // Read-only; nothing in the game folder changes. Returns how many WAVs were prepared.
    int Load(const SoundRow& row, const std::wstring& vgmstreamExe, const std::wstring& cacheDir);
    // Same, from a given archive (used by Load and the self-test).
    int LoadFrom(const std::wstring& archive, const SoundRow& row, const std::wstring& vgmstreamExe, const std::wstring& cacheDir);
}

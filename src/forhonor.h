#pragma once
#include <cstdint>
#include <string>

namespace forhonor {
    // Finds the player's FOR HONOR install (env FHDUELS_FORHONOR_DIR, Steam libraries, Ubisoft Connect).
    std::wstring FindInstall();
    // Reads the sounds named in sheets/sounds.json from FOR HONOR's Wwise .pck files, converts them to
    // WAV with the bundled vgmstream-cli and loads them into audio. Returns a player-facing status
    // line, or "" when everything is ready. With FHDUELS_DUMP_INDEX=1 it also writes every sound's ID
    // to %LOCALAPPDATA%/FHDuels/index.csv so the sheet's wem_ids can be chosen.
    std::string PrepareSounds();

    // Building blocks (also used by tests and the sound-picking dump).
    std::size_t IndexInstall(const std::wstring& rootDir);  // every .pck under rootDir
    bool WriteIndexCsv(const std::wstring& path);
    // Runs the bundled vgmstream-cli on one file (any format it reads) and writes a PCM WAV.
    bool ConvertToWav(const std::wstring& vgmstreamExe, const std::wstring& in, const std::wstring& wavOut);
    bool ExtractWav(std::uint32_t wemId, const char* pckFilter, const std::wstring& vgmstreamExe, const std::wstring& wavOut);
}

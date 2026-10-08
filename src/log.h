#pragma once
#include <string>

namespace fhd {
    // Log file: Documents/My Games/Skyrim Special Edition/FHDuels.log
    void LogInit();
    void Log(const char* fmt, ...);
    std::wstring MyGamesDir();     // .../My Games/Skyrim Special Edition
    std::wstring CacheDir();       // %LOCALAPPDATA%/FHDuels
    std::wstring PluginDir();      // folder holding FHDuels.asi
}

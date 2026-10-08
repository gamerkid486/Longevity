// Loads FHDuels.asi into a process that is not Skyrim: the plugin must log "not supported" and stay off.
#include <windows.h>
#include <cstdio>
int main(int argc, char** argv)
{
    HMODULE m = LoadLibraryA(argc > 1 ? argv[1] : "FHDuels.asi");
    std::printf("LoadLibrary: %s\n", m ? "ok" : "failed");
    Sleep(1500);
    return m ? 0 : 1;
}

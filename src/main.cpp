// FOR HONOR Duels - ASI plugin for Skyrim Special Edition (loaded by Ultimate ASI Loader).
#include "forhonor.h"
#include "game.h"
#include "hooks.h"
#include "input.h"
#include "log.h"
#include "overlay.h"

#include <windows.h>

#include <string>

namespace {
    DWORD WINAPI Start(LPVOID)
    {
        fhd::LogInit();
        std::string version;
        if (!game::Init(version)) {
            fhd::Log("SkyrimSE.exe %s is not supported; FOR HONOR Duels stays off.", version.c_str());
            return 0;
        }
        fhd::Log("SkyrimSE.exe %s supported", version.c_str());

        // Wait until the game has created the player (code unpacked, singletons alive).
        while (!game::Player()) Sleep(250);
        Sleep(500);

        std::string why;
        if (!hooks::Install(why)) {
            fhd::Log("hooks not installed: %s. FOR HONOR Duels stays off.", why.c_str());
            return 0;
        }
        if (!input::Install()) fhd::Log("mouse hooks unavailable: guard flicks are off");
        if (!overlay::Install()) fhd::Log("guard indicators unavailable");

        std::string status = forhonor::PrepareSounds();
        hooks::QueueNotification(status.empty() ? "FOR HONOR Duels: hold block and flick the mouse to change guard." : status);
        return 0;
    }
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        if (HANDLE t = CreateThread(nullptr, 0, Start, nullptr, 0, nullptr)) CloseHandle(t);
    }
    return TRUE;
}

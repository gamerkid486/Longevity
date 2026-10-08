#include "log.h"

#include <windows.h>
#include <shlobj.h>

#include <cstdarg>
#include <cstdio>
#include <mutex>

namespace fhd {
    namespace {
        std::mutex g_lock;
        FILE* g_file = nullptr;

        std::wstring KnownFolder(REFKNOWNFOLDERID id)
        {
            PWSTR path = nullptr;
            std::wstring out;
            if (SUCCEEDED(SHGetKnownFolderPath(id, 0, nullptr, &path))) {
                out = path;
            }
            CoTaskMemFree(path);
            return out;
        }
    }

    std::wstring MyGamesDir() { return KnownFolder(FOLDERID_Documents) + L"\\My Games\\Skyrim Special Edition"; }

    std::wstring CacheDir() { return KnownFolder(FOLDERID_LocalAppData) + L"\\FHDuels"; }

    std::wstring PluginDir()
    {
        HMODULE self = nullptr;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&PluginDir), &self);
        wchar_t buf[MAX_PATH]{};
        GetModuleFileNameW(self, buf, MAX_PATH);
        std::wstring p = buf;
        return p.substr(0, p.find_last_of(L"\\/"));
    }

    void LogInit()
    {
        std::wstring dir = MyGamesDir();
        SHCreateDirectoryExW(nullptr, dir.c_str(), nullptr);
        g_file = _wfopen((dir + L"\\FHDuels.log").c_str(), L"w");
    }

    void Log(const char* fmt, ...)
    {
        std::lock_guard lk(g_lock);
        if (!g_file) return;
        SYSTEMTIME t;
        GetLocalTime(&t);
        std::fprintf(g_file, "[%02d:%02d:%02d.%03d] ", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);
        va_list ap;
        va_start(ap, fmt);
        std::vfprintf(g_file, fmt, ap);
        va_end(ap);
        std::fputc('\n', g_file);
        std::fflush(g_file);
    }
}

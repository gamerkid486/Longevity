#include "forhonor.h"

#include "audio.h"
#include "log.h"

#include <windows.h>
#include <shlobj.h>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <regex>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;

namespace forhonor {
    namespace {
        constexpr int kSteamAppId = 304390;

        std::wstring RegString(HKEY root, const wchar_t* key, const wchar_t* value)
        {
            wchar_t buf[1024]{};
            DWORD size = sizeof(buf);
            if (RegGetValueW(root, key, value, RRF_RT_REG_SZ, nullptr, buf, &size) != ERROR_SUCCESS) return L"";
            return buf;
        }

        fs::path Utf8Path(const std::string& s)
        {
            return fs::path(std::u8string(s.begin(), s.end()));
        }

        std::string ReadText(const fs::path& p)
        {
            std::ifstream f(p, std::ios::binary);
            return { std::istreambuf_iterator<char>(f), {} };
        }

        bool LooksLikeForHonor(const fs::path& dir)
        {
            std::error_code ec;
            return fs::exists(dir / L"ForHonor.exe", ec) || fs::exists(dir / L"ForHonor_vulkan.exe", ec);
        }

        std::wstring FromSteam()
        {
            std::wstring steam = RegString(HKEY_CURRENT_USER, L"Software\\Valve\\Steam", L"SteamPath");
            if (steam.empty()) return L"";
            std::vector<fs::path> libs{ fs::path(steam) };
            std::string vdf = ReadText(fs::path(steam) / L"steamapps" / L"libraryfolders.vdf");
            std::regex pathRe("\"path\"\\s+\"([^\"]+)\"");
            for (std::sregex_iterator it(vdf.begin(), vdf.end(), pathRe), end; it != end; ++it) {
                std::string p = (*it)[1];
                p = std::regex_replace(p, std::regex("\\\\\\\\"), "\\");
                libs.emplace_back(Utf8Path(p));
            }
            std::regex dirRe("\"installdir\"\\s+\"([^\"]+)\"");
            for (const auto& lib : libs) {
                std::string acf = ReadText(lib / L"steamapps" / ("appmanifest_" + std::to_string(kSteamAppId) + ".acf"));
                std::smatch m;
                if (std::regex_search(acf, m, dirRe)) {
                    fs::path dir = lib / L"steamapps" / L"common" / Utf8Path(m[1].str());
                    if (fs::is_directory(dir)) return dir.wstring();
                }
            }
            return L"";
        }

        std::wstring FromUbisoft()
        {
            HKEY installs;
            if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\WOW6432Node\\Ubisoft\\Launcher\\Installs", 0, KEY_READ, &installs) != ERROR_SUCCESS) return L"";
            std::wstring found;
            wchar_t name[256];
            for (DWORD i = 0; found.empty(); ++i) {
                DWORD len = 256;
                if (RegEnumKeyExW(installs, i, name, &len, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS) break;
                std::wstring dir = RegString(installs, name, L"InstallDir");
                if (!dir.empty() && LooksLikeForHonor(dir)) found = dir;
            }
            RegCloseKey(installs);
            return found;
        }

        // --- Wwise AKPK (.pck) and bank (.bnk) reading ---------------------------------------

        struct Entry {
            fs::path pck;
            std::uint64_t offset;
            std::uint32_t size;
            std::uint32_t bankId;  // 0 = streamed file in the pack's sound table
        };

        template <class T>
        bool Get(const std::vector<char>& d, std::size_t at, T& out)
        {
            if (at + sizeof(T) > d.size()) return false;
            std::memcpy(&out, d.data() + at, sizeof(T));
            return true;
        }

        std::vector<char> ReadRange(const fs::path& p, std::uint64_t off, std::uint32_t size)
        {
            std::ifstream f(p, std::ios::binary);
            std::vector<char> d(size);
            f.seekg(static_cast<std::streamoff>(off));
            f.read(d.data(), size);
            d.resize(static_cast<std::size_t>(f.gcount()));
            return d;
        }

        // DIDX/DATA chunks of a bank stored at bankOff inside the pack: embedded sounds.
        void IndexBank(const fs::path& pck, std::uint64_t bankOff, std::uint32_t bankSize, std::uint32_t bankId,
            std::unordered_map<std::uint32_t, Entry>& out)
        {
            std::vector<char> b = ReadRange(pck, bankOff, bankSize);
            std::size_t didx = 0, didxLen = 0, data = 0;
            for (std::size_t p = 0; p + 8 <= b.size();) {
                std::uint32_t len = 0;
                Get(b, p + 4, len);
                if (!std::memcmp(b.data() + p, "DIDX", 4)) { didx = p + 8; didxLen = len; }
                if (!std::memcmp(b.data() + p, "DATA", 4)) data = p + 8;
                p += 8 + std::size_t(len);
            }
            if (!didx || !data) return;
            for (std::size_t e = didx; e + 12 <= didx + didxLen; e += 12) {
                std::uint32_t id = 0, off = 0, size = 0;
                Get(b, e, id), Get(b, e + 4, off), Get(b, e + 8, size);
                out.try_emplace(id, Entry{ pck, bankOff + data + off, size, bankId });
            }
        }

        bool IndexPck(const fs::path& pck, std::unordered_map<std::uint32_t, Entry>& out)
        {
            std::vector<char> h = ReadRange(pck, 0, 8);
            std::uint32_t headerSize = 0;
            if (h.size() < 8 || std::memcmp(h.data(), "AKPK", 4) || !Get(h, 4, headerSize)) {
                fhd::Log("forhonor: %s is not a plain AKPK pack (encrypted or another format)", pck.filename().string().c_str());
                return false;
            }
            std::vector<char> d = ReadRange(pck, 0, headerSize + 8);
            std::uint32_t ver = 0, langSize = 0, bankSize = 0, soundSize = 0, extSize = 0;
            Get(d, 8, ver), Get(d, 12, langSize), Get(d, 16, bankSize), Get(d, 20, soundSize);
            // headerSize counts from offset 8: four u32 sizes (+ an externals size in newer packs) + tables.
            std::size_t tables = 24;
            if (headerSize != 16 + langSize + bankSize + soundSize && Get(d, 24, extSize) &&
                headerSize == 20 + langSize + bankSize + soundSize + extSize) {
                tables = 28;
            }
            std::size_t banks = tables + langSize, sounds = banks + bankSize;

            auto table = [&](std::size_t at, bool isBank) {
                std::uint32_t count = 0;
                Get(d, at, count);
                for (std::uint32_t i = 0; i < count; ++i) {
                    std::size_t e = at + 4 + std::size_t(i) * 20;
                    std::uint32_t id = 0, block = 0, size = 0, start = 0;
                    if (!Get(d, e, id) || !Get(d, e + 4, block) || !Get(d, e + 8, size) || !Get(d, e + 12, start)) return;
                    std::uint64_t off = std::uint64_t(start) * (block ? block : 1);
                    if (isBank) IndexBank(pck, off, size, id, out);
                    else out.try_emplace(id, Entry{ pck, off, size, 0 });
                }
            };
            table(banks, true);
            table(sounds, false);
            return true;
        }

        bool RunVgmstream(const fs::path& exe, const fs::path& in, const fs::path& out)
        {
            std::wstring cmd = L"\"" + exe.wstring() + L"\" -o \"" + out.wstring() + L"\" \"" + in.wstring() + L"\"";
            STARTUPINFOW si{ sizeof(si) };
            PROCESS_INFORMATION pi{};
            if (!CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) return false;
            WaitForSingleObject(pi.hProcess, 15000);
            DWORD code = 1;
            GetExitCodeProcess(pi.hProcess, &code);
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
            std::error_code ec;
            return code == 0 && fs::exists(out, ec);
        }
    }

    std::wstring FindInstall()
    {
        if (const wchar_t* env = _wgetenv(L"FHDUELS_FORHONOR_DIR"); env && *env && fs::is_directory(env)) return env;
        std::wstring dir = FromSteam();
        if (dir.empty()) dir = FromUbisoft();
        return dir;
    }

    namespace {
        std::unordered_map<std::uint32_t, Entry> g_index;
    }

    std::size_t IndexInstall(const std::wstring& rootDir)
    {
        g_index.clear();
        std::error_code ec;
        int packs = 0;
        fs::path root = rootDir;
        for (auto it = fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied, ec);
             it != fs::recursive_directory_iterator(); it.increment(ec)) {
            if (it.depth() > 5) it.disable_recursion_pending();
            if (it->is_regular_file(ec) && it->path().extension() == L".pck" && IndexPck(it->path(), g_index)) ++packs;
        }
        fhd::Log("forhonor: %d sound pack(s), %zu sounds indexed", packs, g_index.size());
        return g_index.size();
    }

    bool WriteIndexCsv(const std::wstring& path)
    {
        std::ofstream csv{ fs::path(path) };
        csv << "wem_id,pck,bank_id,size,offset\n";
        for (const auto& [id, e] : g_index) {
            csv << id << ',' << e.pck.filename().string() << ',' << e.bankId << ',' << e.size << ',' << e.offset << '\n';
        }
        return bool(csv);
    }

    bool ExtractWav(std::uint32_t id, const char* pck, const std::wstring& vgmstreamExe, const std::wstring& wavOut)
    {
        auto e = g_index.find(id);
        if (e == g_index.end() || (pck && *pck && e->second.pck.filename().string() != pck)) return false;
        std::error_code ec;
        fs::path raw = fs::path(wavOut).parent_path() / L"raw" / (std::to_wstring(id) + L".wem");
        fs::create_directories(raw.parent_path(), ec);
        std::vector<char> bytes = ReadRange(e->second.pck, e->second.offset, e->second.size);
        std::ofstream(raw, std::ios::binary).write(bytes.data(), std::streamsize(bytes.size()));
        bool ok = RunVgmstream(vgmstreamExe, raw, wavOut);
        fs::remove(raw, ec);  // keep only the converted WAV, on this PC only
        return ok;
    }

    std::string PrepareSounds()
    {
        std::wstring root = FindInstall();
        if (root.empty()) {
            fhd::Log("forhonor: install not found (Steam app %d, Ubisoft Connect, FHDUELS_FORHONOR_DIR)", kSteamAppId);
            return "FOR HONOR Duels: FOR HONOR is not installed, so its sounds are silent.";
        }
        fhd::Log("forhonor: install at %s", fs::path(root).string().c_str());
        IndexInstall(root);

        std::error_code ec;
        fs::path cache = fhd::CacheDir();
        fs::create_directories(cache, ec);
        if (const wchar_t* dump = _wgetenv(L"FHDUELS_DUMP_INDEX"); dump && *dump == L'1' && WriteIndexCsv(cache / L"index.csv")) {
            fhd::Log("forhonor: wrote index.csv");
        }

        fs::path vgm = fs::path(fhd::PluginDir()) / L"FHDuels" / L"vgmstream" / L"vgmstream-cli.exe";
        int ready = 0, wanted = 0;
        for (const auto& row : kSounds) {
            std::vector<std::wstring> wavs;
            for (int i = 0; i < row.wemCount; ++i) {
                ++wanted;
                std::uint32_t id = row.wemIds[i];
                std::string name = row.cacheFile;
                name.replace(name.find("{id}"), 4, std::to_string(id));
                fs::path wav = cache / name;
                if (!fs::exists(wav, ec) && !ExtractWav(id, row.pck, vgm, wav)) {
                    fhd::Log("forhonor: sound %u for %s could not be read from this FOR HONOR install", id, row.name);
                    continue;
                }
                wavs.push_back(wav.wstring());
            }
            audio::Load(row.id, wavs);
            if (audio::Ready(row.id)) ready += int(wavs.size());
        }
        fhd::Log("forhonor: %d of %d sounds ready", ready, wanted);
        if (wanted == 0) return "FOR HONOR Duels: this build has no FOR HONOR sounds chosen yet.";
        if (ready == 0) return "FOR HONOR Duels: FOR HONOR's sounds could not be read from your install.";
        return "";
    }
}

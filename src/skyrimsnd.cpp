#include "skyrimsnd.h"

#include "audio.h"
#include "forhonor.h"
#include "log.h"

#include <windows.h>

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

namespace fs = std::filesystem;

namespace skyrimsnd {
    namespace {
        constexpr int kMaxVariations = 6;

        struct File {
            std::string path;  // lowercase, '/' separators
            std::uint64_t offset;
            std::uint32_t size;
            bool compressed;
        };

        template <class T>
        bool Read(std::ifstream& f, T& v) { return bool(f.read(reinterpret_cast<char*>(&v), sizeof(v))); }

        std::string Normalize(std::string s)
        {
            for (char& c : s) c = c == '\\' ? '/' : char(std::tolower(static_cast<unsigned char>(c)));
            return s;
        }

        // Skyrim SE archive (BSA version 105): header, folder records, per-folder name + file records,
        // then the file name block.
        bool List(const fs::path& bsa, std::vector<File>& out, std::uint32_t& flags)
        {
            std::ifstream f(bsa, std::ios::binary);
            char magic[4]{};
            std::uint32_t version = 0, folderOffset = 0, folders = 0, files = 0, folderNames = 0, fileNames = 0;
            std::uint16_t fileFlags = 0, pad = 0;
            if (!f.read(magic, 4) || std::memcmp(magic, "BSA\0", 4) != 0) return false;
            Read(f, version), Read(f, folderOffset), Read(f, flags), Read(f, folders), Read(f, files);
            Read(f, folderNames), Read(f, fileNames), Read(f, fileFlags), Read(f, pad);
            if (version != 105 || !(flags & 0x1) || !(flags & 0x2)) {
                fhd::Log("skyrim sounds: %s is BSA version %u flags 0x%X, not supported", bsa.filename().string().c_str(), version, flags);
                return false;
            }
            std::vector<std::uint32_t> counts(folders);
            f.seekg(folderOffset);
            for (auto& c : counts) {
                std::uint64_t hash = 0, offset = 0;
                std::uint32_t padding = 0;
                Read(f, hash), Read(f, c), Read(f, padding), Read(f, offset);
            }
            for (std::uint32_t c : counts) {
                std::uint8_t len = 0;
                Read(f, len);
                std::string dir(len, '\0');
                f.read(dir.data(), len);
                dir.resize(std::strlen(dir.c_str()));
                for (std::uint32_t i = 0; i < c; ++i) {
                    std::uint64_t hash = 0;
                    std::uint32_t size = 0, offset = 0;
                    Read(f, hash), Read(f, size), Read(f, offset);
                    bool compressed = ((flags & 0x4) != 0) != ((size & 0x40000000) != 0);
                    out.push_back({ Normalize(dir) + "/", offset, size & 0x3FFFFFFF, compressed });
                }
            }
            std::string names(fileNames, '\0');
            if (!f.read(names.data(), fileNames)) return false;
            std::size_t at = 0;
            for (auto& file : out) {
                std::size_t end = names.find('\0', at);
                if (end == std::string::npos) return false;
                file.path += Normalize(names.substr(at, end - at));
                at = end + 1;
            }
            return f.good();
        }

        bool Extract(const fs::path& bsa, std::uint32_t flags, const File& file, const fs::path& out)
        {
            if (file.compressed) return false;  // sound archives are stored uncompressed
            std::ifstream f(bsa, std::ios::binary);
            f.seekg(std::streamoff(file.offset));
            std::uint32_t size = file.size;
            if (flags & 0x100) {  // data starts with the file's full path
                std::uint8_t len = 0;
                Read(f, len);
                f.seekg(len, std::ios::cur);
                size -= std::uint32_t(len) + 1;
            }
            std::vector<char> data(size);
            if (!f.read(data.data(), size)) return false;
            std::ofstream(out, std::ios::binary).write(data.data(), std::streamsize(size));
            return true;
        }

        bool Matches(const std::string& path, const std::string& match)
        {
            std::size_t at = 0;
            while (at <= match.size()) {
                std::size_t end = match.find('|', at);
                if (end == std::string::npos) end = match.size();
                std::string part = match.substr(at, end - at);
                if (!part.empty() && path.find(part) == std::string::npos) return false;
                at = end + 1;
            }
            return true;
        }
    }

    int Load(const SoundRow& row, const std::wstring& vgmstreamExe, const std::wstring& cacheDir)
    {
        wchar_t exe[MAX_PATH]{};
        GetModuleFileNameW(nullptr, exe, MAX_PATH);
        return LoadFrom((fs::path(exe).parent_path() / L"Data" / L"Skyrim - Sounds.bsa").wstring(), row, vgmstreamExe, cacheDir);
    }

    int LoadFrom(const std::wstring& archive, const SoundRow& row, const std::wstring& vgmstreamExe, const std::wstring& cacheDir)
    {
        if (!row.skyrimMatch || !*row.skyrimMatch) return 0;
        fs::path bsa = archive;
        std::vector<File> files;
        std::uint32_t flags = 0;
        if (!List(bsa, files, flags)) {
            fhd::Log("skyrim sounds: could not read %s", bsa.string().c_str());
            return 0;
        }
        std::string match = Normalize(row.skyrimMatch);
        std::vector<const File*> hits;
        for (const auto& file : files) {
            bool audioFile = file.path.ends_with(".wav") || file.path.ends_with(".xwm");
            if (audioFile && !file.compressed && Matches(file.path, match)) hits.push_back(&file);
        }
        std::sort(hits.begin(), hits.end(), [](const File* a, const File* b) { return a->path < b->path; });
        fhd::Log("skyrim sounds: %zu file(s) in %zu match '%s' for %s", hits.size(), files.size(), row.skyrimMatch, row.name);
        if (hits.empty()) {  // list some near misses so the sheet's skyrim_match can be corrected
            int shown = 0;
            for (const auto& file : files) {
                if (file.path.find("block") != std::string::npos && shown++ < 20) fhd::Log("skyrim sounds: candidate %s", file.path.c_str());
            }
        }
        if (hits.size() > kMaxVariations) hits.resize(kMaxVariations);

        std::error_code ec;
        std::vector<std::wstring> wavs;
        for (std::size_t i = 0; i < hits.size(); ++i) {
            fs::path wav = fs::path(cacheDir) / (L"sk_" + fs::path(row.name).wstring() + L"_" + std::to_wstring(i) + L".wav");
            fhd::Log("skyrim sounds: %s <- %s", row.name, hits[i]->path.c_str());
            if (!fs::exists(wav, ec)) {
                fs::path raw = fs::path(cacheDir) / L"raw" / fs::path(hits[i]->path).filename();
                fs::create_directories(raw.parent_path(), ec);
                bool ok = Extract(bsa, flags, *hits[i], raw) && forhonor::ConvertToWav(vgmstreamExe, raw.wstring(), wav.wstring());
                fs::remove(raw, ec);
                if (!ok) {
                    fhd::Log("skyrim sounds: could not convert %s", hits[i]->path.c_str());
                    continue;
                }
            }
            wavs.push_back(wav.wstring());
        }
        audio::Load(row.id, wavs);
        return int(wavs.size());
    }
}

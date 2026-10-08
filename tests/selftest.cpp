// Off-game self-test for the sound paths: builds a synthetic Wwise AKPK pack (one sound embedded in
// a bank's DIDX/DATA, one streamed) and a synthetic Skyrim SE sound archive, reads and converts them
// with vgmstream-cli and loads them through the audio module. Run: wine64 build/selftest.exe <vgmstream-cli.exe>
#include "audio.h"
#include "forhonor.h"
#include "log.h"
#include "skyrimsnd.h"

#include <windows.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using Bytes = std::vector<char>;

static int g_fail = 0;
#define CHECK(c) do { if (c) std::printf("PASS %s\n", #c); else { std::printf("FAIL %s (line %d)\n", #c, __LINE__); ++g_fail; } } while (0)

static void Put32(Bytes& b, std::uint32_t v) { b.insert(b.end(), reinterpret_cast<char*>(&v), reinterpret_cast<char*>(&v) + 4); }
static void Put16(Bytes& b, std::uint16_t v) { b.insert(b.end(), reinterpret_cast<char*>(&v), reinterpret_cast<char*>(&v) + 2); }
static void Tag(Bytes& b, const char* t) { b.insert(b.end(), t, t + 4); }

static Bytes Wav(float hz)
{
    const int rate = 22050, n = rate / 10;
    Bytes pcm;
    for (int i = 0; i < n; ++i) Put16(pcm, std::uint16_t(std::int16_t(8000 * std::sin(6.2831853f * hz * i / rate))));
    Bytes w;
    Tag(w, "RIFF"); Put32(w, 36 + std::uint32_t(pcm.size())); Tag(w, "WAVE");
    Tag(w, "fmt "); Put32(w, 16); Put16(w, 1); Put16(w, 1); Put32(w, rate); Put32(w, rate * 2); Put16(w, 2); Put16(w, 16);
    Tag(w, "data"); Put32(w, std::uint32_t(pcm.size())); w.insert(w.end(), pcm.begin(), pcm.end());
    return w;
}

int main(int argc, char** argv)
{
    if (argc < 2) { std::printf("usage: selftest <vgmstream-cli.exe>\n"); return 2; }
    fhd::LogInit();
    fs::path root = fs::temp_directory_path() / "fhd_selftest" / "For Honor";
    fs::remove_all(root.parent_path());
    fs::create_directories(root / "sounddata");
    std::ofstream(root / "ForHonor.exe") << "stub";

    Bytes embedded = Wav(440), streamed = Wav(660);
    Bytes bank;
    Tag(bank, "BKHD"); Put32(bank, 8); Put32(bank, 0x8C); Put32(bank, 900);
    Tag(bank, "DIDX"); Put32(bank, 12); Put32(bank, 111); Put32(bank, 0); Put32(bank, std::uint32_t(embedded.size()));
    Tag(bank, "DATA"); Put32(bank, std::uint32_t(embedded.size())); bank.insert(bank.end(), embedded.begin(), embedded.end());

    const std::uint32_t lang = 4, banks = 4 + 20, sounds = 4 + 20;
    const std::uint32_t headerSize = 16 + lang + banks + sounds;
    std::uint32_t bankOff = 8 + headerSize;
    std::uint32_t streamOff = (bankOff + std::uint32_t(bank.size()) + 15) / 16 * 16;
    Bytes pck;
    Tag(pck, "AKPK"); Put32(pck, headerSize); Put32(pck, 1); Put32(pck, lang); Put32(pck, banks); Put32(pck, sounds);
    Put32(pck, 0);                                                                        // no languages
    Put32(pck, 1); Put32(pck, 900); Put32(pck, 1); Put32(pck, std::uint32_t(bank.size())); Put32(pck, bankOff); Put32(pck, 0);
    Put32(pck, 1); Put32(pck, 222); Put32(pck, 16); Put32(pck, std::uint32_t(streamed.size())); Put32(pck, streamOff / 16); Put32(pck, 0);
    pck.insert(pck.end(), bank.begin(), bank.end());
    pck.resize(streamOff, 0);
    pck.insert(pck.end(), streamed.begin(), streamed.end());
    std::ofstream(root / "sounddata" / "SFX_test.pck", std::ios::binary).write(pck.data(), std::streamsize(pck.size()));

    SetEnvironmentVariableW(L"FHDUELS_FORHONOR_DIR", root.wstring().c_str());
    CHECK(forhonor::FindInstall() == root.wstring());
    CHECK(forhonor::IndexInstall(root.wstring()) == 2);

    fs::path out = root.parent_path() / "cache";
    fs::create_directories(out);
    std::wstring vgm = fs::path(argv[1]).wstring();
    CHECK(forhonor::ExtractWav(111, nullptr, vgm, (out / "fh_111.wav").wstring()));
    CHECK(forhonor::ExtractWav(222, "SFX_test.pck", vgm, (out / "fh_222.wav").wstring()));
    CHECK(!forhonor::ExtractWav(222, "Other.pck", vgm, (out / "fh_x.wav").wstring()));
    CHECK(!forhonor::ExtractWav(333, nullptr, vgm, (out / "fh_333.wav").wstring()));
    CHECK(forhonor::WriteIndexCsv((out / "index.csv").wstring()));

    // Skyrim fallback: a synthetic SE archive (BSA 105, names embedded in the data) with two weapon
    // block sounds and one decoy that must not match "/wpn/|block".
    struct In { const char* dir; const char* name; Bytes data; };
    std::vector<In> files = { { "sound\\fx\\wpn\\block", "wpn_block_01.wav", Wav(330) },
                              { "sound\\fx\\wpn\\block", "wpn_block_02.wav", Wav(550) },
                              { "sound\\fx\\npc\\bear", "bear_block.wav", Wav(110) } };
    const std::uint32_t flags = 0x1 | 0x2 | 0x100;
    std::vector<std::string> dirs = { "sound\\fx\\wpn\\block", "sound\\fx\\npc\\bear" };
    std::uint32_t dirNames = 0, fileNames = 0;
    for (auto& d : dirs) dirNames += std::uint32_t(d.size() + 1);
    for (auto& f : files) fileNames += std::uint32_t(std::strlen(f.name) + 1);
    std::uint32_t dataStart = 36 + 24 * 2 + (dirNames + 2) + 16 * std::uint32_t(files.size()) + fileNames;
    Bytes bsa;
    Tag(bsa, "BSA");  // the literal's terminating NUL makes the 4-byte magic "BSA\0"
    Put32(bsa, 105); Put32(bsa, 36); Put32(bsa, flags); Put32(bsa, 2); Put32(bsa, std::uint32_t(files.size()));
    Put32(bsa, dirNames); Put32(bsa, fileNames); Put16(bsa, 0); Put16(bsa, 0);
    for (int d = 0; d < 2; ++d) { Put32(bsa, d); Put32(bsa, 0); Put32(bsa, d == 0 ? 2 : 1); Put32(bsa, 0); Put32(bsa, 0); Put32(bsa, 0); }
    Bytes data;
    for (int d = 0; d < 2; ++d) {
        bsa.push_back(char(dirs[d].size() + 1));
        bsa.insert(bsa.end(), dirs[d].begin(), dirs[d].end());
        bsa.push_back(0);
        for (auto& f : files) {
            if (dirs[d] != f.dir) continue;
            std::string full = std::string(f.dir) + "\\" + f.name;
            Bytes blob;
            blob.push_back(char(full.size()));
            blob.insert(blob.end(), full.begin(), full.end());
            blob.insert(blob.end(), f.data.begin(), f.data.end());
            Put32(bsa, 0); Put32(bsa, 0); Put32(bsa, std::uint32_t(blob.size())); Put32(bsa, dataStart + std::uint32_t(data.size()));
            data.insert(data.end(), blob.begin(), blob.end());
        }
    }
    for (int d = 0; d < 2; ++d) {
        for (auto& f : files) {
            if (dirs[d] == f.dir) bsa.insert(bsa.end(), f.name, f.name + std::strlen(f.name) + 1);
        }
    }
    CHECK(bsa.size() == dataStart);
    bsa.insert(bsa.end(), data.begin(), data.end());
    std::ofstream(root / "Skyrim - Sounds.bsa", std::ios::binary).write(bsa.data(), std::streamsize(bsa.size()));
    SoundRow row{ Snd::block_impact, "block_impact", "", kWem_block_impact, 0, "/wpn/|block", 1.0f, "fh_{id}.wav" };
    CHECK(skyrimsnd::LoadFrom((root / "Skyrim - Sounds.bsa").wstring(), row, vgm, out.wstring()) == 2);
    CHECK(fs::exists(out / "sk_block_impact_0.wav") && fs::exists(out / "sk_block_impact_1.wav") && !fs::exists(out / "sk_block_impact_2.wav"));

    audio::Load(Snd::block_impact, { (out / "fh_111.wav").wstring(), (out / "fh_222.wav").wstring() });
    std::printf("INFO audio ready: %d (needs XAudio2 at runtime)\n", int(audio::Ready(Snd::block_impact)));
    std::printf("%s (%d failure(s))\n", g_fail ? "SELFTEST FAILED" : "SELFTEST OK", g_fail);
    return g_fail ? 1 : 0;
}

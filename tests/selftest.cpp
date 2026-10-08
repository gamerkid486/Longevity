// Off-game self-test for the FOR HONOR sound path: builds a synthetic Wwise AKPK pack (one sound
// embedded in a bank's DIDX/DATA, one streamed), indexes it, converts both with vgmstream-cli and
// loads them through the audio module. Run: wine64 build/selftest.exe <vgmstream-cli.exe>
#include "audio.h"
#include "forhonor.h"
#include "log.h"

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

    audio::Load(Snd::block_impact, { (out / "fh_111.wav").wstring(), (out / "fh_222.wav").wstring() });
    std::printf("INFO audio ready: %d (needs XAudio2 at runtime)\n", int(audio::Ready(Snd::block_impact)));
    std::printf("%s (%d failure(s))\n", g_fail ? "SELFTEST FAILED" : "SELFTEST OK", g_fail);
    return g_fail ? 1 : 0;
}

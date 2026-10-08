#include "audio.h"

#include "log.h"

#include <windows.h>
#include <mmdeviceapi.h>
#include <xaudio2.h>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <random>

namespace audio {
    namespace {
        struct Clip {
            WAVEFORMATEX fmt{};
            std::vector<BYTE> pcm;
        };

        std::mutex g_lock;
        IXAudio2* g_xa = nullptr;
        IXAudio2MasteringVoice* g_master = nullptr;
        std::map<Snd, std::vector<Clip>> g_clips;
        std::minstd_rand g_rng{ 7 };

        bool HasAudioDevice()
        {
            CoInitializeEx(nullptr, COINIT_MULTITHREADED);
            IMMDeviceEnumerator* en = nullptr;
            if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(&en)))) return false;
            IMMDevice* dev = nullptr;
            bool ok = SUCCEEDED(en->GetDefaultAudioEndpoint(eRender, eConsole, &dev)) && dev;
            if (dev) dev->Release();
            en->Release();
            return ok;
        }

        // One attempt per session; on any failure FOR HONOR sounds stay silent and duels go on.
        bool StartEngine()
        {
            static bool tried = false;
            if (tried) return g_master != nullptr;
            tried = true;
            if (!HasAudioDevice()) return false;
            HMODULE dll = LoadLibraryW(L"xaudio2_9.dll");
            if (!dll) dll = LoadLibraryW(L"xaudio2_8.dll");
            if (!dll) return false;
            using create_t = HRESULT(WINAPI*)(IXAudio2**, UINT32, XAUDIO2_PROCESSOR);
            auto create = reinterpret_cast<create_t>(GetProcAddress(dll, "XAudio2Create"));
            IXAudio2* xa = nullptr;
            if (!create || FAILED(create(&xa, 0, XAUDIO2_DEFAULT_PROCESSOR)) || !xa) return false;
            IXAudio2MasteringVoice* master = nullptr;
            if (FAILED(xa->CreateMasteringVoice(&master)) || !master) {
                xa->Release();
                return false;
            }
            master->SetVolume(tun::sound_volume);
            g_xa = xa;
            g_master = master;
            return true;
        }

        // Minimal RIFF/WAVE reader for vgmstream's PCM16 output.
        bool ReadWav(const std::wstring& path, Clip& out)
        {
            std::ifstream f(std::filesystem::path(path), std::ios::binary);
            std::vector<BYTE> d((std::istreambuf_iterator<char>(f)), {});
            if (d.size() < 12 || std::memcmp(d.data(), "RIFF", 4) || std::memcmp(d.data() + 8, "WAVE", 4)) return false;
            bool haveFmt = false;
            for (std::size_t p = 12; p + 8 <= d.size();) {
                std::uint32_t len;
                std::memcpy(&len, d.data() + p + 4, 4);
                const BYTE* body = d.data() + p + 8;
                if (p + 8 + len > d.size()) break;
                if (!std::memcmp(d.data() + p, "fmt ", 4) && len >= 16) {
                    std::memcpy(&out.fmt, body, 16);
                    out.fmt.cbSize = 0;
                    haveFmt = out.fmt.wFormatTag == WAVE_FORMAT_PCM;
                } else if (!std::memcmp(d.data() + p, "data", 4)) {
                    out.pcm.assign(body, body + len);
                }
                p += 8 + len + (len & 1);
            }
            return haveFmt && !out.pcm.empty();
        }

        float RowVolume(Snd id)
        {
            for (const auto& r : kSounds) {
                if (r.id == id) return r.volume;
            }
            return 1.0f;
        }
    }

    void Load(Snd id, const std::vector<std::wstring>& wavPaths)
    {
        std::lock_guard lk(g_lock);
        if (!StartEngine()) {
            fhd::Log("audio: XAudio2 unavailable");
            return;
        }
        for (const auto& p : wavPaths) {
            Clip c;
            if (ReadWav(p, c)) g_clips[id].push_back(std::move(c));
        }
        fhd::Log("audio: %zu clip(s) loaded for sound %d", g_clips[id].size(), int(id));
    }

    bool Ready(Snd id)
    {
        std::lock_guard lk(g_lock);
        auto it = g_clips.find(id);
        return it != g_clips.end() && !it->second.empty();
    }

    void Play(Snd id)
    {
        std::lock_guard lk(g_lock);
        auto it = g_clips.find(id);
        if (!g_master || it == g_clips.end() || it->second.empty()) return;
        Clip& c = it->second[g_rng() % it->second.size()];
        IXAudio2SourceVoice* voice = nullptr;
        if (FAILED(g_xa->CreateSourceVoice(&voice, &c.fmt))) return;
        XAUDIO2_BUFFER buf{};
        buf.AudioBytes = static_cast<UINT32>(c.pcm.size());
        buf.pAudioData = c.pcm.data();
        buf.Flags = XAUDIO2_END_OF_STREAM;
        voice->SetVolume(RowVolume(id));
        voice->SubmitSourceBuffer(&buf);
        voice->Start();
        // Voices live in a ring of 16: the oldest is destroyed (cut off if somehow still playing).
        static IXAudio2SourceVoice* ring[16]{};
        static int next = 0;
        if (ring[next]) ring[next]->DestroyVoice();
        ring[next] = voice;
        next = (next + 1) % 16;
    }
}

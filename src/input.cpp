#include "input.h"

#include "duel.h"
#include "log.h"

#define DIRECTINPUT_VERSION 0x0800
#include <windows.h>
#include <dinput.h>

#include <atomic>
#include <mutex>
#include <unordered_map>

namespace input {
    namespace {
        // Same slots in IDirectInputDevice8A and W; only GetDeviceInfo's struct differs, and it isn't used.
        using GetCaps_t = HRESULT(STDMETHODCALLTYPE*)(void*, DIDEVCAPS*);
        using GetState_t = HRESULT(STDMETHODCALLTYPE*)(void*, DWORD, void*);
        using GetData_t = HRESULT(STDMETHODCALLTYPE*)(void*, DWORD, DIDEVICEOBJECTDATA*, DWORD*, DWORD);
        constexpr int kGetCaps = 3, kGetState = 9, kGetData = 10;

        struct Table {
            GetState_t state = nullptr;
            GetData_t data = nullptr;
        };
        Table g_a, g_w;

        std::mutex g_lock;
        std::unordered_map<void*, bool> g_isMouse;  // by device
        std::atomic<bool> g_buffered{ false };      // the game reads mouse axes through GetDeviceData
        std::atomic<int> g_stateCalls{ 0 }, g_dataCalls{ 0 };

        bool IsMouse(void* dev)
        {
            std::lock_guard lk(g_lock);
            auto it = g_isMouse.find(dev);
            if (it != g_isMouse.end()) return it->second;
            DIDEVCAPS caps{ sizeof(caps) };
            auto getCaps = reinterpret_cast<GetCaps_t>((*reinterpret_cast<void***>(dev))[kGetCaps]);
            bool mouse = SUCCEEDED(getCaps(dev, &caps)) && GET_DIDEVICE_TYPE(caps.dwDevType) == DI8DEVTYPE_MOUSE;
            if (mouse) fhd::Log("input: mouse device found");
            g_isMouse[dev] = mouse;
            return mouse;
        }

        HRESULT StateHook(const Table& t, void* dev, DWORD size, void* out)
        {
            HRESULT hr = t.state(dev, size, out);
            if (FAILED(hr) || !out || size < sizeof(DIMOUSESTATE) || !IsMouse(dev)) return hr;
            auto* st = static_cast<DIMOUSESTATE*>(out);
            if (g_stateCalls++ < 3) fhd::Log("input: GetDeviceState mouse %ld %ld", st->lX, st->lY);
            if (!g_buffered) duel::OnMouseMove(st->lX, st->lY);
            if (duel::CameraHeld()) st->lX = st->lY = 0;
            return hr;
        }

        HRESULT DataHook(const Table& t, void* dev, DWORD stride, DIDEVICEOBJECTDATA* items, DWORD* count, DWORD flags)
        {
            HRESULT hr = t.data(dev, stride, items, count, flags);
            if (FAILED(hr) || !items || !count || !*count || stride < sizeof(DIDEVICEOBJECTDATA) || !IsMouse(dev)) return hr;
            long dx = 0, dy = 0;
            bool axes = false;
            auto* p = reinterpret_cast<unsigned char*>(items);
            for (DWORD i = 0; i < *count; ++i) {
                auto* d = reinterpret_cast<DIDEVICEOBJECTDATA*>(p + i * stride);
                if (d->dwOfs == DIMOFS_X) dx += static_cast<LONG>(d->dwData);
                else if (d->dwOfs == DIMOFS_Y) dy += static_cast<LONG>(d->dwData);
                else continue;
                axes = true;
            }
            if (!axes) return hr;
            if (g_dataCalls++ < 3) fhd::Log("input: GetDeviceData mouse %ld %ld", dx, dy);
            g_buffered = true;
            duel::OnMouseMove(dx, dy);
            if (duel::CameraHeld()) {
                for (DWORD i = 0; i < *count; ++i) {
                    auto* d = reinterpret_cast<DIDEVICEOBJECTDATA*>(p + i * stride);
                    if (d->dwOfs == DIMOFS_X || d->dwOfs == DIMOFS_Y) d->dwData = 0;
                }
            }
            return hr;
        }

        HRESULT STDMETHODCALLTYPE StateA(void* dev, DWORD size, void* out) { return StateHook(g_a, dev, size, out); }
        HRESULT STDMETHODCALLTYPE StateW(void* dev, DWORD size, void* out) { return StateHook(g_w, dev, size, out); }
        HRESULT STDMETHODCALLTYPE DataA(void* dev, DWORD stride, DIDEVICEOBJECTDATA* items, DWORD* count, DWORD flags) { return DataHook(g_a, dev, stride, items, count, flags); }
        HRESULT STDMETHODCALLTYPE DataW(void* dev, DWORD stride, DIDEVICEOBJECTDATA* items, DWORD* count, DWORD flags) { return DataHook(g_w, dev, stride, items, count, flags); }

        void Patch(void** slot, void* hook, void** orig)
        {
            DWORD old;
            VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &old);
            *orig = *slot;
            *slot = hook;
            VirtualProtect(slot, sizeof(void*), old, &old);
        }

        // Creates a throwaway mouse device through the A or W interface and patches its class vtable.
        bool PatchInterface(REFIID iid, Table& t, void* stateHook, void* dataHook)
        {
            IUnknown* di = nullptr;
            if (FAILED(DirectInput8Create(GetModuleHandleW(nullptr), DIRECTINPUT_VERSION, iid, reinterpret_cast<void**>(&di), nullptr))) return false;
            // CreateDevice is slot 3 in IDirectInput8A and W; GUID_SysMouse is the same for both.
            using Create_t = HRESULT(STDMETHODCALLTYPE*)(IUnknown*, REFGUID, IUnknown**, IUnknown*);
            IUnknown* dev = nullptr;
            HRESULT hr = reinterpret_cast<Create_t>((*reinterpret_cast<void***>(di))[3])(di, GUID_SysMouse, &dev, nullptr);
            if (FAILED(hr) || !dev) {
                di->Release();
                return false;
            }
            void** vt = *reinterpret_cast<void***>(dev);
            if (vt[kGetState] == reinterpret_cast<void*>(&StateA)) {
                t = g_a;  // A and W share one vtable: already patched, don't hook our own hook
            } else {
                Patch(&vt[kGetState], stateHook, reinterpret_cast<void**>(&t.state));
                Patch(&vt[kGetData], dataHook, reinterpret_cast<void**>(&t.data));
            }
            dev->Release();
            di->Release();
            return true;
        }
    }

    bool Install()
    {
        bool a = PatchInterface(IID_IDirectInput8A, g_a, reinterpret_cast<void*>(&StateA), reinterpret_cast<void*>(&DataA));
        bool w = PatchInterface(IID_IDirectInput8W, g_w, reinterpret_cast<void*>(&StateW), reinterpret_cast<void*>(&DataW));
        fhd::Log("input: DirectInput mouse hooks %s/%s", a ? "A" : "-", w ? "W" : "-");
        return a || w;
    }
}

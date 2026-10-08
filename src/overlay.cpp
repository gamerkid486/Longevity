#include "overlay.h"

#include "duel.h"
#include "log.h"

#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>

#include <cmath>

#include "imgui.h"
#include "backends/imgui_impl_dx11.h"
#include "backends/imgui_impl_win32.h"

namespace overlay {
    namespace {
        using Present_t = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT);
        using Resize_t = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);
        Present_t g_origPresent = nullptr;
        Resize_t g_origResize = nullptr;

        bool g_ready = false;
        ID3D11Device* g_device = nullptr;
        ID3D11DeviceContext* g_context = nullptr;
        ID3D11RenderTargetView* g_rtv = nullptr;

        ImU32 Col(const std::uint8_t c[4]) { return IM_COL32(c[0], c[1], c[2], c[3]); }

        // FOR HONOR-style guard indicator: three arcs (top, left, right); the held guard is lit.
        void DrawIndicator(ImDrawList* dl, const hud::HudRow& row, Guard active, bool lit, bool pulsing)
        {
            ImVec2 size = ImGui::GetIO().DisplaySize;
            float scale = size.y / 1080.0f;
            float r = tun::hud_radius_px * scale;
            ImVec2 c(size.x * row.anchorX, size.y * row.anchorY);
            for (const auto& g : kGuards) {
                bool on = lit && g.id == active;
                float mid = g.hudAngleDeg * 3.14159265f / 180.0f;
                float a0 = -(mid + 0.75f), a1 = -(mid - 0.75f);  // screen y grows downward
                dl->PathArcTo(c, r, a0, a1, 24);
                float thick = (on ? 7.0f : 3.0f) * scale * (on && pulsing ? 1.4f : 1.0f);
                dl->PathStroke(on ? Col(row.active) : Col(row.idle), 0, thick);
            }
        }

        void Draw()
        {
            duel::Snapshot s = duel::Read();
            if (!s.playerDuelist) return;
            ImDrawList* dl = ImGui::GetForegroundDrawList();
            DrawIndicator(dl, hud::player_indicator, s.playerGuard, true, s.playerGuarding);
            if (s.hasOpponent) {
                static bool logged = false;
                if (!logged) fhd::Log("overlay: drawing the opponent indicator");
                logged = true;
                DrawIndicator(dl, hud::opponent_indicator, s.opponentGuard, true, s.opponentAttacking);
            }
        }

        void InitImGui(IDXGISwapChain* sc)
        {
            if (FAILED(sc->GetDevice(__uuidof(ID3D11Device), reinterpret_cast<void**>(&g_device)))) return;
            g_device->GetImmediateContext(&g_context);
            DXGI_SWAP_CHAIN_DESC desc;
            sc->GetDesc(&desc);
            ImGui::CreateContext();
            ImGuiIO& io = ImGui::GetIO();
            io.IniFilename = nullptr;
            io.ConfigFlags |= ImGuiConfigFlags_NoMouse | ImGuiConfigFlags_NoMouseCursorChange;
            ImGui_ImplWin32_Init(desc.OutputWindow);
            ImGui_ImplDX11_Init(g_device, g_context);
            g_ready = true;
            fhd::Log("overlay: ready");
        }

        HRESULT STDMETHODCALLTYPE PresentHook(IDXGISwapChain* sc, UINT sync, UINT flags)
        {
            if (!g_ready) InitImGui(sc);
            if (g_ready) {
                if (!g_rtv) {
                    ID3D11Texture2D* back = nullptr;
                    if (SUCCEEDED(sc->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&back)))) {
                        g_device->CreateRenderTargetView(back, nullptr, &g_rtv);
                        back->Release();
                    }
                }
                ImGui_ImplDX11_NewFrame();
                ImGui_ImplWin32_NewFrame();
                ImGui::NewFrame();
                Draw();
                ImGui::Render();
                if (g_rtv) {
                    g_context->OMSetRenderTargets(1, &g_rtv, nullptr);
                    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
                }
            }
            return g_origPresent(sc, sync, flags);
        }

        HRESULT STDMETHODCALLTYPE ResizeHook(IDXGISwapChain* sc, UINT n, UINT w, UINT h, DXGI_FORMAT f, UINT fl)
        {
            if (g_rtv) {
                g_rtv->Release();
                g_rtv = nullptr;
            }
            return g_origResize(sc, n, w, h, f, fl);
        }

        void Patch(void** slot, void* hook, void** orig)
        {
            DWORD old;
            VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &old);
            *orig = *slot;
            *slot = hook;
            VirtualProtect(slot, sizeof(void*), old, &old);
        }
    }

    bool Install()
    {
        WNDCLASSEXW wc{ sizeof(wc), CS_CLASSDC, DefWindowProcW, 0, 0, GetModuleHandleW(nullptr), nullptr, nullptr, nullptr, nullptr, L"FHDuelsDummy", nullptr };
        RegisterClassExW(&wc);
        HWND hwnd = CreateWindowW(L"FHDuelsDummy", L"", WS_OVERLAPPEDWINDOW, 0, 0, 64, 64, nullptr, nullptr, wc.hInstance, nullptr);
        DXGI_SWAP_CHAIN_DESC sd{};
        sd.BufferCount = 1;
        sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.OutputWindow = hwnd;
        sd.SampleDesc.Count = 1;
        sd.Windowed = TRUE;
        IDXGISwapChain* sc = nullptr;
        ID3D11Device* dev = nullptr;
        ID3D11DeviceContext* ctx = nullptr;
        HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &sd, &sc, &dev, nullptr, &ctx);
        if (FAILED(hr)) {
            fhd::Log("overlay: dummy swap chain failed 0x%08lX", hr);
            DestroyWindow(hwnd);
            return false;
        }
        void** vt = *reinterpret_cast<void***>(sc);
        Patch(&vt[8], reinterpret_cast<void*>(&PresentHook), reinterpret_cast<void**>(&g_origPresent));
        Patch(&vt[13], reinterpret_cast<void*>(&ResizeHook), reinterpret_cast<void**>(&g_origResize));
        sc->Release();
        ctx->Release();
        dev->Release();
        DestroyWindow(hwnd);
        return true;
    }
}

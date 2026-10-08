#pragma once

namespace overlay {
    // Hooks IDXGISwapChain::Present (through a throwaway swap chain's vtable) and draws the guard
    // indicators from sheets/hud.json with Dear ImGui.
    bool Install();
}

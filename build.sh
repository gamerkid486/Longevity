#!/usr/bin/env bash
# Builds dist/FHDuels.asi (Windows x64) with mingw-w64, after regenerating code from the sheets
# and running the preflight. Usage: ./build.sh
set -euo pipefail
cd "$(dirname "$0")"

IMGUI_TAG=v1.91.9
[ -d third_party/imgui ] || git clone -q --depth 1 --branch "$IMGUI_TAG" https://github.com/ocornut/imgui third_party/imgui

python3 tools/gen.py
python3 tools/preflight.py

CXX=${CXX:-x86_64-w64-mingw32-g++-posix}
IM=third_party/imgui
mkdir -p build dist
$CXX -std=c++20 -O2 -s -shared -o dist/FHDuels.asi \
  -Isrc -I"$IM" \
  -DWIN32_LEAN_AND_MEAN -DNOMINMAX -DUNICODE -D_UNICODE -D_WIN32_WINNT=0x0A00 \
  -DIMGUI_DISABLE_WIN32_DEFAULT_IME_FUNCTIONS \
  -Wall -Wno-unknown-pragmas \
  src/main.cpp src/log.cpp src/game.cpp src/hooks.cpp src/duel.cpp src/audio.cpp src/forhonor.cpp src/overlay.cpp \
  "$IM"/imgui.cpp "$IM"/imgui_draw.cpp "$IM"/imgui_tables.cpp "$IM"/imgui_widgets.cpp \
  "$IM"/backends/imgui_impl_dx11.cpp "$IM"/backends/imgui_impl_win32.cpp \
  -static -static-libgcc -static-libstdc++ \
  -ld3d11 -ldxgi -ld3dcompiler_47 -lversion -lshell32 -lole32 -luuid -ladvapi32 -lgdi32 -ldwmapi
ls -l dist/FHDuels.asi

if [ "${1:-}" = "test" ]; then
  $CXX -std=c++20 -O1 -o build/selftest.exe -Isrc -DUNICODE -D_UNICODE -D_WIN32_WINNT=0x0A00 \
    tests/selftest.cpp src/forhonor.cpp src/audio.cpp src/log.cpp \
    -static -static-libgcc -static-libstdc++ -lshell32 -lole32 -luuid -ladvapi32
  echo "built build/selftest.exe"
fi

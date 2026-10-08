#!/usr/bin/env bash
# Builds the release zip: FHDuels.asi + bundled vgmstream + licences. Never includes game files.
# Usage: ./package.sh <version>
set -euo pipefail
cd "$(dirname "$0")"
VER=${1:?version}
VGM_URL=https://github.com/vgmstream/vgmstream/releases/download/r2117/vgmstream-win64.zip
./build.sh
STAGE=build/stage
rm -rf "$STAGE" && mkdir -p "$STAGE/FHDuels/vgmstream" "$STAGE/FHDuels/licenses"
[ -f build/vgmstream-win64.zip ] || curl -sSfL -o build/vgmstream-win64.zip "$VGM_URL"
echo "6c4a8a3813864fefed081bbd337dbc0ad93bf88e0b92f5db98d7ab258b22dc6c  build/vgmstream-win64.zip" | sha256sum -c -
unzip -oq build/vgmstream-win64.zip -d "$STAGE/FHDuels/vgmstream"
mv "$STAGE/FHDuels/vgmstream/COPYING" "$STAGE/FHDuels/licenses/vgmstream-COPYING.txt"
rm -f "$STAGE/FHDuels/vgmstream/USAGE.md"
cp third_party/imgui/LICENSE.txt "$STAGE/FHDuels/licenses/imgui-LICENSE.txt"
cp docs/THIRD_PARTY.md "$STAGE/FHDuels/licenses/THIRD_PARTY.md"
cp dist/FHDuels.asi "$STAGE/"
rm -f "dist/FHDuels-$VER.zip"
(cd "$STAGE" && zip -qr9 "../../dist/FHDuels-$VER.zip" .)
ls -l "dist/FHDuels-$VER.zip" && sha256sum "dist/FHDuels-$VER.zip"

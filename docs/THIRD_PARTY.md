# Third-party software in FOR HONOR Duels

| Component | Author | Licence | Used for |
|---|---|---|---|
| Dear ImGui v1.91.9 | Omar Cornut | MIT (imgui-LICENSE.txt) | Drawing the guard indicators, compiled into FHDuels.asi |
| vgmstream r2117 (vgmstream-cli.exe and its DLLs) | vgmstream contributors | ISC-style (vgmstream-COPYING.txt). Bundled FFmpeg DLLs are LGPL-2.1, libvorbis is BSD, mpg123 is LGPL-2.1, and the codec libraries carry their own licences; source is at https://github.com/vgmstream/vgmstream (FFmpeg source: https://ffmpeg.org/download.html) | Converting FOR HONOR's Wwise sounds to WAV on your own PC |
| Ultimate ASI Loader | ThirteenAG | MIT | Loads FHDuels.asi (installed by Melty, not bundled) |

Reference only (nothing shipped): CommonLibSSE-NG (MIT) for Skyrim struct layouts and Address Library IDs;
valhallaCombat by D7ry for the melee-hit hook site; meh321's AddressLibraryDatabase for the per-version
offsets embedded in the plugin.

No files from The Elder Scrolls V: Skyrim or FOR HONOR are included. FOR HONOR's sounds are read from
your own installed copy when the game starts, and the converted WAVs stay on your PC in
`%LOCALAPPDATA%\FHDuels`.

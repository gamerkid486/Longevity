# Run 2: plugin loads and patches hooks, but nothing changes in game

Date: 2026-10-07, local Windows 11 session. Build: `release/FHDuels-0.1.1.zip` (sha256 matched).

## Result
- No crash. There's no SkyrimSE.exe entry in the Event Viewer Application log for the session.
- The player reports that **everything played like normal Skyrim, including combat**: no guard
  indicator at the bottom, no enemy indicator at the top, no blocking or recoil effect from matching
  guards, and no FHDuels notification. (The individual step 6 questions weren't answered separately.
  The player's summary covers all of them.)
- `FHDuels.log` (copied in full) shows that all three hooks were patched and the overlay reported
  "ready". **No line was logged after startup** (21:25:14). The player then played for about 10 more
  minutes (the last save is 21:36).

## Versions and paths
- Skyrim SE 1.7.104.0: `C:\Program Files (x86)\Steam\steamapps\common\Skyrim Special Edition`.
  Started from Steam (not skse64_loader).
- FOR HONOR (Steam): `C:\Program Files (x86)\Steam\steamapps\common\For Honor`. Only read (file
  sizes and the first bytes of each `.pck`), never started.
- Ultimate ASI Loader 9.7.4 (`dinput8.dll`), already installed.
- The log is written to the OneDrive-redirected Documents folder,
  `%USERPROFILE%\OneDrive\Documents\My Games\Skyrim Special Edition\FHDuels.log`, not
  `%USERPROFILE%\Documents\...`.

## Hook targets checked against the player's Address Library (versionlib-1-7-104-0.bin, format 5)
A reverse lookup of the vtable slot RVAs from the log:
- `vtbl_look_handler[0x3] -> +0x7B32A0` = **ID 523967**. IDs this high don't exist in 1.6.1179
  (that file's count is 428510), so this function is new or renumbered in 1.7.x. The neighbors are
  ID 42424 (+0x7B3270) and ID 523968 (+0x7B37B0). **Suspect: slot 3 may no longer be
  ProcessMouseMove on 1.7.104** (a virtual may have been added).
- `vtbl_player_character[0xAD] -> +0x745200` = **ID 40447** (neighbors 40446 at +0x7450B0 and 40448 at
  +0x7466E0). It's an ordinary pre-1.7 ID. Whether it's PlayerCharacter::Update still needs checking
  against the 1.6.1179 slot 0xAD target.
- The hit call at +0x4A8 was found. `tools/versionlib_dump.ps1` re-run: all 11 sheet offsets match for
  both 1.6.1179 and 1.7.104.
- The tick hook would log or show something every frame if it ran (HUD snapshot, notifications), and
  nothing appeared. So either the 0xAD slot isn't called per frame on 1.7.104, or the HUD and
  opponent logic never activate. Note: SkyUI (and its UIExtensions) were switched off for this run.

## FOR HONOR sounds
- 21 `.pck` files in `For Honor\SoundPackages\`, 11 to 23 MB each. **Every one starts with the same
  bytes `C0 4E 1E A2`** instead of `AKPK`. The first 32 bytes of `soundpc_00_aa_sfx.pck` are
  `C0 4E 1E A2 C6 F1 AA D1 C9 E0 02 00 8C 8C 02 4D 41 C7 03 42 89 80 86 10 68 00 00 00 00 00 00 20`.
  They look encrypted or wrapped (the shared 4-byte prefix is a container magic or a key-dependent
  header).
- Result: 0 packs and 0 sounds indexed. `index.csv` is just the header (copied).

## Changes to the game setup (step 2b and step 4), all reversible
- Renamed `SkyCraft.asi` to `SkyCraft.asi.off` in the Skyrim folder. SkyCraft did **not** load
  alongside FHDuels in this run.
- Copied `%LOCALAPPDATA%\Skyrim Special Edition\Plugins.txt` to `Plugins.txt.fhduels-backup`, then
  switched off (removed the `*` from) 15 plugins: unofficial skyrim special edition patch.esp,
  Campfire.esm, SkyUI_SE.esp, SMIM-SE-Merged-All.esp, ssNordMage.esp, castle volkihar rebuilt.esp,
  SeranaDialogAddon.esp, Lucien.esp, SDA Campfire Patch.esp, SDA Castle Volkihar Rebuilt Patch.esp,
  Followers.esp, CVDNMaster.esp, CAMPFIRE_ADV_REPLACER.esp, Apocalypse - Magic of Skyrim.esp,
  UIExtensions.esp.
- **Vortex manages the Data folder** (`Data\SKSE\Plugins\__folder_managed_by_vortex`), so it may
  rewrite Plugins.txt the next time it opens.
- Added `FHDuels.asi` and the `FHDuels\` folder (`vgmstream\*`, `licenses\*`) from the zip to the
  Skyrim folder. They were left installed.
- `FHDUELS_DUMP_INDEX=1` was set with setx and has since been removed (`reg delete`).
- SKSE (1.6.1170 build, mismatched) was left alone. `Data\SKSE\Plugins` also has versionlib files for
  1.6.1170, 1.6.1179, 1.7.99 and 1.7.104.

- At the end of the run, the player chose to **keep the other mods set aside** for the next test.
  `SkyCraft.asi.off` and `Plugins.txt.fhduels-backup` are still in place.

## Suggestions for the cloud session
- Make the plugin log the first call of each hook (once) and every guard change or opponent pick,
  so the next run can show which hook never fires.
- Find the 1.7.104 vtable index of LookHandler::ProcessMouseMove (check what ID 523967 is, and
  compare the slot layout with 1.6.1179).
- FOR HONOR's packs need a different reader, or decryption that's out of scope. The sound part
  can't work with plain AKPK parsing.

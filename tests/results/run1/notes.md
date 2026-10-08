# Run 1: stopped at step 1 (unsupported Skyrim version)

Date: 2026-10-07, local Windows 11 session.

## Result
Stopped before installing. `SkyrimSE.exe` FileVersion is **1.7.104.0**. Supported versions are
1.6.640, 1.6.659, 1.6.1130, 1.6.1170 and 1.6.1179. The plugin needs per-version addresses for 1.7.104
before it can be tested.

## Paths
- Skyrim SE (Steam 489830): `C:\Program Files (x86)\Steam\steamapps\common\Skyrim Special Edition`
- FOR HONOR (Steam 304390): `C:\Program Files (x86)\Steam\steamapps\common\For Honor`, with 21 `.pck` files
  found by a recursive search. FOR HONOR was only read (a file count), never started or changed.

## Already in the Skyrim folder (nothing removed or changed)
- `dinput8.dll`: Ultimate ASI Loader x64 **9.7.4**, dated 2026-10-07. Step 3 isn't needed.
- `SkyCraft.asi`: another ASI mod (no version info), dated 2026-10-07
- `skse64_loader.exe` + `skse64_1_6_1170.dll` (2024-01-17). This SKSE is older than the game version.
- No `winmm.dll`, `version.dll`, `d3d11.dll` or `dxgi.dll` (so no ReShade or ENB).

## Not done
- No files were added to the game folder. `FHDuels-0.1.0.zip` was verified locally (sha256 matches) but
  wasn't extracted.
- `FHDUELS_DUMP_INDEX` wasn't set. There's no game session, `FHDuels.log` or `index.csv`.

## Next steps (cloud session)
- Add 1.7.104 addresses (check whether AddressLibraryDatabase / `skyrimae.relib` has an entry for 1.7.104),
  then rebuild.
- `SkyCraft.asi` will load alongside FHDuels and may hook the same functions (player Update, LookHandler,
  Present). Keep that in mind when reading the first real log.

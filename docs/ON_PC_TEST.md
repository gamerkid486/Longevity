# First on-PC test of FOR HONOR Duels (instructions for a Claude session on the user's Windows PC)

You're running on the user's Windows PC, in a clone of github.com/gamerkid486/Longevity on branch
`claude/skyrim-mod-melty-8pkxd9`. Read `README.md` and `MODLOG.md` first.

The mod is an ASI plugin (`FHDuels.asi`) for Skyrim SE/AE, loaded by Ultimate ASI Loader. It adds
FOR HONOR guard-stance duels and reads FOR HONOR's Wwise sounds from the user's FOR HONOR install,
read-only. Never launch, change or inject into FOR HONOR: it's online-only and uses EasyAntiCheat.

Talk to the user in plain, friendly language, one step at a time. Ask before changing anything in the
game folder.

## Steps
1. **Find the games.**
   - Skyrim Special Edition: Steam app 489830, `SkyrimSE.exe`. Find it through Steam's
     `steamapps/libraryfolders.vdf`.
   - FOR HONOR: Steam app 304390, or the Ubisoft Connect registry key
     `HKLM\SOFTWARE\WOW6432Node\Ubisoft\Launcher\Installs\*\InstallDir`.

   Report both paths and the exact file version of `SkyrimSE.exe`, for example with PowerShell:
   `(Get-Item SkyrimSE.exe).VersionInfo.FileVersion`. The supported versions are 1.6.640, 1.6.659,
   1.6.1130, 1.6.1170, 1.6.1179 and 1.7.104. If the version isn't one of these, stop and report it.
2. **Check what's already in the Skyrim folder.** List any of these files: `dinput8.dll`,
   `winmm.dll`, `version.dll`, `*.asi`, `skse64_loader.exe`, and `d3d11.dll` or `dxgi.dll`
   (ReShade/ENB). Don't remove anything.
2b. **Set other mods aside for the test (the user agreed to this; still confirm before you start).**
   Everything here must be reversible: rename or switch off, never delete. Write each change into
   notes.md so step 8 can undo it.
   - Other ASI mods: rename each `*.asi` except `FHDuels.asi` to `*.asi.off` (for example
     `SkyCraft.asi`). Keep `dinput8.dll`, since it loads our mod.
   - Plugins from Nexus or Creations: copy `%LOCALAPPDATA%\Skyrim Special Edition\Plugins.txt` to
     `Plugins.txt.fhduels-backup`. Then remove the leading `*` from every line except the official
     masters (`Skyrim.esm`, `Update.esm`, `Dawnguard.esm`, `HearthFires.esm`, `Dragonborn.esm`). That
     switches them off without touching their files. If a mod manager (Vortex or Mod Organizer 2) is
     installed, tell the user, because it may rewrite this file the next time it opens.
   - Leave SKSE alone. The game is started from Steam or `SkyrimSE.exe`, not `skse64_loader.exe`.
   - Saves made with those mods may warn about missing content when loaded. That's fine for a
     throwaway save. Don't save over a save the user cares about.
3. **Install Ultimate ASI Loader if it's missing.** Ask the user first. Then download the official x64
   build from https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases (the x64 zip asset) and put
   its `dinput8.dll` next to `SkyrimSE.exe`. Melty installs this loader for players itself; it's only
   needed for this manual test. Write down every file you add. (Run 1 found Ultimate ASI Loader 9.7.4
   already installed, so this step is usually skipped.)
4. **Install the test build.** Check `release/FHDuels-0.1.3.zip` against
   `release/FHDuels-0.1.3.zip.sha256`. Then extract it into the Skyrim folder; it adds `FHDuels.asi`
   and an `FHDuels/` folder. Write down the added files so they can be removed later.
5. **Turn on the sound list for the next launch.** Set a user environment variable with
   `setx FHDUELS_DUMP_INDEX 1`. The game has to inherit it: either have the user restart Steam, or
   launch `SkyrimSE.exe` directly from a new shell where the variable is set.
6. **Play the test fight with the user.** Ask them to start Skyrim and load a save (a throwaway save
   is safest). Have them draw a melee weapon and fight an armed human enemy; a bandit is ideal. Ask
   them to report:
   - when they hold block and flick the mouse up, left or right, whether the guard indicator at the
     bottom changes and whether the camera stays still;
   - whether an indicator for the enemy's guard appears at the top;
   - when they match the enemy's guard, whether the attack does no damage and the enemy recoils;
   - whether a notification appears at the top left, and what it says.

   Ask these one at a time and write down each answer separately, even if nothing happened. Also ask
   them to sheathe and draw the weapon once (the bottom indicator should disappear and come back),
   and to try one fight with no weapon drawn. When an enemy attacks, ask them to look at the top of
   the screen for a second indicator.

   If you have screenshot tools, capture the game window only, showing the indicators during a fight.
7. **Collect the results.** After the user quits the game, copy these into `tests/results/run4/`:
   - `Documents/My Games/Skyrim Special Edition/FHDuels.log`. On this PC, Documents is redirected to
     OneDrive: `%USERPROFILE%\OneDrive\Documents\My Games\Skyrim Special Edition\FHDuels.log`;
   - `%LOCALAPPDATA%/FHDuels/index.csv`, if it exists. If it's over 20 MB, commit a gzip of it instead;
   - a `notes.md` file with the game versions and paths, the files added to the game folder, the
     user's answers from step 6, any crash or error text, and anything else odd.

   Never commit game files, extracted sounds (`.wem`/`.wav`), tokens, or personal paths other than
   the game folders.
8. **Remove the environment variable:** `reg delete HKCU\Environment /v FHDUELS_DUMP_INDEX /f`. Leave
   the mod installed unless the user asks you to remove it. Then ask the user whether to put their
   other mods back now. If they say yes, undo step 2b: rename the `*.asi.off` files back and restore
   `Plugins.txt` from `Plugins.txt.fhduels-backup`.
9. **Send the results back.** Commit `tests/results/run4/`, push it to
   `claude/skyrim-mod-melty-8pkxd9`, and tell the user to go back to the cloud session and say
   "results are pushed".

## If something goes wrong
- **The game crashes on start, or there's no FHDuels.log:** check that the ASI loader is present and
  that the log folder exists. Collect the Windows Event Viewer application error for SkyrimSE.exe and
  any crash logs from other mods.
- **The log says "hooks not installed":** include the exact line. The plugin refuses to patch the
  game when its safety checks fail; that's expected behavior.
- **Don't edit the plugin source here.** Report what you found, and the cloud session will fix and
  rebuild it.

# Run 3: duelist and guard state now detected, but guard direction, enemy HUD and blocking don't work

Date: 2026-10-07, local Windows 11 session. Build: `release/FHDuels-0.1.2.zip` (sha256 matched;
installed `FHDuels.asi` sha256 `1EB27755...095869` matches the zip entry).

## Result summary
- No crash. There's no SkyrimSE.exe entry in the Event Viewer Application log since 21:50.
- **The 0.1.2 offset fix worked:** the log shows `player duelist: yes`, guard changes, an opponent and
  two melee hits. The bottom guard indicator now appears.
- **The look-handler hook never fired:** there's no `hooks: first look handler call` line (and no
  mouse delta lines), although the player flicked the mouse while blocking. The guard stayed on "up"
  and the camera turned normally. This matches the run 2 suspicion that `vtbl_look_handler[0x3]`
  (+0x7B32A0, ID 523967) isn't ProcessMouseMove on 1.7.104.
- The player update hook fired (`hooks: first player update call` at 21:52:42) and so did the melee hit
  hook (21:54:44).

## Step 6 answers (each asked separately; one-handed sword vs. an armed human enemy, throwaway save)
1. **Block plus flicking the mouse up, left or right:** "the guard stays in the up position but
   highlights when i guard. the camera does not stay still either. so combat is still standard skyrim
   combat but i do see the indicator"
2. **Enemy guard indicator at the top:** "there is no enemy indicator"
3. **Matching guard blocks the attack and the enemy recoils:** "it plays out like a normal skyrim
   block" (since the player's guard couldn't change, this was the up guard).
4. **Notification at the top left:** "no notification appeared"
5. **Sheathe and draw the weapon once:** "bottom indicator does not disappear when i sheath my
   sword." The log has no `player duelist: no` line after sheathing, so the duelist state didn't
   update when the weapon was put away.

## Log observations (full log in `FHDuels.log`)
- `player duelist: yes (right hand form 00013790 type 0x29 anim 3)` at 21:53:48. The duelist state
  never switched back to "no", including when the sword was sheathed (the sheathe happened near the
  end, around 21:58).
- `opponent: found` at 21:54:42 and `opponent: none` at 21:55:34, yet the player saw no enemy
  indicator at the top while it was "found".
- `melee hit: npc attacks player, guards 2 vs 1 -> landed`: the player was guarding (guard 1, which
  is presumably up) and the NPC's guard was 2, so it wasn't a match. Every NPC hit while the player held
  block reported a mismatch, and Skyrim's normal block played out.
- `melee hit: player attacks npc, guards 1 vs 1 -> landed`: guards **matched** (1 vs 1), but the hit
  still landed. The hit rule probably only applies when the NPC is defending. Worth checking.
- Only 2 `melee hit` lines were logged during a whole fight. The log only records non-duel hits, so
  either most hits weren't logged or they weren't routed through this call site.

## Versions and paths
- Skyrim SE 1.7.104.0: `C:\Program Files (x86)\Steam\steamapps\common\Skyrim Special Edition`.
  Started from Steam (not skse64_loader). Steam was not restarted.
- FOR HONOR (Steam): `C:\Program Files (x86)\Steam\steamapps\common\For Honor`. Only read, never
  started. All 21 packs are still "not a plain AKPK pack". 0 sounds indexed (`index.csv` is just the
  header, copied). Step 5 (the sound list) was skipped as planned.
- Ultimate ASI Loader 9.7.4 (`dinput8.dll`), already installed.
- The log is at `%USERPROFILE%\OneDrive\Documents\My Games\Skyrim Special Edition\FHDuels.log`.

## Changes to the game setup
- Replaced `FHDuels.asi` (0.1.1, 2,264,576 bytes, now 0.1.2 at 2,266,112 bytes) and the `FHDuels\`
  folder's files with the ones from the 0.1.2 zip. Only the ASI and two license files changed; the
  vgmstream files are identical. Left installed.
- Step 2b from run 2 is still in effect: `SkyCraft.asi.off` and `Plugins.txt.fhduels-backup` are in
  place. All 15 plugins are still switched off in `Plugins.txt`; **Vortex hasn't re-enabled
  anything** (Plugins.txt last written 2026-10-07 21:25:20, during run 2).
- No environment variables were set this run.

## Suggestions for the cloud session
- Find the right LookHandler vtable slot for ProcessMouseMove on 1.7.104. Slot 3 is never called while
  blocking and moving the mouse.
- Re-evaluate duelist state when the weapon is sheathed (weapon-drawn flag), so the HUD hides.
- Check why the opponent HUD doesn't draw when `opponent: found`.
- Check the player-attacks-npc case, where `guards 1 vs 1 -> landed` was logged.

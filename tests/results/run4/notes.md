# Run 4: guard flicks, camera hold, enemy HUD and sheathe-hide all work; matched blocks don't stagger

Date: 2026-10-08 (game session 05:14 to 05:25 local), local Windows 11 session. Build:
`release/FHDuels-0.1.3.zip` (sha256 `984779c8...0e71` matched). The installed `FHDuels.asi`
(2,302,976 bytes, sha256 `956F0E07...95CF`) matches the zip entry, and all `FHDuels\` folder files
match the zip.

## Result summary
- No crash. The game closed normally, and there's no SkyrimSE.exe entry in the Event Viewer
  Application log from the last 3 hours.
- **The DirectInput mouse path works:** the log has `input: DirectInput mouse hooks A/W` and
  `input: mouse device found`. Guard flicks change the guard (`player guard -> Left/Right/Top`) and
  the camera holds still while blocking.
- **The enemy indicator now shows:** `overlay: drawing the opponent indicator` was logged, and the
  player saw it at the top.
- **Sheathing hides the HUD:** `player duelist: no (... weapon state 0)` after sheathing, and `yes`
  again after drawing (weapon state 3).
- **Matched guards block:** there are many `-> blocked` lines in both directions, and the player
  took no damage, but the attacker doesn't stagger (no recoil).

## Step 6 answers (each asked separately; one-handed sword vs. an armed human enemy, throwaway save)
1. **Block plus flicking the mouse up, left or right:** "the lit guard indicator did change. the
   camera did stay still but didn't lock onto the target"
2. **Enemy guard indicator at the top:** "Yes, it appears" (a second indicator shows up at the top
   during the fight).
3. **Matching guard blocks the attack and the enemy recoils:** "No damage, no recoil" (blocked, but
   the enemy doesn't stagger).
4. **Notification at the top left:** "No notification".
5. **Sheathe and draw the weapon once:** "Hides, then returns" (the bottom indicator disappears when
   sheathed and comes back when drawn).
6. **Fight with no weapon drawn:** "No indicators, normal" (no indicators shown; plain Skyrim
   combat).

## Log observations (full log in `FHDuels.log`)
- Guard numbers in the hit lines look like 1 = Top, 2 = Left, 3 = Right. For example,
  `player guard -> Left` at 05:22:50.697 is followed by `npc attacks player, guards 2 vs 2 ->
  blocked`.
- `blocked` happens in both directions: `npc attacks player, guards 2 vs 2 -> blocked` (5 times) and
  `player attacks npc, guards 1 vs 1 / 3 vs 3 / 2 vs 2 -> blocked`. The player said there was no
  recoil, so the stagger/recoil reaction doesn't seem to be applied, or isn't visible.
- Many hits are still `landed` with matching guards because of `(victim mid-attack)`. In the second
  and third fights, about half the hits were trades where both sides were attacking.
- Several hits are logged as `npc attacks player, guards X vs Y -> landed` while the log shows
  `player guarding: no` at that moment, so the player's guard value is used even when not blocking.
  Check whether that's intended.
- In the first fight there were many `melee hit: npc vs npc, not a duel` lines (8). Other NPCs were
  fighting nearby.
- The first hit of each fight is `not a duel (victim right hand form 000135B8 type 0x29 anim 1,
  weapon state 2, weapon melee)`: the NPC was still drawing (weapon state 2).
- **Odd:** at 05:21:50.531 the same NPC weapon (form 000135B8, type 0x29, anim 1, weapon state 3) was
  reported as `weapon not melee`, although the same form was `weapon melee` a few seconds earlier.
  The melee check may be flaky for that weapon (anim 1 = one-handed sword?).
- At 05:21:02 to 05:21:08 the guard changed about 30 times in 6 seconds (Left/Top/Right cycling every
  50 to 150 ms). This was during the player's flick testing and may just be fast flicking, but the
  flick threshold could be too sensitive.
- `input: mouse device found` is logged again at 05:20:01, 05:22:19, 05:24:13, 05:24:37 and
  05:25:32. The game seems to re-create or re-acquire the mouse device (menus, loading, quitting),
  and the hooks kept working after each one.
- `overlay: drawing the opponent indicator` is only logged once, as designed for the first draw.
- No notification lines in the log, matching answer 4.

## Versions and paths
- Skyrim SE 1.7.104.0: `C:\Program Files (x86)\Steam\steamapps\common\Skyrim Special Edition`.
  Started from Steam (not skse64_loader).
- FOR HONOR (Steam): `C:\Program Files (x86)\Steam\steamapps\common\For Honor`. Only read, never
  started. All 21 packs are still "not a plain AKPK pack". 0 sounds indexed (`index.csv` is just the
  32-byte header from run 3, copied). Step 5 (the sound list) was skipped as planned.
- Ultimate ASI Loader 9.7.4 (`dinput8.dll`), already installed.
- The log is at `%USERPROFILE%\OneDrive\Documents\My Games\Skyrim Special Edition\FHDuels.log`.

## Changes to the game setup
- Replaced `FHDuels.asi` (0.1.2, 2,266,112 bytes, now 0.1.3 at 2,302,976 bytes) and overwrote the
  `FHDuels\` folder's files with the ones from the 0.1.3 zip. Left installed.
- Step 2b from run 2 is still in effect: `SkyCraft.asi.off` and `Plugins.txt.fhduels-backup` are in
  place. All 15 plugins are still switched off in `Plugins.txt`; **Vortex hasn't re-enabled
  anything** (no `*` lines; the file was last written 2026-10-08 05:15:03 when the game started).
  The user chose to keep the other mods set aside for further runs.
- No environment variables were set this run, so there was nothing to remove in step 8.

## Suggestions for the cloud session
- Add a stagger/recoil to the attacker on a `blocked` hit. The player saw no recoil.
- The player expected the camera to lock onto the target while guarding (FOR HONOR style). It holds
  still but doesn't track the opponent.
- Look into the `weapon not melee` result for form 000135B8 at 05:21:50.531.
- Consider whether `-> landed` / `-> blocked` should use the player's guard while they aren't
  blocking, and whether the flick threshold needs debouncing.
- Still no top-left notification on block. Check whether one is expected.

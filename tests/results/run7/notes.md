# Run 7: lock-on turns at about the right speed and stays on the enemy; block sounds play on every block

Date: 2026-10-08 (game session 06:16:36 to 06:25:17 local), local Windows 11 session. Build:
`release/FHDuels-0.1.6.zip` (sha256 `e8dca28c...e7b4` matched). The installed `FHDuels.asi`
(2,334,720 bytes, sha256 `B37DD2B8...E472`) matches the zip.

## Result summary
- No crash. The game closed normally, and the Event Viewer Application log has no SkyrimSE.exe
  entry from the last 2 hours.
- **Lock-on is fixed.** The player said the turn speed is "about right", and while holding block and
  moving or circling for 10+ seconds the camera "stayed on them". There are 120 `lock-on:` lines.
  Start of a hold: -36.6 deg goes to -0.7 deg one second later (06:17:27.591 to 06:17:28.591), and
  -24.7 to -2.1, 19.0 to -1.8, 74.1 to 2.1, 60.5 to 6.1 to -1.5 the same way. In 102 lines that
  aren't the first of a hold, 84 are within 3 deg and 89 within 5 deg.
- **Learned degrees per count:** 0.0361 to 0.0787, median 0.0463, mean 0.0491. That's close to the
  0.05 start value and run 6's estimate. No sign of a sign flip: it stays positive and the errors stay
  small.
- **Block sound works,** and our clip is what plays: every one of the 41 `-> blocked` lines is
  followed by `audio: played sound 0 clip N` at the same millisecond. All 6 Skyrim clips were used
  (clip 0 x9, 1 x7, 2 x6, 3 x8, 4 x6, 5 x5). The player heard a clang.
- **Recoil, guard flicks, the HUD and sheathe-hide** still work, according to the player. All 41
  blocks are followed by `attacker reaction: recoilLargeStart`. There are 48 `player guard ->`
  changes.

## Step 6 answers (each asked separately; throwaway save, melee weapon vs. an armed human enemy)
1. **Holding block near an enemy who is off to the side, how quickly does the camera turn to face
   them?** "about right"
2. **Holding block and moving or circling for 10+ seconds, does the camera ever turn away?** "no it
   stayed on them"
3. **Is there still a clang on matched blocks?** "yes there was a clang"
4. **Do recoil, guard flicks, the HUD and sheathe-hide still work?** "all still worked"

(The notification wasn't asked this time. The log has it once at 06:17:14.858: "FOR HONOR Duels:
hold block and flick the mouse to change guard. Using Skyrim's block sounds.")

## Log observations (full log in `FHDuels.log`)
- **A few one-second samples mid-hold are far off, and the next one is back on target:**
  - `-46.4` at 06:19:38.257 goes to `-0.9` at 06:19:39.257;
  - `-45.9` at 06:19:56.256 goes to `1.2`;
  - `-47.3` at 06:19:59.257 goes to `2.1`;
  - `-16.4` at 06:19:40.257 and `12.7` at 06:19:28.257, both recovered;
  - `68.7` at 06:24:03.872 came 0.37 s after a new hold started (06:24:03.506), so it's really a
    hold start.

  Several NPCs were fighting nearby (`npc vs npc` lines), so these may be the enemy dodging past,
  a recoil or stagger moving them, or the target switching to another NPC. Opponent switches
  aren't logged. The player didn't notice any of them as turning away.
- The steady error stays within about ±2.5 deg (the dead zone or the enemy's own movement), with
  no drift in one direction.
- Hits: 41 blocked (34 NPC to player, 7 player to NPC), 53 landed. The recoil goes to the attacker
  in both directions.
- `melee hit: not a duel (... weapon not melee)` twice (06:17:54.075, 06:24:03.323), the same
  victim form `000135B8` as in run 6.
- `input: mouse device found` 4 times (06:16:46, 06:18:44, 06:19:24, 06:21:32), probably menus or
  alt-tab. Nothing broke.
- The sheathe and draw cycles in `player duelist: yes/no` match the sheathe-hide answer.

## Versions and paths
- Skyrim SE 1.7.104.0: `C:\Program Files (x86)\Steam\steamapps\common\Skyrim Special Edition`.
  Started from Steam (not skse64_loader).
- FOR HONOR (Steam): `C:\Program Files (x86)\Steam\steamapps\common\For Honor`. Only read, never
  started. All 21 packs are still "not a plain AKPK pack", and 0 sounds were indexed. Step 5 (the
  sound list) was skipped as planned.
- `Data\Skyrim - Sounds.bsa` was only read: 23 of 6198 matched, and 6 were loaded, the same as in
  run 6.
- Ultimate ASI Loader 9.7.4 (`dinput8.dll`), already installed.
- The log is at `%USERPROFILE%\OneDrive\Documents\My Games\Skyrim Special Edition\FHDuels.log`.

## Changes to the game setup
- Replaced `FHDuels.asi` (0.1.5, 2,333,696 bytes, sha256 `FE836591...3F24`, now 0.1.6) and
  overwrote the `FHDuels\` folder's files with the 0.1.6 zip's. Left installed. A copy of the 0.1.5
  `.asi` is kept outside the game folder, in the session scratch folder.
- Step 2b from run 2 is still in effect: `SkyCraft.asi.off` and `Plugins.txt.fhduels-backup` are in
  place. `Plugins.txt` had no `*` lines before or after the run, so **Vortex hasn't re-enabled
  anything**. The game rewrote the file at 06:16:46, and the plugins stayed off.
- No environment variables were set, so there was nothing to remove in step 8.

## Suggestions for the cloud session
- Lock-on can be marked working in game on 1.7.104. The learned degrees per count is about 0.046
  on this mouse.
- If the brief 45-deg samples matter, log opponent switches (the form ID when the target changes)
  so they can be told apart from the enemy dodging.
- Block sounds are confirmed as our clip (an `audio: played` line on every block). Recoil, flicks,
  the HUD and sheathe-hide are still verified.

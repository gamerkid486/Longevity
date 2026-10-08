# Run 6: lock-on turns toward the enemy but slowly, then turns away; Skyrim block sounds play

Date: 2026-10-08 (game session 05:58:59 to 06:00:12 local), local Windows 11 session. Build:
`release/FHDuels-0.1.5.zip` (sha256 `e7ab268f...0b1a` matched). The installed `FHDuels.asi`
(2,333,696 bytes, sha256 `FE836591...3F24`) matches the zip. All `FHDuels\` folder files were
already the same as the 0.1.5 zip's.

## Result summary
- No crash. The game closed normally, and the Event Viewer Application log has no SkyrimSE.exe
  entry from the last 3 hours.
- **Lock-on turns the camera, but it's slow and later turns away.** The player said the camera
  "did turn to face them pretty slow", and it "did turn away after a few seconds of me moving around
  while locked on". The log matches: the first 5 turns are only `-2` counts at about 5.6 to 5.1
  degrees off (the error went down about 0.1 degrees per frame, so the direction was right). Then
  the direction flipped 3 times, the maximum: 05:59:43.382 (`now -1`), 05:59:43.548 (`now 1`) and
  05:59:46.732 (`now -1`). It ended at `-1`, the opposite of the starting direction, which fits the
  camera turning away. The player was moving around then, so the "turning made it worse" check
  probably fired because of the player's own movement or the enemy circling, not because the
  direction was wrong. After the third flip, nothing can correct it.
- **Block sound works:** the player heard a weapon clang on matched blocks. The log has
  `skyrim sounds: 23 file(s) in 6198 match '/wpn/|block' for block_impact`, with 6 loaded
  (`wpn_block_axe_01/02`, `wpn_block_blade1hand_01/02/03`, `wpn_block_blade2hand_01`), then
  `audio: 6 clip(s) loaded for sound 0` and `sounds: 0 FOR HONOR clip(s), 1 sound(s) from Skyrim`.
  The 6 converted WAVs are in `%LOCALAPPDATA%\FHDuels\sk_block_impact_0..5.wav` (77 to 118 KB, not
  committed). Nothing was written to the game folder. The log has no per-play line, so the log alone
  can't show whether our clip played or Skyrim's own block sound did.
- **Notification:** `notification: FOR HONOR Duels: hold block and flick the mouse to change guard.
  Using Skyrim's block sounds.` at 05:59:27.049, 7.99 s after `hooks: first player update call`
  (05:59:19.062). The player saw it ("FOR HONOR Duels", then something about the block mechanics).
  It fired while the weapon was sheathed (`player duelist: no` at 05:59:23.314), like in run 5.
- **Run 5 features still work:** the player said recoil, guard flicks, the HUD and sheathe-hide all
  worked like before. All 3 `-> blocked` lines are followed by `attacker reaction: recoilLargeStart`.

## Step 6 answers (each asked separately; throwaway save, melee weapon vs. an armed human enemy)
1. **While holding block near an enemy, does the camera turn to face them? Smooth, too slow, too
   fast or wobbly?** "When I held block near an enemy the camera did turn to face them pretty slow."
   (Too slow.)
2. **Does it ever turn away from them?** "Yes." "The camera did turn away after a few seconds of
   me moving around while locked on."
3. **When a matched guard blocks a hit, is there a block sound (a weapon clang)?** "Yes, a clang."
4. **What does the top-left "FOR HONOR Duels" notification say?** "Something like FOR HONOR
   DUELS, and then mentioned the block mechanics." (It matched "hold block and flick the mouse to
   change guard. Using Skyrim's block sounds.")
5. **Do recoil, guard flicks, the HUD and sheathe-hide still work?** "All still worked."

## Log observations (full log in `FHDuels.log`)
- `lock-on:` lines: 5 turn lines (05:59:38.567 to .633, one per frame about 16 ms apart, `-5.6`
  to `-5.1` deg, `turning -2` each time), then 3 `turning made it worse, flipped direction` lines
  (above). After the third flip there are no more lock-on lines, so the log can't show how far off
  the camera got.
- At 2 counts per frame, closing 5 degrees took about 0.1 degrees per frame, so 5 degrees takes
  about 50 frames (close to 1 s). That fits "pretty slow". It looks like the gain is low or the
  output is clamped to a small value close in.
- Hits: 3 `-> blocked` (2 `npc attacks player, guards 2 vs 2`, 1 `player attacks npc, guards 2 vs
  2` at 06:00:03.732), each with `attacker reaction: recoilLargeStart`. 6 `-> landed` with
  mismatched guards (`1 vs 2`, `2 vs 3` x4, `3 vs 2`).
- **New this run:** the first player-to-NPC block happened (`player attacks npc, guards 2 vs 2 ->
  blocked` at 06:00:03.732). The recoil was applied to the player as the attacker. The player didn't
  mention anything odd about it.
- One `melee hit: not a duel (player attacks, ... victim ... weapon not melee)` at 06:00:02.882.
- `melee hit: npc vs npc, not a duel` appears 11 times (other NPCs fighting nearby).
- `input:` lines: `DirectInput mouse hooks A/W`, `mouse device found` once (05:59:08.801), then 3
  `GetDeviceState mouse 0 0` lines.
- `opponent: found` at 05:59:37.549 and `opponent: none` at 06:00:09.598. `overlay: drawing the
  opponent indicator` once.
- Guard changes while guarding are at least 417 ms apart (the closest is 05:59:34.249 Right, then
  05:59:34.666 Left).

## Versions and paths
- Skyrim SE 1.7.104.0: `C:\Program Files (x86)\Steam\steamapps\common\Skyrim Special Edition`.
  Started from Steam (not skse64_loader).
- FOR HONOR (Steam): `C:\Program Files (x86)\Steam\steamapps\common\For Honor`. Only read, never
  started. All 21 packs are still "not a plain AKPK pack". 0 sounds indexed. Step 5 (the sound list)
  was skipped as planned. `%LOCALAPPDATA%\FHDuels\index.csv` is 32 bytes (header only) and isn't
  committed.
- `Data\Skyrim - Sounds.bsa` was only read.
- Ultimate ASI Loader 9.7.4 (`dinput8.dll`), already installed.
- The log is at `%USERPROFILE%\OneDrive\Documents\My Games\Skyrim Special Edition\FHDuels.log`.

## Changes to the game setup
- Replaced `FHDuels.asi` (0.1.4, 2,304,000 bytes, sha256 `9B501D42...CD9D`, now 0.1.5 at
  2,333,696 bytes) and overwrote the `FHDuels\` folder's files with the 0.1.5 zip's (they were
  already the same). Left installed.
- Step 2b from run 2 is still in effect: `SkyCraft.asi.off` and `Plugins.txt.fhduels-backup` are in
  place. `Plugins.txt` had no `*` lines before or after the run, so **Vortex hasn't re-enabled
  anything**. The game rewrote the file at 05:59:08 on start, and the plugins stayed off.
- No environment variables were set, so there was nothing to remove in step 8.

## Suggestions for the cloud session
- Lock-on: raise the gain (or the minimum count near the target) so it turns faster. Fix the flip
  check, because player movement or a circling enemy makes |err| grow even when the direction is
  right. Options: compare the error against how much the camera actually turned (not the total
  error), reset the flip count on a good frame, don't flip while the player is moving, or drop the
  flip and use the sign of the angle difference directly. Also log a sample of turn lines after
  the first 5 (say once a second) so a turn-away shows up in the log.
- Block sounds can be marked working in game on 1.7.104. An `audio: play` line (sound id and clip)
  would show whether our clip played or Skyrim's own block sound did.
- Recoil, flicks, the HUD, sheathe-hide and the notification are still verified.

# Run 5: blocked hits now make the attacker recoil, blocking needs block held, guards are steady, notification shows

Date: 2026-10-08 (game session 05:34 to 05:36 local), local Windows 11 session. Build:
`release/FHDuels-0.1.4.zip` (sha256 `f5818fd8...94a9` matched). The installed `FHDuels.asi`
(2,304,000 bytes, sha256 `9B501D42...CD9D`) and all `FHDuels\` folder files match the zip.

## Result summary
- No crash. The game closed normally, and there's no SkyrimSE.exe entry in the Event Viewer
  Application log from the last 3 hours.
- **Attacker recoil works:** each of the 9 `-> blocked` hits is followed by
  `attacker reaction: recoilLargeStart` at the same timestamp. The first event in the list was
  accepted every time, so the fallbacks weren't needed. The player saw the enemy stagger.
- **Blocking needs block held:** every hit while `player guarding: no` shows a player guard of 0 and
  `-> landed` (`3 vs 0`, `2 vs 0`, `1 vs 0`). The player confirmed that hits always land when not
  blocking.
- **Guard switching is steady:** the closest two flick-driven guard changes are 183 ms apart
  (05:36:09.576 Left, then 05:36:09.759 Right). There is no 50 ms cycling like in run 4, and the
  player said it was steady.
- **Notification shows:** `notification: FOR HONOR Duels: this build has no FOR HONOR sounds chosen
  yet.` at 05:34:55.391, 8.01 s after `hooks: first player update call` (05:34:47.378). The player
  saw it at the top left.

## Step 6 answers (each asked separately; throwaway save, melee weapon vs. an armed human enemy)
1. **Holding block with a matching guard, does the enemy stagger or recoil when they hit?** "Yes,
   they staggered."
2. **Not holding block, do hits always land, even with matching guards?** "Yes, hits always
   landed."
3. **Is guard switching steady (no rapid cycling)?** "Steady."
4. **Does a "FOR HONOR Duels" notification appear at the top left about 8 s after loading?** "Yes,
   it appeared."

## Log observations (full log in `FHDuels.log`)
- Hit lines: 9 `npc attacks player ... -> blocked` (all matched guards while guarding, each with
  `attacker reaction: recoilLargeStart`). There are 11 `npc attacks player ... -> landed`: 5 with
  mismatched guards while guarding (e.g. `1 vs 2`, `3 vs 2`, `2 vs 3`), 5 with player guard 0 while
  not guarding, and 1 `3 vs 0 (victim mid-attack)`.
- **Possibly odd:** all 4 `player attacks npc` hits are `-> landed` against NPC guard 3 (`2 vs 3`
  once, `1 vs 3` three times, 05:36:19 to 05:36:31). In this short fight the NPC's guard stayed on 3
  (Right) every time the player hit it. The player never attacked from the Right, so no
  player-to-NPC block happened this run. The NPC's guard may not change much while it's being hit.
- At 05:35:09.526, `npc attacks player, guards 1 vs 2 -> landed` came while guarding with a
  mismatched guard. That's correct behavior.
- The notification fired while the player's weapon was sheathed (`player duelist: no` at
  05:34:52.725), so it doesn't wait for a duel. It's the "no sounds chosen" message, as expected with
  the encrypted packs.
- `input:` lines: `DirectInput mouse hooks A/W`, `mouse device found` once (05:34:36.499), and 3
  `GetDeviceState mouse 0 0` lines right after. Unlike run 4, there were no later re-acquire lines;
  the player went straight from loading to fighting to quitting.
- `melee hit: npc vs npc, not a duel` appears 11 times in the first fight (other NPCs fighting
  nearby). No `weapon not melee` lines this run.
- `opponent: found` / `opponent: none` bracket both fights (05:35:05 to 05:35:26 and 05:36:05 to
  05:36:31).
- `player guard -> Left` is logged 2 ms after `player guarding: yes` at 05:36:06.19. That looks
  like the guard set at block start, not a flick, so the cooldown doesn't apply there.

## Versions and paths
- Skyrim SE 1.7.104.0: `C:\Program Files (x86)\Steam\steamapps\common\Skyrim Special Edition`.
  Started from Steam (not skse64_loader).
- FOR HONOR (Steam): `C:\Program Files (x86)\Steam\steamapps\common\For Honor`. Only read, never
  started. All 21 packs are still "not a plain AKPK pack". 0 sounds indexed. Step 5 (the sound list)
  was skipped as planned, so there's no new `index.csv`.
- Ultimate ASI Loader 9.7.4 (`dinput8.dll`), already installed.
- The log is at `%USERPROFILE%\OneDrive\Documents\My Games\Skyrim Special Edition\FHDuels.log`.

## Changes to the game setup
- Replaced `FHDuels.asi` (0.1.3, 2,302,976 bytes, sha256 `956F0E07...95CF`, now 0.1.4 at
  2,304,000 bytes) and overwrote the `FHDuels\` folder's files with the ones from the 0.1.4 zip.
  The file list is unchanged. Left installed.
- Step 2b from run 2 is still in effect: `SkyCraft.asi.off` and `Plugins.txt.fhduels-backup` are in
  place. `Plugins.txt` has no `*` lines before or after the run; **Vortex hasn't re-enabled
  anything**. The game rewrote the file at 05:34:36 on start, and the plugins stayed off.
- No environment variables were set this run, so there was nothing to remove in step 8.

## Suggestions for the cloud session
- The four 0.1.4 fixes all work in game. Recoil, blocking only while holding block, the flick
  cooldown and the delayed notification can be marked verified on 1.7.104.
- Check the NPC guard choice: it stayed on Right (3) for every player hit in the second fight.
- The camera lock-on while guarding (asked for in run 4) wasn't part of 0.1.4 and wasn't tested.

# Run 8: enemy guard indicator sits over the torso; user found lock-on and left-guard bugs and asked for a target-switch rule

Date: 2026-10-08 (game session 16:07:33 to 16:10:25 local), local Windows 11 session. Build:
`release/FHDuels-0.1.7.zip` (sha256 `85c1423c...2776` matched). The installed `FHDuels.asi`
(2,334,720 bytes, sha256 `3C51424D...51E6`) matches the zip.

## Setup
- Skyrim SE 1.7.104.0: `C:\Program Files (x86)\Steam\steamapps\common\Skyrim Special Edition`.
- Installed 0.1.7 (with the user's OK) by replacing `FHDuels.asi` and the `FHDuels\` folder (16 files)
  from the zip. Nothing else in the game folder changed.
- Other mods still set aside from earlier runs (`SkyCraft.asi.off`, `Plugins.txt.fhduels-backup`);
  left as is. Step 5 (FOR HONOR sound list) skipped, as asked.
- No screenshot: the user closed the game before reporting.
- No crash. The Event Viewer Application log has no SkyrimSE.exe entry from the last 2 hours.

## Answers (each asked separately; throwaway save, melee weapon vs. armed enemies)
1. **Holding block and locked on, is the enemy indicator over their torso?** "About right"
2. **Does it get in the way of seeing the enemy's attacks?** "No"
3. **Does everything else still work (flicks, blocks, recoil, clang, lock-on, sheathe-hide)?** "All
   worked" (apart from the bugs below).

## Bugs the user reported (unprompted)
Their words: "I couldn't lock on to certain enemies. My block couldn't move to the left for a brief
moment. and could you allow lock on change only when you unblock from your primary target?"

Follow-up answers:
- **Lock-on failure:** holding block near some enemies "didn't turn the camera", and "near towards
  the end the camera almost did a 180 degree turn to lock onto the spellcaster with a dagger".
- **Left guard stuck:** when it happened was "Random / not sure".
- **Feature request:** only switch the lock-on target after the player releases block, not while
  they are holding block on their current target.

## Log observations (full log in `FHDuels.log`, 286 lines)
- **The 180-degree turn is in the log:** `16:10:16.509 player guarding: yes`, then
  `16:10:17.726 lock-on: facing 143.0 deg off` with `opponent: found` in the same tick, so a new
  target was picked mid-hold, almost behind the player. One second later it was 0.8 deg off. At
  16:10:20.011: `melee hit: not a duel (player attacks, victim right hand form 00013986 type 0x29
  anim 2, weapon state 3, weapon not melee)`. Anim 2 looks like a dagger, so the dagger-wielding
  spellcaster is a lock-on target but the hit code treats a dagger as "not melee".
- **Another enemy has a spell in the right hand:** `16:10:10.010 melee hit: not a duel (player
  attacks, victim right hand form 0002DD2A type 0x16 anim 0, ...)`. Type 0x16 is a spell, which
  probably explains some "couldn't lock on" cases.
- **Holding block without any lock-on:** 16:08:41 to 16:09:08 and 16:10:10 to 16:10:16 show several
  `player guarding: yes` holds with no `lock-on:` or `opponent: found` lines (`opponent: none` at
  16:08:40.543 and again at 16:10:20.609).
- **Guard flicks:** 83 `player guard ->` lines. From 16:08:58 to 16:09:08 the guard cycles strictly
  Left -> Top -> Right -> Left every 0.2 to 0.3 s. This is worth checking against the "couldn't move
  left" report (it may be the user testing, or each flick may be read as the next guard in order).
  The log doesn't show a specific left flick being dropped.
- **Blocks:** 10 `-> blocked` lines, each followed in the same millisecond by `audio: played sound 0
  clip N` and `attacker reaction: recoilLargeStart`. 11 `-> landed` hits on the player were guard
  mismatches (for example `guards 1 vs 2`).
- **Lock-on accuracy:** 55 `lock-on:` lines. Most are within 3 deg. Mid-hold outliers: -23.2
  (16:09:28.392, start of a new hold), -13.0, -20.3, -17.3, 10.1; each is back near 0 one second
  later. Learned degrees per count stays 0.041 to 0.075 with no sign flip.
- `overlay: drawing the opponent indicator` was logged at 16:08:16.826. FOR HONOR packs are still
  "not a plain AKPK pack", and the mod falls back to Skyrim's 6 block clips as before.

# MODLOG: FOR HONOR Duels

## Decisions
- **Host:** Skyrim SE/AE, through Ultimate ASI Loader (Melty installs it for every player). There's no
  SKSE dependency, because Melty installs SKSE only on 1.6.1179 and 1.5.97.
- **FOR HONOR:** role "secondary" (not in Melty's catalog). It's online-only with EasyAntiCheat, so the
  mod never launches, injects into or changes it. It only reads the Wwise `.pck` files from disk.
- **Route:** a native ASI plugin with no CommonLib at runtime. Addresses come from meh321's public
  AddressLibraryDatabase (`skyrimae.relib`, AE versions only), resolved per version into
  `sheets/game_addresses.json` and embedded in the plugin. No Address Library files are shipped or
  needed.
- **Toolchain:** mingw-w64 g++ 13 on Linux (the MSVC SDK download from aka.ms is blocked in the cloud
  session). Game calls go through plain function pointers on the Win64 ABI.

## Hooks (all self-checked at startup; nothing is patched if a check fails)
1. **Melee hit:** the E8 call from 38627 (`melee_hit_caller`) to 38586 (`process_hit`). The plugin
   checks the sheet's +0x4A8 hint first, then scans the function for the call. 38586 was identified
   from valhallaCombat's comment address 0x64BAB0, which maps to ID 38586 in 1.6.317/318.
2. **LookHandler::ProcessMouseMove** (vtable 208710, index 3): mouse flicks while blocking set the
   guard, and the camera is frozen.
3. **PlayerCharacter::Update** (vtable 208040, index 0xAD): a per-frame tick on the main thread that
   selects the opponent, publishes the HUD snapshot and flushes notifications.
4. **IDXGISwapChain::Present/ResizeBuffers:** patched through the vtable of a throwaway swap chain, to
   draw the ImGui overlay.
- **Sanity check:** the live player's vtable must equal `vtbl_player_character`, or nothing is patched.

## Verified so far (off-game, 2026-10-08)
- Preflight: 0 blocking, 34 test-pending (every game address and layout row waits on an in-game check,
  and the sound IDs wait on the player's FOR HONOR copy).
- `selftest.exe` under Wine 9: builds a synthetic AKPK (one bank-embedded sound and one streamed
  sound), then checks indexing, the pck filter, missing IDs, CSV output and vgmstream r2117
  conversion. All 7 checks pass.
- `loadtest.exe` under Wine 9: the plugin loads in a non-Skyrim process and logs "not supported".
- **Not tested yet:** anything inside SkyrimSE.exe, WAV playback (Wine has no audio device here), and
  real FOR HONOR packs. One forum report says some packs are encrypted, so the plugin logs those and
  skips them.

## Run 1 (player's PC, 2026-10-07): stopped at the version check
- The player's Skyrim is **1.7.104.0**. The public AddressLibraryDatabase (last updated 2025-05) stops
  at 1.6.1179, but the player has Address Library's `versionlib-1-7-104-0.bin` installed.
- Address Library 1.7.x files are **format 5**: four version ints, a 64-byte exe name, pointer size, a
  zero int, the entry count, then a uint32 offset for every ID starting at ID 0 (0 means no address).
  The header is 96 bytes, and the player's file is exactly 96 + 4 × 565759 bytes. The first 20 entries
  equal the 1.6.1179 offsets from the `.relib`.
- `tools/versionlib_dump.ps1` reads formats 2 and 5 on the player's PC and prints only our 11 offsets.
  Its format 2 reader reproduced every 1.6.1179 offset in the sheet. The 1.7.104 offsets were added
  to `game_addresses.json` from that output. `resolve_offsets.py` keeps versions the `.relib` doesn't
  have.
- Hit hook sanity check: `process_hit`→`melee_hit_caller` is +0x2CB0 and `process_hit`→
  `actor_is_attacking` is +0x12E0 in both 1.6.1179 and 1.7.104, so those functions moved as a block.
- **Unverified for 1.7.104:** the vtable slot indices (LookHandler 3, PlayerCharacter 0xAD) and the
  struct layouts in `layouts.json` (written for 1.6.629+). The plugin now logs each patched vtable
  slot's target RVA, so run 2's log can be checked against Address Library.
- Also seen: `SkyCraft.asi` (another ASI mod) and SKSE built for 1.6.1170, which doesn't match the
  game. Neither is ours, and both were left alone.

## Run 2 (player's PC, 2026-10-07): hooks patched, nothing happened in game
- No crash. All three hooks were patched on 1.7.104, but there was no HUD, no blocking and no log
  line after startup.
- **Root cause (a bug on every version, not just 1.7):** `layouts.json` gave `actor_process` and
  `actor_combat_target` as 0xF0 and 0xFC "relative to actor_runtime". In CommonLibSSE-NG those
  comments are SE absolute offsets, and the runtime block starts at 0xE0. So the real relative
  offsets are 0x10 and 0x1C. The plugin was reading 0xE8 bytes too far, so `IsDuelist` was never
  true. The overlay draws nothing for a non-duelist, and the hit rule returns "not a duel" without
  logging anything. Fixed in 0.1.2.
- Slot check: `vtbl_player_character[0xAD]` points to ID 40447, which is PlayerCharacter::Update's AE
  ID, so that slot is right. `vtbl_look_handler[3]` points to ID 523967, a new 1.7.x ID. The camera
  behaved normally through it, but whether it's ProcessMouseMove is still unconfirmed.
- 0.1.2 logs the first call of each hook (and the first three look-handler mouse deltas), every change
  in the player's duelist, guarding and opponent state (with the right-hand form), and every melee
  hit that isn't treated as a duel.
- **FOR HONOR's `.pck` files on this install aren't plain AKPK.** All 21 start with `C0 4E 1E A2`, so
  0 sounds were indexed. They look encrypted or wrapped. Getting around Ubisoft's encryption is out of
  scope, so the duels run without FOR HONOR sounds until there's a decision on that.
- Documents is redirected to OneDrive on this PC, and the log was found there.

## Run 3 (player's PC, 2026-10-07): duelist detected, mouse hook never fired
- The 0.1.2 offset fix worked: `player duelist: yes (right hand form 00013790 type 0x29 anim 3)`.
  Guarding, an opponent and melee hits were all logged, and the bottom indicator showed up.
- `vtbl_look_handler[3]` (ID 523967) was **never called** on 1.7.104, so guard flicks and the camera
  hold didn't work. 0.1.3 drops the LookHandler hook. The mouse is now read through DirectInput
  (`IDirectInputDevice8::GetDeviceState`/`GetDeviceData`, patched on a throwaway mouse device's A
  and W vtables, the same way the overlay patches Present). It uses no game addresses, and it zeroes
  the axes while the player guards. `vtbl_look_handler` and `mouse_move_x/y` were removed from the
  sheets.
- The indicator stayed up after sheathing, because `IsDuelist` only checked the equipped weapon. Now
  it also needs ActorState `weaponState` >= 3 (ActorState is at +0xC0 on AE 1.6.629+, and
  `actorState2` at +0x0C, bits 5-7). If the ActorState subobject doesn't start with a game vtable,
  the check switches itself off and logs once.
- `opponent: found` was logged, but the player saw no enemy indicator. 0.1.3 logs the first time the
  overlay draws it, to tell a drawing problem apart from the player missing it.
- `player attacks npc, guards 1 vs 1 -> landed` is expected when the NPC is mid-attack (`blocked`
  requires the victim not to be attacking). The log now says "(victim mid-attack)".
- Only 2 duel hits were logged in a whole fight. 0.1.3 logs skipped hits (no aggressor, dead victim)
  and the first NPC-vs-NPC hits, to find where the others went.

## Gotchas
1. Wine's FAudio crashes in CreateMasteringVoice when there's no audio device. The plugin now checks
   for an MMDevice endpoint first.
2. The `.relib` file has two 1.6.1170 entries (`1.6.1170.0` and `1.6.1170.0.1`). Only the first is
   listed. The call-site scan protects the hit hook either way.

## Next steps
1. Run Skyrim 1.6.1179 with the plugin on the user's PC. Check the log for "hooks: hit call at
   +0x4A8", see the indicators on screen, and confirm a matching guard blocks a bandit's attack.
2. Run with FHDUELS_DUMP_INDEX=1, pick block and clash sound IDs from FOR HONOR's packs by ear, and
   fill `sheets/sounds.json`.
3. Mark the sheet rows verified, then run Melty's inspect_package, validate_recipe and
   one_click_check (melty.gg is currently blocked in the cloud session).

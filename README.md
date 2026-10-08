# FOR HONOR Duels (Skyrim Special Edition × FOR HONOR)

FOR HONOR's guard-stance dueling inside Skyrim's melee combat, with FOR HONOR's real block and clash
sounds read from your own copy of FOR HONOR.

**Status: v0.1 is built but not yet tested in game.** It compiles, the sound reader passes an off-game
self-test, and the plugin loads and safely switches itself off outside Skyrim. No one has played it yet.

## Features

- Hold block and flick the mouse up, left or right to choose your guard: Top, Left or Right. The camera
  holds still while you're guarding, as in FOR HONOR.
- Your guard is also the direction you attack from.
- Every armed melee enemy (bandits, draugr, guards and so on) also takes a guard and switches between
  them as it fights you.
- An attack is blocked when the defender's guard matches the attack's direction. The hit does no damage
  and the attacker recoils. If the guards don't match, or the defender is mid-attack, the hit lands
  normally.
- A FOR HONOR-style indicator shows your guard (bottom) and your opponent's guard (top). It glows while
  you're guarding or while your opponent is attacking.
- FOR HONOR's own block and clash sounds play on each exchange. They're read from your FOR HONOR install
  (Steam or Ubisoft Connect) and converted on your PC. If FOR HONOR isn't installed, duels still work
  without its sounds.
- It's single-player. Creatures, bows, magic and fights between NPCs keep vanilla Skyrim behavior.

## Requirements

- The Elder Scrolls V: Skyrim Special Edition / Anniversary Edition, version 1.6.640, 1.6.659, 1.6.1130,
  1.6.1170, 1.6.1179 or 1.7.104. 1.5.97 isn't supported yet.
- FOR HONOR installed on the same PC, for its sounds. Its files are only read: FOR HONOR is never
  started or changed, so your Ubisoft account isn't touched.
- Ultimate ASI Loader, which Melty installs.

## How it is built

The design lives in `sheets/*.json`, one row per thing and one column per property. `tools/gen.py`
turns the sheets into `src/generated/sheets.h`, and `tools/preflight.py` lists every unfilled cell,
broken reference and unused row before each build.

```
./build.sh          # gen + preflight + dist/FHDuels.asi (mingw-w64)
./build.sh test     # also builds build/selftest.exe (run it with wine64 or on Windows)
./package.sh 0.1.2  # dist/FHDuels-0.1.2.zip
python3 tools/resolve_offsets.py skyrimae.relib   # refresh per-version offsets
```

Logs are written to `Documents/My Games/Skyrim Special Edition/FHDuels.log`.

### Choosing FOR HONOR's sounds

`sheets/sounds.json` lists which FOR HONOR sound IDs to use. To pick them, start Skyrim once with the
environment variable `FHDUELS_DUMP_INDEX=1`. Every sound in your FOR HONOR install is then listed in
`%LOCALAPPDATA%\FHDuels\index.csv`.

## Credits

Made with Claude (AI). The author credit and content licence are still to be chosen. Third-party components are listed in
`docs/THIRD_PARTY.md`.

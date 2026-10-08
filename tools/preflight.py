"""Preflight: overlay every sheet's rows x columns and list what is unfilled, unresolved or unimplemented.

Exit code 0 = buildable (no BLOCKING items). TEST-PENDING items (verified_in_game=false, sound IDs
that can only be picked from the player's FOR HONOR copy) don't block a build but block a release.
Usage: python3 tools/preflight.py [--release]
"""
import glob
import json
import os
import re
import sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
SHEETS = os.path.join(ROOT, "sheets")
SRC = os.path.join(ROOT, "src")


def load():
    return {os.path.basename(p)[:-5]: json.load(open(p)) for p in sorted(glob.glob(os.path.join(SHEETS, "*.json")))}


def empty(v):
    return v is None or v == "" or v == [] or v == {}


def main():
    release = "--release" in sys.argv
    s = load()
    blocking, pending = [], []

    # 1. every cell filled
    for name, sheet in s.items():
        cols = sheet["columns"]
        ids = set()
        for row in sheet["rows"]:
            rid = row.get("id", "?")
            if rid in ids:
                blocking.append(f"{name}.{rid}: duplicate id")
            ids.add(rid)
            for extra in set(row) - set(cols):
                blocking.append(f"{name}.{rid}: column '{extra}' not declared")
            for c in cols:
                v = row.get(c)
                if c == "verified_in_game":
                    if v is not True:
                        pending.append(f"{name}.{rid}: not yet verified in the running game")
                    continue
                if name == "sounds" and c == "skyrim_match" and empty(v):
                    continue  # optional: no Skyrim fallback for this row (landed hits already sound in vanilla)
                if name == "sounds" and c in ("fh_pck", "wem_ids") and empty(v):
                    pending.append(f"sounds.{rid}.{c}: pick from the player's FOR HONOR install (FHDUELS_DUMP_INDEX=1)")
                    continue
                if empty(v):
                    blocking.append(f"{name}.{rid}.{c}: unfilled")
                elif isinstance(v, dict):
                    for k, sub in v.items():
                        if empty(sub):
                            blocking.append(f"{name}.{rid}.{c}[{k}]: unfilled")

    # 2. references between sheets
    idx = {n: {r["id"] for r in sh["rows"]} for n, sh in s.items()}
    for r in s["rules"]["rows"]:
        if r["sound"] != "none" and r["sound"] not in idx["sounds"]:
            blocking.append(f"rules.{r['id']}.sound -> sounds.{r['sound']} does not exist")
    for r in s["sounds"]["rows"]:
        if r["used_by_rule"] not in idx["rules"]:
            blocking.append(f"sounds.{r['id']}.used_by_rule -> rules.{r['used_by_rule']} does not exist")
    for r in s["hud"]["rows"]:
        if r["shows"] not in ("player", "opponent"):
            blocking.append(f"hud.{r['id']}.shows must be player|opponent")
    primary = [g for g in s["games"]["rows"] if g["role"] == "primary"]
    if len(primary) != 1:
        blocking.append("games: exactly one primary game required")
    else:
        for r in s["game_addresses"]["rows"]:
            for ver in primary[0]["versions_supported"]:
                if empty(r["offsets"].get(ver)):
                    blocking.append(f"game_addresses.{r['id']}.offsets[{ver}]: unresolved (run tools/resolve_offsets.py)")
        for r in s["game_addresses"]["rows"]:
            if r["kind"] == "call_site" and r["call_offset_hint"] == "n/a":
                blocking.append(f"game_addresses.{r['id']}: call_site needs call_offset_hint")
            if r["kind"] == "vtable" and r["vfunc_index"] == "n/a":
                blocking.append(f"game_addresses.{r['id']}: vtable needs vfunc_index")

    # 3. every row is implemented: generated name used in hand-written source
    code = ""
    for p in glob.glob(os.path.join(SRC, "*.cpp")) + glob.glob(os.path.join(SRC, "*.h")):
        code += open(p, encoding="utf-8").read()
    uses = {"tunables": "tun::{}", "game_addresses": "addr::{}", "layouts": "lay::{}", "rules": "Rule::{}",
            "hud": "hud::{}"}
    tables = {"guards": "kGuards", "sounds": "kSounds", "rules": "kRules"}  # consumed row-by-row as tables
    if not code:
        blocking.append("src/: no source yet")
    else:
        for sheet, pat in uses.items():
            for r in s[sheet]["rows"]:
                name = pat.format(r["id"])
                if not re.search(re.escape(name) + r"\b", code):
                    blocking.append(f"{sheet}.{r['id']}: not used in src/ (expected {name})")
        for sheet, table in tables.items():
            if not re.search(r"\b" + table + r"\b", code):
                blocking.append(f"{sheet}: table {table} not used in src/")

    print(f"PREFLIGHT  sheets={len(s)}  rows={sum(len(x['rows']) for x in s.values())}")
    for b in blocking:
        print("  BLOCKING     ", b)
    for p in pending:
        print("  TEST-PENDING ", p)
    print(f"  {len(blocking)} blocking, {len(pending)} test-pending")
    sys.exit(1 if blocking or (release and pending) else 0)


if __name__ == "__main__":
    main()

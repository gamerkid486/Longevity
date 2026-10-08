"""Fill game_addresses.offsets from meh321's AddressLibraryDatabase (skyrimae.relib).

Usage: python3 tools/resolve_offsets.py <path/to/skyrimae.relib>
Writes sheets/game_addresses.json in place, one offset per supported version (games.json).
"""
import json
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
import relib  # noqa: E402

ROOT = os.path.join(os.path.dirname(__file__), "..", "sheets")


def main(relib_path):
    games = json.load(open(os.path.join(ROOT, "games.json")))
    versions = next(g for g in games["rows"] if g["role"] == "primary")["versions_supported"]
    _, libs = relib.load(relib_path)
    by_name = {".".join(map(str, v)): vals for v, (_, vals) in libs.items()}
    path = os.path.join(ROOT, "game_addresses.json")
    sheet = json.load(open(path))
    for row in sheet["rows"]:
        row["offsets"] = {}
        for ver in versions:
            off = by_name.get(ver, {}).get(row["ae_id"])
            row["offsets"][ver] = hex(off) if off is not None else ""
    json.dump(sheet, open(path, "w"), indent=2)
    print("resolved", len(sheet["rows"]), "rows x", len(versions), "versions")


if __name__ == "__main__":
    main(sys.argv[1])

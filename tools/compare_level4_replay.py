#!/usr/bin/env python3
"""Compare a headless MAME Stage 4 capture with the native replay output.

    python3 tools/compare_level4_replay.py capture.csv native.csv

Both files carry one row per board frame with identical columns.  Rows are
matched on "rel" (the stage-relative frame) and only the frames in which the
board is actually playing are compared: the wipe and the tally run on the
cabinet's own phase machine, which the native game does not reproduce.
"""
import csv
import sys
from collections import defaultdict

# The emulator's own bookkeeping, or values the native build has no reason to
# reproduce: the absolute frame number, the raw port read and the free-running
# frame byte.
IGNORED = {"frame", "input", "phase05", "phase06", "frame_byte", "dips",
           "visits"}


def load(path):
    with open(path, newline="") as handle:
        rows = {}
        for row in csv.DictReader(handle):
            rel = int(row["rel"])
            if rel < 0:
                continue
            rows.setdefault(rel, row)
        return rows


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        return 2
    mame = load(sys.argv[1])
    native = load(sys.argv[2])
    shared = sorted(set(mame) & set(native))
    if not shared:
        print("the two files share no stage frames")
        return 1
    columns = [c for c in mame[shared[0]] if c not in IGNORED]
    mismatches = defaultdict(list)
    checked = 0
    for rel in shared:
        left, right = mame[rel], native[rel]
        if left["phase06"] != "3" or left["state"] == "0":
            continue
        checked += 1
        for column in columns:
            if left.get(column) != right.get(column):
                mismatches[column].append((rel, left.get(column),
                                           right.get(column)))
    for column, rows in sorted(mismatches.items(),
                               key=lambda item: -len(item[1])):
        rel, want, got = rows[0]
        print(f"{column}: {len(rows)} mismatches, first at frame {rel}: "
              f"mame={want} native={got}")
    print(f"checked {checked} frames, {len(mismatches)} mismatching columns")
    return 1 if mismatches else 0


if __name__ == "__main__":
    sys.exit(main())

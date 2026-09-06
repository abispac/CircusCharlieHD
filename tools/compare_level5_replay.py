#!/usr/bin/env python3
"""Compare a headless MAME Event 5 capture with the native replay output.

    python3 tools/compare_level5_replay.py capture.csv native.csv

Rows are matched on "rel"; frames in which the board is not playing (the wipe
and the tally) are skipped, and the comparison ends at the wipe after the goal.
"""
import csv
import sys
from collections import defaultdict

IGNORED = {"frame", "input", "phase05", "phase06", "frame_byte", "dips", "v2212", "stage"}


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
        if left["phase06"] != "3":
            continue
        if left.get("stage") not in (None, "4"):
            break   # the next stage has begun
        checked += 1
        for column in columns:
            if left.get(column) != right.get(column):
                mismatches[column].append((rel, left.get(column), right.get(column)))
    for column, rows in sorted(mismatches.items(), key=lambda i: -len(i[1])):
        rel, want, got = rows[0]
        print(f"{column}: {len(rows)} mismatches, first at rel {rel}: "
              f"mame={want} native={got}")
    print(f"checked {checked} frames, {len(mismatches)} mismatching columns")
    return 1 if mismatches else 0


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""Compare a headless MAME Event 2 capture with the native replay output.

    python3 tools/compare_level2_replay.py capture.csv native.csv

Both files carry one row per board frame with identical columns.  Rows are
matched on "rel" (the stage-relative frame) and only the frames in which the
board is actually playing are compared: the wipe and the tally run on the
cabinet's own phase machine, which the native game does not reproduce.
"""
import csv
import sys
from collections import defaultdict

IGNORED = {"frame", "input", "phase05", "phase06", "frame_byte", "dips",
           "visits", "code", "schedptr"}
IGNORED.update({f"mk{i}_code" for i in range(7)})


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
    ended = False
    for rel in shared:
        left, right = mame[rel], native[rel]
        if left["state"] == "4":
            ended = True
        if left["phase06"] != "3":
            # The wipe after the goal ends the comparison: the cabinet then
            # starts the next event, which the native replay does not follow.
            if ended:
                break
            continue
        if left["state"] == "0":
            continue
        checked += 1
        for column in columns:
            if left.get(column) != right.get(column):
                mismatches[column].append((rel, left.get(column),
                                           right.get(column)))
    for column, rows in sorted(mismatches.items(), key=lambda i: -len(i[1])):
        rel, want, got = rows[0]
        print(f"{column}: {len(rows)} mismatches, first at frame {rel}: "
              f"mame={want} native={got}")
    print(f"checked {checked} frames, {len(mismatches)} mismatching columns")
    return 1 if mismatches else 0


if __name__ == "__main__":
    sys.exit(main())

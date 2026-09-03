#!/usr/bin/env python3
"""Compare a native Level 3 replay (--replay-event 3 --replay-output) with the
MAME capture it replayed (tools/autoplay_level3_headless.lua).

Rows are matched by the MAME-relative frame ("rel"); every column except the
frame counters, the raw input byte and the board phase bytes must agree.
Restart phases (phase06 5/6/0/1/2 in the capture) are skipped: the native
game runs its own 96-frame restart scene there."""
import csv
import sys

IGNORED = {"frame", "input", "phase05", "phase06", "frame_byte"}


def load(path):
    rows = {}
    with open(path, newline="") as handle:
        for row in csv.DictReader(handle):
            if not row.get("rel") or row["rel"].startswith("#"):
                continue
            rows[int(row["rel"])] = row
    return rows


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    mame = load(sys.argv[1])
    native = load(sys.argv[2])
    limit = int(sys.argv[3]) if len(sys.argv) > 3 else 20
    checked = 0
    mismatched = 0
    first = None
    columns = None
    for rel in sorted(native):
        if rel not in mame or rel < 0:
            continue
        reference = mame[rel]
        candidate = native[rel]
        # Only playing frames: the restart phases and the frame that runs
        # $8C61 again (state 0) belong to the board's own transition.
        if reference.get("phase06") != "3" or reference.get("state") == "0":
            continue
        if columns is None:
            columns = [c for c in reference if c in candidate and c not in IGNORED]
        diffs = [c for c in columns if reference[c] != candidate[c]]
        checked += 1
        if diffs:
            mismatched += 1
            if first is None:
                first = rel
            if mismatched <= limit:
                print(f"rel {rel}: " + "; ".join(
                    f"{c}: mame={reference[c]} native={candidate[c]}" for c in diffs))
    print(f"checked {checked} frames, {mismatched} mismatching"
          + (f", first at rel {first}" if first is not None else ""))
    return 1 if mismatched else 0


if __name__ == "__main__":
    sys.exit(main())

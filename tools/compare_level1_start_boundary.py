#!/usr/bin/env python3
"""Compare native start-line object motion with deterministic MAME traces."""

import argparse
import csv


def rows(path):
    with open(path, newline="", encoding="utf-8") as handle:
        return list(csv.DictReader(handle))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("mame")
    parser.add_argument("native")
    parser.add_argument("--frames", type=int, default=120)
    parser.add_argument("--mame-first-native-frame", type=int, default=1325)
    parser.add_argument("--native-skip", type=int, default=0)
    parser.add_argument(
        "--delta-only",
        action="store_true",
        help="compare consecutive 8.8 motion deltas instead of absolute X",
    )
    args = parser.parse_args()
    mame = {int(row["frame"]): row for row in rows(args.mame)}
    native = rows(args.native)
    first = None
    compared = 0
    previous_expected = None
    previous_actual = None
    for row in native[args.native_skip : args.native_skip + args.frames]:
        native_frame = int(row["frame"])
        source_frame = (
            args.mame_first_native_frame + native_frame - args.native_skip
        )
        if source_frame not in mame:
            continue
        expected = int(mame[source_frame]["hoop_26d0_x"], 16)
        actual = int(row["hoop_x_8_8"])
        if args.delta_only:
            if previous_expected is None:
                previous_expected = expected
                previous_actual = actual
                continue
            expected_delta = (expected - previous_expected) & 0xFFFF
            actual_delta = (actual - previous_actual) & 0xFFFF
            previous_expected = expected
            previous_actual = actual
            expected = expected_delta
            actual = actual_delta
        compared += 1
        if expected != actual and first is None:
            first = (native_frame, source_frame, expected, actual)
    if first:
        native_frame, source_frame, expected, actual = first
        raise SystemExit(
            f"first divergence native {native_frame} / MAME {source_frame}: "
            f"hoop {'delta ' if args.delta_only else ''}expected "
            f"{expected:04x}, native {actual:04x}"
        )
    comparison = "deltas" if args.delta_only else "positions"
    print(
        f"start-boundary hoop {comparison} synchronized for {compared} frames"
    )


if __name__ == "__main__":
    main()

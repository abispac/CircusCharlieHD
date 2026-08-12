#!/usr/bin/env python3
"""Compare ROM movement commands with a deterministic native Level 1 trace."""

import argparse
import csv


SOURCE_TO_WORLD_X = 480.0 / 224.0
TOLERANCE = 0.0001


def signed16(high: str, low: str) -> int:
    value = (int(high, 16) << 8) | int(low, 16)
    return value - 0x10000 if value & 0x8000 else value


def load(path: str):
    with open(path, newline="", encoding="utf-8") as handle:
        return list(csv.DictReader(handle))


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("mame")
    parser.add_argument("native")
    args = parser.parse_args()
    mame = load(args.mame)
    native = load(args.native)

    # MAME input changes appear in the trace one frame after Lua schedules
    # them. Align both traces at the first effective directional command.
    start = next(
        index
        for index, row in enumerate(mame)
        if (row["input_left"] == "1" or row["input_right"] == "1")
        and (row["ram_20b1"] != "00" or row["ram_20b2"] != "00")
    )

    compared = 0
    for native_index, native_row in enumerate(native):
        if start + native_index >= len(mame):
            break
        mame_row = mame[start + native_index]
        raw_delta = signed16(mame_row["ram_20b1"], mame_row["ram_20b2"])
        expected_world_delta = -(raw_delta / 256.0) * SOURCE_TO_WORLD_X
        actual_world_delta = float(native_row["delta_x"])
        if abs(expected_world_delta - actual_world_delta) > TOLERANCE:
            print(
                "first divergence: native frame "
                f"{native_row['frame']}, MAME frame {mame_row['frame']}; "
                f"expected dx={expected_world_delta:.6f}, "
                f"native dx={actual_world_delta:.6f}"
            )
            return 1
        compared += 1

    jump_rows = [index for index, row in enumerate(native) if row["input_jump"] == "1"]
    if jump_rows:
        native_jump = jump_rows[0]
        mame_ground = int(mame[start + native_jump]["rider_25f0_y"], 16)
        mame_jump = next(
            index
            for index in range(start + native_jump, len(mame))
            if int(mame[index]["rider_25f0_y"], 16) != mame_ground
        )
        native_ground = float(native[native_jump - 1]["player_y"])
        jump_samples = 0
        while native_jump < len(native) and mame_jump < len(mame):
            expected_y = native_ground - (
                mame_ground - int(mame[mame_jump]["rider_25f0_y"], 16)
            ) * (640.0 / 256.0)
            actual_y = float(native[native_jump]["player_y"])
            if abs(expected_y - actual_y) > TOLERANCE:
                print(
                    "first jump divergence: native frame "
                    f"{native[native_jump]['frame']}, MAME frame "
                    f"{mame[mame_jump]['frame']}; expected y={expected_y:.6f}, "
                    f"native y={actual_y:.6f}"
                )
                return 1
            jump_samples += 1
            if native[native_jump]["grounded"] == "1":
                break
            native_jump += 1
            mame_jump += 1
        print(f"jump arc synchronized for {jump_samples} frames")

    print(f"movement command synchronized for {compared} frames")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

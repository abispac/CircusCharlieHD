#!/usr/bin/env python3
"""Report the first MAME/native state difference at the Event 1 failure."""

import argparse
import csv


PLAYING_SCENE = 3
SOURCE_TO_WORLD_X = 480.0 / 224.0


def load(path: str):
    with open(path, newline="", encoding="utf-8") as handle:
        return list(csv.DictReader(handle))


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("mame_collision")
    parser.add_argument("mame_objects")
    parser.add_argument("native")
    parser.add_argument("--native-frame-zero", type=int, default=1322)
    args = parser.parse_args()

    collisions = load(args.mame_collision)
    objects = load(args.mame_objects)
    native = load(args.native)
    failure = next(
        row for row in collisions
        if row["pc"] == "7c4d" and row["u"] == "26d0"
    )

    # The state write occurs during logical frame 1389. MAME displays the
    # newly buffered sprite bank on the following frame.
    mame_visible_frame = int(failure["frame"]) + 1
    native_frame = mame_visible_frame - args.native_frame_zero
    native_row = next(row for row in native if int(row["frame"]) == native_frame)

    # The expanded trace can expose an object-state disagreement earlier than
    # the old rider-Y-only comparator. Object $26d0 uses source X=0x40 as the
    # rider collision axis in $7130-$7192.
    for object_row in objects:
        if object_row["slot"] != "26d0" or object_row["b00"] == "00":
            continue
        object_frame = int(object_row["frame"])
        aligned_native_frame = object_frame - args.native_frame_zero
        aligned_native = next(
            (row for row in native if int(row["frame"]) == aligned_native_frame),
            None,
        )
        if aligned_native is None:
            continue
        if object_frame < mame_visible_frame and int(
            aligned_native["scene"]
        ) != PLAYING_SCENE:
            print(
                "first expanded-state divergence: native frame "
                f"{aligned_native_frame}, MAME frame {object_frame}; "
                "MAME remains Playing but native has already entered "
                f"scene={aligned_native['scene']}"
            )
            return 1
        object_x = int(object_row["x_hi"], 16) + (
            int(object_row["x_lo"], 16) / 256.0
        )
        expected_relative_x = (object_x - 0x40) * SOURCE_TO_WORLD_X
        player_screen_x = (
            float(aligned_native["player_x"])
            - float(aligned_native["camera_x"])
            + 20.0
        )
        actual_relative_x = (
            float(aligned_native["hoop_screen_x"]) - player_screen_x
        )
        if abs(expected_relative_x - actual_relative_x) > 0.0001:
            print(
                "first expanded-state divergence: native frame "
                f"{aligned_native_frame}, MAME frame {object_frame}; "
                f"expected active-hoop relative x={expected_relative_x:.6f}, "
                f"native relative x={actual_relative_x:.6f}"
            )
            return 1

    if int(native_row["scene"]) == PLAYING_SCENE:
        print(
            "first state divergence: native frame "
            f"{native_frame}, MAME frame {mame_visible_frame}; "
            "MAME entered hoop failure via $7c47 with "
            f"U=${failure['u'].upper()}, rider_y=${failure['rider_y'].upper()}, "
            f"object_x=${failure['object_x'].upper()}, but native remains Playing; "
            f"native hoop_screen_x={float(native_row['hoop_screen_x']):.6f}, "
            f"overlap={native_row['hoop_overlap']}"
        )
        return 1

    expected_y = 550.0
    actual_y = float(native_row["player_y"])
    if abs(expected_y - actual_y) > 0.0001:
        print(
            f"failure pose divergence at native frame {native_frame}: "
            f"expected y={expected_y:.6f}, native y={actual_y:.6f}"
        )
        return 1

    print(
        f"failure transition synchronized at native frame {native_frame} / "
        f"MAME frame {mame_visible_frame}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

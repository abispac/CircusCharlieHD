#!/usr/bin/env python3
"""Compare the deterministic successful-hoop MAME and native traces."""

import argparse
import csv


SOURCE_TO_LOGICAL_Y = 640.0 / 256.0
GROUND_Y = 590.0
MAME_FIRST_NATIVE_FRAME = 1322


def load(path: str):
    with open(path, newline="", encoding="utf-8") as handle:
        return list(csv.DictReader(handle))


def source_y(native_row) -> int:
    return round(float(native_row["player_y"]) / SOURCE_TO_LOGICAL_Y) - 28


def source_x_fixed(native_row) -> int:
    screen_x = float(native_row["hoop_screen_x"])
    source_x = 64.0 + (screen_x - 98.0) / (480.0 / 224.0)
    return round(source_x * 256.0) & 0xFFFF


def collision_result(native_row) -> str:
    hoop_x = source_x_fixed(native_row) >> 8
    distance = abs(hoop_x - 0x40)
    if distance >= 0x0E:
        return "safe_x"
    rider_y = source_y(native_row)
    if rider_y < 0xB6:
        return "safe_above"
    if rider_y - 0xB6 + distance > 0x1C:
        return "safe_boundary"
    return "failure"


def bcd_score(value: str) -> int:
    return int(value, 10)


def mame_rider_state(row) -> str:
    code = int(row["rider0_code"], 16)
    return {0x62: "A", 0xCD: "B", 0x5C: "C"}.get(code, "?")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("mame")
    parser.add_argument("native")
    parser.add_argument("--comparison-out", required=True)
    args = parser.parse_args()
    mame = {int(row["frame"]): row for row in load(args.mame)}
    native = load(args.native)

    columns = [
        "native_frame", "mame_frame", "mame_rider_y", "native_rider_y",
        "mame_hoop_x_8_8", "native_hoop_x_8_8", "mame_collision",
        "native_collision", "mame_landing", "native_landing",
        "mame_score_event", "native_score_event", "match",
        "mame_rider_state", "native_rider_state", "native_hd_frame",
        "native_anchor_x", "native_anchor_y",
    ]
    first_divergence = None
    with open(args.comparison_out, "w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=columns, lineterminator="\n")
        writer.writeheader()
        for native_row in native:
            native_frame = int(native_row["frame"])
            mame_frame = MAME_FIRST_NATIVE_FRAME + native_frame
            if mame_frame not in mame:
                continue
            mame_row = mame[mame_frame]
            native_landing = native_row["landing_transition"] == "1"
            native_score_event = int(native_row["hoop_score_event"])
            expected_state = mame_rider_state(mame_row)
            expected_hd_frame = {"A": 1, "B": 2, "C": 3}[expected_state]
            expected_anchor = (512.0, 640.0)
            checks = {
                "rider_y": int(mame_row["rider_source_y"], 16) == source_y(native_row),
                "hoop_x": int(mame_row["hoop_x_8_8"], 16) == source_x_fixed(native_row),
                "collision": mame_row["collision_result"] == collision_result(native_row),
                "landing": (mame_row["landing_transition"] == "1") == native_landing,
                "score": bcd_score(mame_row["score_event_bcd"]) == native_score_event,
                "rider_state": native_row["rider_animation_state"] == expected_state,
                "hd_frame": int(native_row["rider_hd_frame"]) == expected_hd_frame,
                "anchor": (
                    abs(float(native_row["rider_anchor_x"]) - expected_anchor[0]) < 0.01
                    and abs(float(native_row["rider_anchor_y"]) - expected_anchor[1]) < 0.01
                ),
            }
            match = all(checks.values())
            if not match and first_divergence is None:
                first_divergence = (native_frame, mame_frame, checks)
            writer.writerow({
                "native_frame": native_frame,
                "mame_frame": mame_frame,
                "mame_rider_y": mame_row["rider_source_y"],
                "native_rider_y": f"{source_y(native_row):02x}",
                "mame_hoop_x_8_8": mame_row["hoop_x_8_8"],
                "native_hoop_x_8_8": f"{source_x_fixed(native_row):04x}",
                "mame_collision": mame_row["collision_result"],
                "native_collision": collision_result(native_row),
                "mame_landing": mame_row["landing_transition"],
                "native_landing": int(native_landing),
                "mame_score_event": mame_row["score_event_bcd"],
                "native_score_event": f"{native_score_event:06x}",
                "mame_rider_state": expected_state,
                "native_rider_state": native_row["rider_animation_state"],
                "native_hd_frame": native_row["rider_hd_frame"],
                "native_anchor_x": native_row["rider_anchor_x"],
                "native_anchor_y": native_row["rider_anchor_y"],
                "match": int(match),
            })

    if first_divergence:
        native_frame, mame_frame, checks = first_divergence
        failed = ", ".join(name for name, value in checks.items() if not value)
        print(f"first divergence: native {native_frame} / MAME {mame_frame}: {failed}")
        return 1
    print("successful large-hoop sequence synchronized through comparison window")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

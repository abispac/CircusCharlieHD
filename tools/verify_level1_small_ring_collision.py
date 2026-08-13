#!/usr/bin/env python3
"""Emit the exact $7130-$7192 small-ring collision branch truth table."""

from __future__ import annotations

import argparse
import csv
from pathlib import Path


CASES = (
    ("A_center_opening", 0x40, 0x95),
    ("B_upper_fire", 0x40, 0xA6),
    ("C_lower_fire", 0x40, 0xC0),
    ("D_leading_edge", 0x33, 0xAF),
    ("E_trailing_edge", 0x4D, 0xAF),
    ("F_grounded_under", 0x40, 0xD0),
)


def evaluate(hoop_x: int, rider_y: int) -> dict[str, int | str]:
    distance = abs(hoop_x - 0x40)
    if distance >= 0x0E:
        return {
            "horizontal": distance,
            "adjusted_y": rider_y,
            "vertical": -1,
            "sum": -1,
            "path": "7137->7139 BCC $7195",
            "result": "safe_x",
        }
    adjusted = rider_y + 0x10
    vertical = adjusted - 0xB6
    if vertical < 0:
        return {
            "horizontal": distance,
            "adjusted_y": adjusted,
            "vertical": vertical,
            "sum": vertical + distance,
            "path": "714B +$10; 714D -$B6; 714F BMI; 7151==$2760; 7157 reward",
            "result": "safe_opening_reward",
        }
    combined = vertical + distance
    if combined <= 0x1C:
        result = "failure"
        path = "714B +$10; 714D -$B6; 718C add |X-$40|; 7190 <=$1C; 7192 $7C47"
    else:
        result = "safe_below_boundary"
        path = "714B +$10; 714D -$B6; 718C add |X-$40|; 7190 BHI $71C8"
    return {
        "horizontal": distance,
        "adjusted_y": adjusted,
        "vertical": vertical,
        "sum": combined,
        "path": path,
        "result": result,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", newline="") as handle:
        fields = (
            "case", "object_pointer", "hoop_x", "rider_y_2644",
            "horizontal_distance", "adjusted_rider_y", "vertical_distance",
            "combined", "instruction_path", "result",
        )
        writer = csv.DictWriter(handle, fieldnames=fields)
        writer.writeheader()
        for name, hoop_x, rider_y in CASES:
            result = evaluate(hoop_x, rider_y)
            writer.writerow({
                "case": name,
                "object_pointer": "2760",
                "hoop_x": f"{hoop_x:02x}",
                "rider_y_2644": f"{rider_y:02x}",
                "horizontal_distance": f"{result['horizontal']:02x}",
                "adjusted_rider_y": f"{result['adjusted_y'] & 0xff:02x}",
                "vertical_distance": f"{result['vertical'] & 0xff:02x}",
                "combined": f"{result['sum'] & 0xff:02x}",
                "instruction_path": result["path"],
                "result": result["result"],
            })
    print(f"wrote {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""Reduce the complete MAME Level 1 capture to reviewable object evidence."""

from __future__ import annotations

import argparse
import csv
from pathlib import Path


HOOPS = ("26d0", "2700", "2730", "2760")
POTS = ("24b0", "24f0", "2530")


def active_signature(row: dict[str, str]) -> str:
    tracked = row["tracked_object"].lower()
    parts: list[str] = []
    for address in HOOPS:
        if row[f"hoop_{address}_status"] == "00":
            continue
        kind = "small" if address == "2760" else "large"
        if tracked == address:
            kind = "extra-charlie"
        parts.append(f"{kind}[{address} x={row[f'hoop_{address}_x']}]")
    for address in POTS:
        if row[f"pot_{address}_status"] != "00":
            parts.append(f"pot[{address} x={row[f'pot_{address}_x']}]")
    if row["coin_2570_status"] != "00":
        parts.append(f"coin[2570 x={row['coin_2570_x']}]")
    return " ".join(parts) or "none"


def object_states(row: dict[str, str]) -> tuple[str, ...]:
    return tuple(
        row[field]
        for field in (
            *(f"hoop_{address}_status" for address in HOOPS),
            *(f"pot_{address}_status" for address in POTS),
            "coin_2570_status",
            "extra_charlie_state",
            "tracked_object",
            "prize_state",
        )
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("frames", type=Path)
    parser.add_argument("events", type=Path)
    parser.add_argument("signatures", type=Path)
    parser.add_argument("--first", type=int, default=943)
    parser.add_argument("--last", type=int, default=3040)
    args = parser.parse_args()

    with args.frames.open(newline="") as source:
        rows = [
            row
            for row in csv.DictReader(source)
            if args.first <= int(row["frame"]) <= args.last
        ]

    args.events.parent.mkdir(parents=True, exist_ok=True)
    with args.events.open("w", newline="") as target:
        fieldnames = (
            "frame",
            "course_index",
            "course_state",
            "course_offset",
            "activation_8_8",
            "board_frame",
            "extra_charlie_state",
            "prize_state",
            "active_signature",
        )
        writer = csv.DictWriter(target, fieldnames=fieldnames)
        writer.writeheader()
        previous: tuple[str, ...] | None = None
        for row in rows:
            state = object_states(row)
            if state == previous:
                continue
            writer.writerow(
                {
                    "frame": row["frame"],
                    "course_index": row["course_index"],
                    "course_state": row["course_state"],
                    "course_offset": row["course_offset"],
                    "activation_8_8": row["activation_hi"]
                    + row["activation_lo"],
                    "board_frame": row["board_frame"],
                    "extra_charlie_state": row["extra_charlie_state"],
                    "prize_state": row["prize_state"],
                    "active_signature": active_signature(row),
                }
            )
            previous = state

    with args.signatures.open("w", newline="") as target:
        writer = csv.writer(target)
        writer.writerow(("frame", "active_signature"))
        for row in rows:
            writer.writerow((row["frame"], active_signature(row)))

    print(f"wrote {args.events}")
    print(f"wrote {args.signatures}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

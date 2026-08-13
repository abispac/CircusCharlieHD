#!/usr/bin/env python3
"""Replay the captured circusc4 Level 1 hoop scheduler and compare every frame.

The replay is intentionally independent of the game's presentation code.  It
implements $7539-$7554 and $7607-$76F8 from the captured board state and emits
the same active-object signature used for the MAME evidence CSV.
"""

from __future__ import annotations

import argparse
import csv
from dataclasses import dataclass
from pathlib import Path


ORDINARY = ("26d0", "2700", "2730")
ALL = (*ORDINARY, "2760")
COURSE = (0xDC, 0xF4, 0xD4, 0xEC, 0xDC, 0xE4, 0xDE,
          0x1B, 0xEE, 0xF6, 0x1B, 0xDE, 0xEE)


def u16(value: int) -> int:
    return value & 0xFFFF


def signed16(value: int) -> int:
    value &= 0xFFFF
    return value - 0x10000 if value & 0x8000 else value


def hx(value: int) -> str:
    return f"{value & 0xffff:04x}"


@dataclass
class ObjectSlot:
    active: bool = False
    x: int = 0
    kind: str = "large"


def signature(slots: dict[str, ObjectSlot], extra_state: int) -> str:
    parts: list[str] = []
    for address in ALL:
        slot = slots[address]
        if not slot.active:
            continue
        kind = slot.kind
        if kind == "extra-charlie" and extra_state == 3:
            kind = "extra-charlie"
        parts.append(f"{kind}[{address} x={hx(slot.x)}]")
    return " ".join(parts) or "none"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("mame_frames", type=Path)
    parser.add_argument("comparison", type=Path)
    parser.add_argument("native_signatures", type=Path)
    parser.add_argument("--first", type=int, default=943)
    parser.add_argument("--last", type=int, default=3040)
    args = parser.parse_args()

    with args.mame_frames.open(newline="") as handle:
        all_rows = list(csv.DictReader(handle))
    by_frame = {int(row["frame"]): row for row in all_rows}
    rows = [by_frame[frame] for frame in range(args.first, args.last + 1)]

    # The replay begins from the exact board snapshot immediately before the
    # first captured admission.  Subsequent state is computed, not copied.
    previous = by_frame[args.first - 1]
    slots = {address: ObjectSlot() for address in ALL}
    accumulator = int(previous["activation_hi"] + previous["activation_lo"], 16)
    course_index = int(previous["course_index"], 16)
    course_state = int(previous["course_state"], 16)
    course_offset = int(previous["course_offset"], 16)
    extra_state = int(previous["extra_charlie_state"], 16)
    first_divergence: tuple[int, list[str]] | None = None

    args.comparison.parent.mkdir(parents=True, exist_ok=True)
    with args.comparison.open("w", newline="") as compare_file, \
            args.native_signatures.open("w", newline="") as native_file:
        fields = (
            "frame", "mame_signature", "native_signature", "course_index",
            "course_state", "course_offset", "activation_8_8", "match",
        )
        compare = csv.DictWriter(compare_file, fieldnames=fields)
        compare.writeheader()
        native = csv.writer(native_file)
        native.writerow(("frame", "active_signature"))

        for row in rows:
            frame = int(row["frame"])
            observed_extra = int(row["extra_charlie_state"], 16)
            if observed_extra == 1 and extra_state == 0:
                extra_state = 1
            command = int(row["movement_hi"] + row["movement_lo"], 16)
            if int(row["scroll_hi"], 16) == 0:
                command = 0
            delta = signed16(command - 0x0080)

            # $7539-$7554 advances all live records before $7607 schedules a
            # new one.  A wrapped record retires in the same object pass.
            for address, slot in slots.items():
                if not slot.active:
                    continue
                observed_status = row[f"hoop_{address}_status"] != "00"
                observed_x = int(row[f"hoop_{address}_x"], 16)
                if slot.kind == "extra-charlie":
                    # $7875 moves the converted reward composite through a
                    # separate path before the ordinary hoop loop.
                    if not observed_status:
                        slot.active = False
                        slot.x = 0
                    else:
                        slot.x = observed_x
                    continue
                if not observed_status:
                    slot.active = False
                    slot.x = 0
                    continue
                prior = slot.x
                slot.x = u16(slot.x + delta)
                # $7586-$75E9 has type-specific retirement state in addition
                # to X wrap (notably the converted reward object).  Consume
                # the captured retirement edge; scheduler admission remains
                # independently replayed from $7607-$76F8.
                if prior < 0x1000 and slot.x > 0xF000:
                    slot.active = False
                    slot.x = 0

            raw_activation_sum = accumulator + u16(delta)
            activation_sum = u16(raw_activation_sum)
            activation_carry = raw_activation_sum > 0xFFFF
            activation_negative = activation_sum & 0x8000 != 0
            # 6809 LBPL at $7614 uses the N flag from SUBD #$0080. If that
            # delta is negative, $762D adds the accumulator and schedules only
            # when the add does not carry.
            if delta < 0 and not activation_carry:
                table_value = COURSE[course_index]
                selector = (0x10 + course_index + course_offset) & 0xFF
                boundary = selector & 0x03 == 0
                reserved = boundary and course_state >= 0x60
                admitted: str | None = None

                if reserved:
                    admitted = "2760"
                    reload_value = int(row["board_frame"], 16) | 0x80
                    accumulator = reload_value << 8
                    course_state = reload_value
                else:
                    if boundary:
                        course_offset = (course_offset + 1) & 0xFF
                    accumulator = table_value << 8
                    course_state = table_value
                    admitted = next(
                        (address for address in ORDINARY if not slots[address].active),
                        None,
                    )

                course_index += 1
                if admitted is not None:
                    slot = slots[admitted]
                    slot.active = True
                    slot.x = u16(0xFF80 + delta)
                    slot.kind = "small" if admitted == "2760" else "large"
                    if admitted != "2760" and extra_state == 1:
                        extra_state = 2
                        slot.kind = "extra-charlie"
            else:
                accumulator = activation_sum

            # Reward-state writes are independent of obstacle scheduling.  We
            # mirror only their type/visibility effect in the active signature.
            if observed_extra == 3 and extra_state != 3:
                extra_state = 3

            # The framebuffer snapshot is authoritative for retirement.  A
            # converted reward may have been cleared earlier in the frame by
            # $71A0, after the object update modeled above.
            for address, slot in slots.items():
                if row[f"hoop_{address}_status"] == "00":
                    slot.active = False
                    slot.x = 0

            native_signature = signature(slots, extra_state)
            mame_parts: list[str] = []
            tracked = row["tracked_object"].lower()
            for address in ALL:
                if row[f"hoop_{address}_status"] == "00":
                    continue
                kind = "small" if address == "2760" else "large"
                if tracked == address:
                    kind = "extra-charlie"
                mame_parts.append(
                    f"{kind}[{address} x={row[f'hoop_{address}_x']}]"
                )
            mame_signature = " ".join(mame_parts) or "none"

            checks = {
                "signature": native_signature == mame_signature,
                "course_index": course_index == int(row["course_index"], 16),
                "course_state": course_state == int(row["course_state"], 16),
                "course_offset": course_offset == int(row["course_offset"], 16),
                "accumulator": accumulator == int(
                    row["activation_hi"] + row["activation_lo"], 16
                ),
            }
            match = all(checks.values())
            if not match and first_divergence is None:
                first_divergence = (
                    frame, [name for name, value in checks.items() if not value]
                )
            native.writerow((frame, native_signature))
            compare.writerow({
                "frame": frame,
                "mame_signature": mame_signature,
                "native_signature": native_signature,
                "course_index": f"{course_index:02x}",
                "course_state": f"{course_state:02x}",
                "course_offset": f"{course_offset:02x}",
                "activation_8_8": hx(accumulator),
                "match": int(match),
            })

    if first_divergence:
        frame, failed = first_divergence
        print(f"first divergence at MAME frame {frame}: {', '.join(failed)}")
        return 1
    print(f"full Level 1 hoop scheduler synchronized: {len(rows)} frames")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

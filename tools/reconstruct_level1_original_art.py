#!/usr/bin/env python3
"""Reconstruct verified Circus Charlie Level 1 hardware sprite composites."""

from __future__ import annotations

import argparse
import binascii
import csv
import hashlib
import json
import struct
import zlib
from dataclasses import dataclass
from pathlib import Path


SPRITE_ROMS = tuple(f"380_j{i:02d}.{i + 5}e" for i in range(6, 12))
RIDER_RECORDS = [f"0x{address:04x}" for address in range(0x25F0, 0x2650, 0x10)]


@dataclass(frozen=True)
class Cell:
    code: int
    color: int
    x: int
    y: int
    slot: int | None = None
    attr: int | None = None
    flipx: bool = False
    flipy: bool = False


def png_bytes(width: int, height: int, pixels: list[list[tuple[int, int, int, int]]]) -> bytes:
    def chunk(kind: bytes, data: bytes) -> bytes:
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", binascii.crc32(kind + data) & 0xFFFFFFFF)
    raw = b"".join(b"\0" + bytes(channel for pixel in row for channel in pixel) for row in pixels)
    return b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)) + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b"")


def read_rgb_png(path: Path) -> tuple[int, int, list[list[tuple[int, int, int]]]]:
    data = path.read_bytes()
    pos, payloads = 8, []
    while pos < len(data):
        size = struct.unpack(">I", data[pos:pos + 4])[0]
        kind, payload = data[pos + 4:pos + 8], data[pos + 8:pos + 8 + size]
        pos += size + 12
        if kind == b"IHDR":
            width, height, depth, color_type, _, _, _ = struct.unpack(">IIBBBBB", payload)
            if depth != 8 or color_type != 2:
                raise ValueError(f"unsupported verification PNG format in {path}")
        elif kind == b"IDAT":
            payloads.append(payload)
        elif kind == b"IEND":
            break
    raw, stride, rows, previous, offset = zlib.decompress(b"".join(payloads)), width * 3, [], [0] * (width * 3), 0
    for _ in range(height):
        mode, current = raw[offset], list(raw[offset + 1:offset + 1 + stride])
        offset += stride + 1
        for index in range(stride):
            left = current[index - 3] if index >= 3 else 0
            above = previous[index]
            upper_left = previous[index - 3] if index >= 3 else 0
            if mode == 1:
                current[index] = (current[index] + left) & 0xFF
            elif mode == 2:
                current[index] = (current[index] + above) & 0xFF
            elif mode == 3:
                current[index] = (current[index] + ((left + above) // 2)) & 0xFF
            elif mode == 4:
                estimate = left + above - upper_left
                distances = (abs(estimate - left), abs(estimate - above), abs(estimate - upper_left))
                predictor = (left, above, upper_left)[distances.index(min(distances))]
                current[index] = (current[index] + predictor) & 0xFF
            elif mode != 0:
                raise ValueError(f"unsupported PNG filter {mode}")
        rows.append([tuple(current[index:index + 3]) for index in range(0, stride, 3)])
        previous = current
    return width, height, rows


class Decoder:
    def __init__(self, rom_dir: Path):
        self.rom_dir = rom_dir
        self.sprite_data = b"".join((rom_dir / name).read_bytes() for name in SPRITE_ROMS)
        self.palette_prom = (rom_dir / "380_j18.2a").read_bytes()
        self.lookup = (rom_dir / "380_j16.10c").read_bytes()
        self.palette = []
        for value in self.palette_prom:
            red = sum(((value >> bit) & 1) * weight for bit, weight in zip((0, 1, 2), (33, 71, 151)))
            green = sum(((value >> bit) & 1) * weight for bit, weight in zip((3, 4, 5), (33, 71, 151)))
            blue = sum(((value >> bit) & 1) * weight for bit, weight in zip((6, 7), (81, 174)))
            self.palette.append((red, green, blue))

    def pixel(self, cell: Cell, x: int, y: int) -> tuple[int, int, int, int]:
        if cell.flipx:
            x = 15 - x
        if cell.flipy:
            y = 15 - y
        packed = self.sprite_data[cell.code * 128 + y * 8 + x // 2]
        pen = packed >> 4 if x % 2 == 0 else packed & 0x0F
        indirect = self.lookup[(cell.color & 0x0F) * 16 + pen] & 0x0F
        return (*self.palette[indirect], 0 if indirect == 0 else 255)


def compose(decoder: Decoder, cells: list[Cell]) -> tuple[list[list[tuple[int, int, int, int]]], dict]:
    min_x, min_y = min(c.x for c in cells), min(c.y for c in cells)
    max_x, max_y = max(c.x for c in cells) + 16, max(c.y for c in cells) + 16
    width, height = max_x - min_x, max_y - min_y
    driver = [[(0, 0, 0, 0) for _ in range(width)] for _ in range(height)]
    for cell in cells:  # MAME draws ascending hardware slots; later cells win.
        for source_y in range(16):
            for source_x in range(16):
                pixel = decoder.pixel(cell, source_x, source_y)
                if pixel[3]:
                    driver[cell.y - min_y + source_y][cell.x - min_x + source_x] = pixel
    upright = [[driver[height - 1 - x][y] for x in range(height)] for y in range(width)]
    opaque = [(x, y) for y, row in enumerate(upright) for x, pixel in enumerate(row) if pixel[3]]
    left, top = min(x for x, _ in opaque), min(y for _, y in opaque)
    right, bottom = max(x for x, _ in opaque) + 1, max(y for _, y in opaque) + 1
    cropped = [row[left:right] for row in upright[top:bottom]]
    geometry = {
        "driver_cell_bounds": {"x": min_x, "y": min_y, "width": width, "height": height},
        "upright_uncropped_size": {"width": height, "height": width},
        "opaque_crop_in_upright": {"x": left, "y": top, "width": right - left, "height": bottom - top},
        "composite_anchor_in_png": {"x": -left, "y": -top},
        "png_size": {"width": right - left, "height": bottom - top},
    }
    return cropped, geometry


def cell_metadata(cell: Cell, origin_x: int, origin_y: int) -> dict:
    address = cell.code * 128
    return {
        "hardware_slot": cell.slot,
        "code": f"0x{cell.code:03x}",
        "attribute": None if cell.attr is None else f"0x{cell.attr:02x}",
        "color_index": cell.color,
        "flip_x": cell.flipx,
        "flip_y": cell.flipy,
        "driver_offset": {"x": cell.x - origin_x, "y": cell.y - origin_y},
        "sprite_rom_region_offset": f"0x{address:05x}",
        "sprite_rom_file": SPRITE_ROMS[address // 0x2000],
        "sprite_rom_file_offset": f"0x{address % 0x2000:04x}",
    }


def rider_cells(codes: list[int], color: int = 0) -> list[Cell]:
    positions = [(224, 171), (224, 187), (224, 203), (208, 171), (208, 187), (208, 203)]
    return [Cell(code, color, x, y, slot=31 + index, attr=color) for index, (code, (x, y)) in enumerate(zip(codes, positions))]


def hoop_cells(color: int) -> list[Cell]:
    return [Cell(code, color, x, 156, slot=45 + index, attr=color) for index, (code, x) in enumerate(zip((0xE4, 0xE5, 0xE6), (156, 172, 188)))]


def export_asset(decoder: Decoder, root: Path, relative: str, cells: list[Cell], extra: dict, metadata: list[dict]) -> None:
    pixels, geometry = compose(decoder, cells)
    path = root / relative
    path.parent.mkdir(parents=True, exist_ok=True)
    image = png_bytes(len(pixels[0]), len(pixels), pixels)
    path.write_bytes(image)
    origin_x, origin_y = min(c.x for c in cells), min(c.y for c in cells)
    metadata.append({
        "asset": relative,
        "sha256": hashlib.sha256(image).hexdigest(),
        "rendering": "gfx_16x16x4_packed_msb; sprite PROM lookup; indirect color 0 transparent; ascending-slot draw; ROT90 clockwise; tight alpha crop",
        "geometry": geometry,
        "components": [cell_metadata(cell, origin_x, origin_y) for cell in cells],
        **extra,
    })


def verify(decoder: Decoder, trace_csv: Path, capture_dir: Path) -> dict:
    rows = list(csv.DictReader(trace_csv.open()))
    checks = []
    targets = {
        "run-a-00": set(range(31, 37)),
        "run-b-00": set(range(31, 37)),
        "run-c-00": set(range(31, 37)),
        # The hoop visually occludes the rider at frame 1375 because its slots
        # draw later. Frame 1360 verifies the same airborne composite before
        # that priority overlap.
        "airborne-13": set(range(31, 37)),
        "hoop-crossing": set(range(45, 48)),
    }
    for label, slots in targets.items():
        frame_rows = [row for row in rows if row["label"] == label and int(row["slot"]) in slots]
        frame = int(frame_rows[0]["frame"])
        screenshot = next(capture_dir.glob(f"*-frame-{frame:04d}-{label}.png"))
        _, _, screen = read_rgb_png(screenshot)
        candidates = []
        for bank in (0, 1):
            cells = [Cell(int(row["decoded_code"]), int(row["color"]), int(row["x"]), int(row["y"]), int(row["slot"]), int(row["attr"], 16), row["flipx"] == "1", row["flipy"] == "1") for row in frame_rows if int(row["bank"]) == bank]
            pixels, _ = compose(decoder, cells)
            points = [(x, y, pixel[:3]) for y, row in enumerate(pixels) for x, pixel in enumerate(row) if pixel[3]]
            best = (0, 0, 0)
            for origin_y in range(257 - len(pixels)):
                for origin_x in range(225 - len(pixels[0])):
                    matched = sum(screen[origin_y + y][origin_x + x] == rgb for x, y, rgb in points)
                    best = max(best, (matched, origin_x, origin_y))
            candidates.append((best[0], len(points), bank, best[1], best[2]))
        matched, tested, bank, origin_x, origin_y = max(candidates)
        checks.append({"label": label, "frame": frame, "display_bank": bank, "matched_at_upright": {"x": origin_x, "y": origin_y}, "matched_opaque_pixels": matched, "tested_opaque_pixels": tested, "mismatched_opaque_pixels": tested - matched})
    return {"method": "exact RGB comparison of reconstructed opaque sprite pixels against MAME screenshots", "checks": checks, "passed": all(check["mismatched_opaque_pixels"] == 0 for check in checks)}


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--rom-dir", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--trace-csv", type=Path)
    parser.add_argument("--capture-dir", type=Path)
    args = parser.parse_args()
    decoder, root, metadata = Decoder(args.rom_dir), args.output, []

    poses = {
        "rider/run-a.png": ([0x62, 0x61, 0x60, 0x5F, 0x5E, 0x5D], [1326, 1343, 1410]),
        "rider/run-b.png": ([0xCD, 0xCC, 0xB6, 0xB5, 0x64, 0x63], [1328, 1418]),
        "rider/run-c.png": ([0x5C, 0x5B, 0x5A, 0x59, 0x58, 0x57], [1336, 1425]),
        "rider/airborne.png": ([0x5C, 0x5B, 0x5A, 0x59, 0x58, 0x57], list(range(1347, 1410))),
        "rider/death.png": ([0x38, 0x37, 0x36, 0x35, 0x30, 0x2F], list(range(1390, 1411))),
    }
    for relative, (codes, frames) in poses.items():
        export_asset(decoder, root, relative, rider_cells(codes), {"source_frames": frames, "object_records": RIDER_RECORDS, "state_note": "airborne.png is intentionally pixel-identical to run-c.png; jump state changes absolute hardware X/source Y, not codes, attributes, flips, or relative placement" if "airborne" in relative else None}, metadata)
    for index, color in enumerate((3, 4, 5)):
        export_asset(decoder, root, f"large-hoop/hoop-{index:02d}.png", hoop_cells(color), {"source_frames": "observed repeatedly during synchronized frames 1326-1410", "object_records": ["0x26d0", "0x26e0", "0x26f0"], "logical_rows": ["0x9c", "0xac", "0xbc"]}, metadata)
    export_asset(decoder, root, "misc/failure-effect-left.png", [Cell(c, 0, 90, y, slot=23 + i, attr=0) for i, (c, y) in enumerate(zip((0x18, 0x17, 0x16), (64, 80, 96)))], {"source_frames": list(range(1390, 1411)), "object_records": "separate failure-effect hardware slots; not the rider records"}, metadata)
    export_asset(decoder, root, "misc/failure-effect-right.png", [Cell(c, 0, 82, y, slot=26 + i, attr=0) for i, (c, y) in enumerate(zip((0x18, 0x17, 0x16), (160, 176, 192)))], {"source_frames": list(range(1390, 1411)), "object_records": "separate failure-effect hardware slots; not the rider records"}, metadata)

    for directory in ("fire-pot", "bonus-ring", "metadata"):
        (root / directory).mkdir(parents=True, exist_ok=True)
    (root / "fire-pot" / "UNRESOLVED.md").write_text("No fire-pot composite was mapped with sufficient sprite-RAM evidence in the synchronized successful-hoop trace. No guessed export was created.\n")
    (root / "bonus-ring" / "UNRESOLVED.md").write_text("No small/bonus-ring composite was mapped with sufficient sprite-RAM evidence in the synchronized successful-hoop trace. No guessed export was created.\n")
    (root / "misc" / "UNRESOLVED.md").write_text("Hidden coin, extra Charlie, goal platform, and score/bonus object graphics still require dedicated observed sprite-RAM captures; none were guessed.\n")
    verification = verify(decoder, args.trace_csv, args.capture_dir) if args.trace_csv and args.capture_dir else {"passed": False, "reason": "verification inputs not supplied"}
    (root / "metadata" / "assets.json").write_text(json.dumps({"rom_set": "circusc4", "assets": metadata}, indent=2) + "\n")
    (root / "metadata" / "verification.json").write_text(json.dumps(verification, indent=2) + "\n")
    (root / "metadata" / "palette.json").write_text(json.dumps({"resistor_weights": {"red_green": [33, 71, 151], "blue": [81, 174]}, "indirect_rgb": decoder.palette, "sprite_lookup": list(decoder.lookup)}, indent=2) + "\n")
    if not verification.get("passed"):
        raise SystemExit("verification failed or was not run")
    print(f"Exported {len(metadata)} assembled assets; verification passed: {root}")


if __name__ == "__main__":
    main()

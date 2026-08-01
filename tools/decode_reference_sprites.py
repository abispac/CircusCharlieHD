#!/usr/bin/env python3
"""Decode private circusc sprite ROMs into temporary clean-room references.

The output is analysis-only and must never be copied into production assets.
"""

from __future__ import annotations

import argparse
from pathlib import Path

from PIL import Image, ImageDraw


SPRITE_ROMS = (
    "380_j06.11e",
    "380_j07.12e",
    "380_j08.13e",
    "380_j09.14e",
    "380_j10.15e",
    "380_j11.16e",
)


def component(bits: tuple[int, ...], weights: tuple[int, ...]) -> int:
    return min(255, sum(bit * weight for bit, weight in zip(bits, weights)))


def load_palette(rom_dir: Path) -> tuple[list[tuple[int, int, int]], bytes]:
    palette_prom = (rom_dir / "380_j18.2a").read_bytes()
    sprite_lookup = (rom_dir / "380_j16.10c").read_bytes()
    palette: list[tuple[int, int, int]] = []
    for value in palette_prom:
        red = component(tuple((value >> bit) & 1 for bit in (0, 1, 2)),
                        (33, 71, 151))
        green = component(tuple((value >> bit) & 1 for bit in (3, 4, 5)),
                          (33, 71, 151))
        blue = component(tuple((value >> bit) & 1 for bit in (6, 7)),
                         (71, 151))
        palette.append((red, green, blue))
    return palette, sprite_lookup


def decode_sprite(data: bytes, code: int, color_group: int,
                  palette: list[tuple[int, int, int]],
                  lookup: bytes) -> Image.Image:
    start = code * 128
    sprite = Image.new("RGBA", (16, 16), (0, 0, 0, 0))
    pixels = sprite.load()
    for y in range(16):
        for x in range(16):
            packed = data[start + y * 8 + x // 2]
            pen = packed >> 4 if x % 2 == 0 else packed & 0x0F
            if pen == 0:
                continue
            indirect = lookup[(color_group & 0x0F) * 16 + pen] & 0x0F
            pixels[x, y] = (*palette[indirect], 255)
    return sprite


def checker(size: tuple[int, int]) -> Image.Image:
    image = Image.new("RGBA", size, (28, 30, 37, 255))
    draw = ImageDraw.Draw(image)
    for y in range(0, size[1], 8):
        for x in range(0, size[0], 8):
            if (x // 8 + y // 8) % 2 == 0:
                draw.rectangle((x, y, x + 7, y + 7), fill=(48, 51, 61, 255))
    return image


def compose_pose(sprites: dict[int, Image.Image], codes: tuple[int, ...]) -> Image.Image:
    raw = Image.new("RGBA", (32, 48), (0, 0, 0, 0))
    for column in range(2):
        for row in range(3):
            raw.alpha_composite(sprites[codes[column * 3 + row]],
                                (column * 16, row * 16))
    # The cabinet display is rotated 90 degrees clockwise from native memory.
    return raw.transpose(Image.Transpose.ROTATE_270)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--rom-dir", type=Path, required=True)
    parser.add_argument("--out", type=Path, default=Path("/tmp/circusc-rider-blueprints.png"))
    args = parser.parse_args()

    data = b"".join((args.rom_dir / name).read_bytes() for name in SPRITE_ROMS)
    palette, lookup = load_palette(args.rom_dir)
    sprites = {code: decode_sprite(data, code, 0, palette, lookup)
               for code in range(len(data) // 128)}

    poses = (
        ("ground-a", (0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C)),
        ("ground-b", (0x63, 0x64, 0xB5, 0x8F, 0x94, 0xD4)),
        ("ground-c", (0x63, 0x64, 0xB5, 0xB6, 0xCC, 0xCD)),
    )
    sheet = checker((768, 256))
    draw = ImageDraw.Draw(sheet)
    for index, (label, codes) in enumerate(poses):
        pose = compose_pose(sprites, codes).resize((384, 256), Image.Resampling.NEAREST)
        x = index * 256
        sheet.alpha_composite(pose.crop((64, 0, 320, 256)), (x, 0))
        draw.text((x + 8, 8), label, fill=(255, 240, 120, 255))
    args.out.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(args.out)
    print(args.out)


if __name__ == "__main__":
    main()

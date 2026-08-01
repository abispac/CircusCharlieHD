#!/usr/bin/env python3
"""Normalize generated rider cells around one stable torso/saddle anchor."""

from __future__ import annotations

import argparse
from pathlib import Path

from PIL import Image


# Offsets measured against the common lion torso/saddle point in the v5 HD
# generation. They reposition the whole drawing inside its cell; they do not
# warp or repaint any character pixels.
CELL_OFFSETS = (
    (0, 0),
    (-20, 15),
    (-30, 15),
    (0, 80),
    (0, 120),
    (0, 45),
)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()

    source = Image.open(args.input).convert("RGBA")
    cell_width = source.width // 3
    cell_height = source.height // 2
    output = Image.new("RGBA", source.size, (0, 0, 0, 0))

    for frame, (offset_x, offset_y) in enumerate(CELL_OFFSETS):
        column = frame % 3
        row = frame // 3
        box = (
            column * cell_width,
            row * cell_height,
            (column + 1) * cell_width,
            (row + 1) * cell_height,
        )
        cell = source.crop(box)
        normalized = Image.new("RGBA", (cell_width, cell_height),
                               (0, 0, 0, 0))
        normalized.alpha_composite(cell, (offset_x, offset_y))
        output.alpha_composite(normalized,
                               (column * cell_width, row * cell_height))

    args.out.parent.mkdir(parents=True, exist_ok=True)
    output.save(args.out)
    print(args.out)


if __name__ == "__main__":
    main()

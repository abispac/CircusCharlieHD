#!/usr/bin/env python3
"""Build the Event 1 money-bird atlas.

    python3 tools/build_stage1_bird.py

The painted bird (art/source/incoming/stage1-bird-sheet-v1.png, four cells)
carries its bag in cells 0 and 1 and pours it out in cell 2; cell 3 was a
bag-less bird, so drawing a separate bag under it showed two bags.  The
arcade bird keeps hold of the bag ($25D0, row $6F) the whole time the coins
pour, so this writes assets/stage1-bird-sheet-v2.png with cell 3 replaced by
a mid-flap copy of cell 2 (its wing squashed toward the shoulder), giving a
pour pose for both wing frames.
"""
import os

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
SOURCE = os.path.join(ROOT, "art", "source", "incoming", "stage1-bird-sheet-v1.png")
OUT = os.path.join(ROOT, "assets", "stage1-bird-sheet-v2.png")
WING_SQUASH = 0.55


def mid_flap(cell):
    width, height = cell.size
    wing = Image.new("RGBA", cell.size, (0, 0, 0, 0))
    body = cell.copy()
    source = cell.load()
    wing_px = wing.load()
    body_px = body.load()
    for y in range(height):
        for x in range(width):
            # the raised wing sits above and behind the tilted body
            if (x >= 300 and y <= 372) or (x >= 255 and y <= 300):
                wing_px[x, y] = source[x, y]
                body_px[x, y] = (0, 0, 0, 0)
    box = wing.getbbox()
    crop = wing.crop(box)
    squashed = crop.resize((crop.width, int(crop.height * WING_SQUASH)),
                           Image.LANCZOS)
    out = body
    out.alpha_composite(squashed, (box[0], box[3] - squashed.height))
    return out


def main():
    sheet = Image.open(SOURCE).convert("RGBA")
    width = sheet.width // 4
    cells = [sheet.crop((i * width, 0, (i + 1) * width, sheet.height))
             for i in range(4)]
    cells[3] = mid_flap(cells[2])
    atlas = Image.new("RGBA", sheet.size, (0, 0, 0, 0))
    for i, cell in enumerate(cells):
        atlas.paste(cell, (i * width, 0))
    atlas.save(OUT)
    print("wrote", OUT, atlas.size)


if __name__ == "__main__":
    main()

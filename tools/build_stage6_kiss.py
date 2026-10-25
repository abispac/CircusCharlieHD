#!/usr/bin/env python3
"""Build the Event 6 goal atlas from the Gemini kiss sheet
(art/source/incoming/stage6-kiss-sheet-gemini-01.png, six frames on black):
0 side by side, 1 she leans in, 2 the peck on the cheek, 3 he blushes with
the kiss mark, 4 the same with his hands on his cheeks, 5 both wave.

Every cell is as wide as the widest pair and Charlie's centre (his red
trousers) sits on the cell's middle, his and her feet on the baseline, so
the game draws the cell centred on Charlie's record.  Written to
assets/stage6-kiss-6-v3.png (+ .json); she stands kPartnerRows tall.
"""
import json, os, sys
import numpy as np
from PIL import Image
from scipy import ndimage

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "art", "source", "incoming", "stage6-kiss-sheet-gemini-01.png")
OUT = os.path.join(ROOT, "assets", "stage6-kiss-6-v3.png")
CELLS = [(86, 303), (445, 722), (851, 1093), (1255, 1468), (1649, 1854), (2073, 2330)]  # the lone Charlie at 1949-2062 is left out
PAD = 8

rgb = np.asarray(Image.open(SRC).convert("RGB")).astype(np.float32)
bright = rgb.max(axis=2)
solid = bright > 45
solid = ndimage.binary_dilation(solid, iterations=2)
solid = ndimage.binary_fill_holes(solid)
solid = ndimage.binary_erosion(solid, iterations=1)
alpha = np.where(solid, np.clip((bright - 8.0) / 30.0, 0.0, 1.0), 0.0)
alpha = np.where(solid & (bright >= 38), 1.0, alpha)
rgba = np.dstack([rgb, alpha * 255.0]).astype(np.uint8)
sheet = Image.fromarray(rgba, "RGBA")

frames = []
for x0, x1 in CELLS:
    cell = sheet.crop((x0 - 4, 0, x1 + 5, sheet.height))
    bb = cell.getbbox()
    cell = cell.crop(bb)
    a = np.asarray(cell).astype(int)
    red = (a[..., 3] > 128) & (a[..., 0] > 150) & (a[..., 1] < 90) & (a[..., 2] < 90)
    red[: int(cell.height * 0.35)] = False
    xs = np.where(red.any(axis=0))[0]
    centre = (xs.min() + xs.max()) / 2.0
    frames.append((cell, centre))

reach = max(max(c, f.width - c) for f, c in frames)
cw = int(np.ceil(2 * reach)) + 2 * PAD
ch = max(f.height for f, _ in frames) + 2 * PAD
atlas = Image.new("RGBA", (cw * len(frames), ch), (0, 0, 0, 0))
meta = {"source": os.path.basename(SRC), "cell": [cw, ch], "columns": len(frames), "rows": 1,
        "baseline": PAD, "anchor": "charlie centre on the cell middle", "frames": []}
for i, (f, c) in enumerate(frames):
    x = i * cw + int(round(cw / 2.0 - c))
    y = ch - PAD - f.height
    atlas.paste(f, (x, y), f)
    meta["frames"].append({"index": i, "size": [f.width, f.height], "charlie_x": int(round(c)),
                           "feet": [i * cw + cw // 2, ch - PAD]})
atlas.save(OUT)
json.dump(meta, open(os.path.splitext(OUT)[0] + ".json", "w"), indent=1)
print("wrote", OUT, atlas.size)

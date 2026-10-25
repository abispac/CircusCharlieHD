#!/usr/bin/env python3
"""Build the Event 3 (trampolines) Charlie atlases.

    python3 tools/build_stage3_charlie.py

The board shows Charlie from the front while he bounces on a drum (crouch,
rise, stretch, star) and, between drums, a forward somersault seen from the
side: a diagonal launch, a flat dive, the dive tipping over, the upside-down
tuck with his hands on his shins, the unroll, then an upright side view down
to the next drum (MAME circusc4, stage frames 46-86; see
docs/STAGE3_TAMBOURINE_CALIBRATION.md).  The front poses come from
art/source/incoming/charlie-front-bounce-gemini-01.jpeg and the side ones
from charlie-side-somersault-gemini-01.jpeg (Gemini, drawn from the side
still and the turnaround so the face matches); the fallen pose is borrowed
from assets/stage2-charlie-poses-4-v4.png.
Everything is scaled so the standing Charlie is the same 400 px as the Stage 2
v4 walk. Writes

    assets/stage3-charlie-bounce-12-v4.png    (moving between drums)
    assets/stage3-charlie-vertical-12-v4.png  (bouncing in place)

as single-row atlases (12 cells, feet on a common baseline, 8 px pad) with a
JSON file each, like tools/import_sprites.py does.

Bounce cells: 0 upright side view (after the flip, down to the drum), 1-2
front crouch, 3-7 the five somersault cells (launch, dive, dive tipping,
upside-down tuck, unroll; the game draws them centred), 8 front crouch, 9
fallen, 10 arms spread (the frozen frame and the cheer's second cell), 11
arms up (cheer).
Vertical cells: 0 crouch, 1 half risen, 2 stretched, 3 star, then the game
alternates 4 (star) and 6 (stretched); 5 and 7-11 are spares.
"""
import json
import os
import subprocess
import sys
import tempfile

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
ASSETS = os.path.join(ROOT, "assets")
FRONT_SHEET = os.path.join(ROOT, "art", "source", "incoming",
                           "charlie-front-bounce-gemini-01.jpeg")
# The stretched front pose is 571 px on the sheet; standing he would be a
# little shorter, so this puts him at about the walk's 400 px.
FRONT_SCALE = 0.73
SIDE_SHEET = os.path.join(ROOT, "art", "source", "incoming",
                          "charlie-side-somersault-gemini-01.jpeg")
# The upright side pose is 464 px on that sheet.
SIDE_SCALE = 400.0 / 464.0
PAD = 8
POSES = os.path.join(ASSETS, "stage2-charlie-poses-4-v4.png")
POSES_JSON = os.path.join(ASSETS, "stage2-charlie-poses-4-v4.json")


def cells_of(png, meta_json):
    meta = json.load(open(meta_json))
    sheet = Image.open(png).convert("RGBA")
    out = {}
    for frame in meta["frames"]:
        # the importer's manifest places each frame by its feet (centre x,
        # bottom y) and size inside the atlas
        w, h = frame["size"]
        fx, fy = frame["feet"]
        x0 = fx - w // 2
        out[frame["index"]] = sheet.crop((x0, fy - h, x0 + w, fy))
    return out


def sheet_cells(sheet, scale):
    with tempfile.TemporaryDirectory() as tmp:
        atlas = os.path.join(tmp, "sheet.png")
        subprocess.run([sys.executable, os.path.join(HERE, "import_sprites.py"), sheet,
                        atlas, "--chroma", "--scale", str(scale)], check=True)
        return cells_of(atlas, os.path.join(tmp, "sheet.json"))


def turned(img, degrees):
    """The pose rotated clockwise about its centre (a later moment of the
    roll), trimmed to its pixels."""
    out = img.rotate(-degrees, resample=Image.BICUBIC, expand=True)
    return out.crop(out.getbbox())


def write_atlas(path, cells):
    width = max(c.width for c in cells) + 2 * PAD
    height = max(c.height for c in cells) + 2 * PAD
    atlas = Image.new("RGBA", (width * len(cells), height), (0, 0, 0, 0))
    meta = {"source": os.path.basename(path), "cell": [width, height],
            "columns": len(cells), "rows": 1, "baseline": PAD, "frames": []}
    for i, cell in enumerate(cells):
        x = i * width + (width - cell.width) // 2
        y = height - PAD - cell.height
        atlas.paste(cell, (x, y), cell)
        meta["frames"].append({"index": i, "cell": [i, 0],
                               "source": [x, y, x + cell.width - 1, y + cell.height - 1],
                               "size": [cell.width, cell.height],
                               "feet": [i * width + width // 2, height - PAD]})
    atlas.save(path)
    with open(os.path.splitext(path)[0] + ".json", "w") as f:
        json.dump(meta, f, indent=1)
    print("wrote", path, atlas.size)


def main():
    f = sheet_cells(FRONT_SHEET, FRONT_SCALE)   # 0 crouch, 1 half risen, 2 stretched, 3 star
    # 0 launch, 1 dive, 2 swoop (unused), 3 on his back, 4 upside-down tuck, 5 upright
    r = sheet_cells(SIDE_SHEET, SIDE_SCALE)
    p = cells_of(POSES, POSES_JSON)   # 0 stand, 1 arms up, 2 fallen, 3 tuck
    roll = [r[0], r[1], turned(r[1], 40), r[4], turned(r[3], 35)]
    bounce = [r[5], f[0], f[0]] + roll + [f[0], p[2], f[1], f[2]]
    vertical = [f[0], f[1], f[2], f[3], f[3], f[2], f[2], f[3], f[2], f[1], f[0], f[2]]
    write_atlas(os.path.join(ASSETS, "stage3-charlie-bounce-12-v4.png"), bounce)
    write_atlas(os.path.join(ASSETS, "stage3-charlie-vertical-12-v4.png"), vertical)


if __name__ == "__main__":
    main()

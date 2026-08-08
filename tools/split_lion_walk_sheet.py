#!/usr/bin/env python3
"""Split and anchor the 12-frame lion gait generated as a 4x3 image sheet."""

from pathlib import Path

import numpy as np
from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "assets" / "source-art" / "stage1-lion-walk-12-v1-alpha.png"
OUTPUT = ROOT / "assets" / "source-art" / "stage1-lion-walk-12-v1-frames"
ATLAS = ROOT / "assets" / "stage1-lion-walk-12-v1.png"
PREVIEW = ROOT / "wam-proof" / "out" / "stage1-lion-walk-12-v1-preview.gif"

CANVAS = (512, 320)
ANCHOR = (382, 108)


def runs(values, minimum_gap=3):
    indices = np.flatnonzero(values)
    if not len(indices):
        return []
    groups = []
    start = previous = int(indices[0])
    for value in map(int, indices[1:]):
        if value - previous > minimum_gap:
            groups.append((start, previous + 1))
            start = value
        previous = value
    groups.append((start, previous + 1))
    return groups


def detect_boxes(image):
    pixels = np.asarray(image)
    mask = pixels[:, :, 3] > 24
    row_runs = runs(mask.any(axis=1), minimum_gap=8)
    if len(row_runs) != 3:
        raise RuntimeError(f"Expected 3 occupied rows, found {len(row_runs)}")

    boxes = []
    for y0, y1 in row_runs:
        row_mask = mask[y0:y1]
        column_runs = runs(row_mask.any(axis=0), minimum_gap=8)
        if len(column_runs) != 4:
            raise RuntimeError(f"Expected 4 lions in row, found {len(column_runs)}")
        for x0, x1 in column_runs:
            local = mask[y0:y1, x0:x1]
            occupied_y = np.flatnonzero(local.any(axis=1))
            boxes.append((x0, y0 + int(occupied_y[0]), x1, y0 + int(occupied_y[-1]) + 1))
    return boxes


def mane_anchor(frame):
    pixels = np.asarray(frame)
    alpha = pixels[:, :, 3] > 96
    height, width = alpha.shape
    rgb = pixels[:, :, :3]
    dark_gold = (
        alpha
        & (rgb[:, :, 0] < 205)
        & (rgb[:, :, 1] < 135)
        & (rgb[:, :, 2] < 70)
    )
    dark_gold[:, : int(width * 0.48)] = False
    ys, xs = np.nonzero(dark_gold)
    if not len(xs):
        ys, xs = np.nonzero(alpha)
    return int(np.median(xs)), int(np.median(ys))


def normalize_frame(source, box):
    padding = 3
    x0, y0, x1, y1 = box
    crop = source.crop((max(0, x0 - padding), max(0, y0 - padding), min(source.width, x1 + padding), min(source.height, y1 + padding)))
    anchor_x, anchor_y = mane_anchor(crop)
    canvas = Image.new("RGBA", CANVAS, (0, 0, 0, 0))
    paste_x = ANCHOR[0] - anchor_x
    paste_y = ANCHOR[1] - anchor_y
    canvas.alpha_composite(crop, (paste_x, paste_y))
    return canvas


def main():
    source = Image.open(SOURCE).convert("RGBA")
    boxes = detect_boxes(source)
    OUTPUT.mkdir(parents=True, exist_ok=True)
    frames = []
    for index, box in enumerate(boxes, start=1):
        frame = normalize_frame(source, box)
        frame.save(OUTPUT / f"lion-walk-{index:02d}.png", optimize=True)
        frames.append(frame)

    atlas = Image.new("RGBA", (CANVAS[0] * 4, CANVAS[1] * 3), (0, 0, 0, 0))
    for index, frame in enumerate(frames):
        atlas.alpha_composite(frame, ((index % 4) * CANVAS[0], (index // 4) * CANVAS[1]))
    atlas.save(ATLAS, optimize=True)

    preview_frames = []
    background = Image.new("RGBA", CANVAS, (4, 12, 28, 255))
    for frame in frames:
        preview = background.copy()
        preview.alpha_composite(frame)
        preview_frames.append(preview.convert("RGB"))
    preview_frames[0].save(PREVIEW, save_all=True, append_images=preview_frames[1:], duration=95, loop=0, optimize=True)

    print(f"Wrote {len(frames)} frames to {OUTPUT}")
    print(f"Wrote atlas {ATLAS}")
    print(f"Wrote preview {PREVIEW}")


if __name__ == "__main__":
    main()


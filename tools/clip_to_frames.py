#!/usr/bin/env python3
"""Turn an AI-generated green-screen character clip into clean transparent
frames and a sprite sheet.

    python3 tools/clip_to_frames.py CLIP.mp4 OUT_DIR --times 1.0,1.08,... \
        [--crop x0,x1] [--rope] [--contact every_seconds]

For each time a frame is grabbed, the green is keyed to alpha (same
green-excess key as tools/import_sprites.py --chroma), the drawn rope or
floor line is erased (`--rope`: dark pixel runs that reach outside the
character's own columns, and the dark line between his feet), the frame is
cropped to the character and saved as OUT_DIR/frame-NN.png; OUT_DIR/sheet.png
holds them side by side for tools/import_sprites.py.
`--contact S` instead dumps every S seconds to OUT_DIR/contact.png to pick
the times from.
"""
import argparse
import os
import subprocess
import sys

import numpy as np
from PIL import Image, ImageDraw
from scipy import ndimage


def grab(path, seconds):
    out = subprocess.run(["ffmpeg", "-v", "error", "-ss", f"{seconds:.4f}", "-i", path,
                          "-frames:v", "1", "-f", "image2pipe", "-vcodec", "png", "-"],
                         capture_output=True, check=True).stdout
    from io import BytesIO
    if not out:
        raise SystemExit(f"no frame at {seconds:.3f}s in {path}")
    return Image.open(BytesIO(out)).convert("RGB")


def chroma_alpha(rgb, threshold=40.0, soft=40.0):
    r, g, b = rgb[..., 0], rgb[..., 1], rgb[..., 2]
    other = np.maximum(r, b)
    excess = g - other
    alpha = np.clip((threshold + soft - excess) / soft, 0.0, 1.0)
    # the shadow the model paints under the rope is dark green: greenish by
    # ratio even where the excess is small
    ratio = g / np.maximum(other, 1.0)
    shadow = (ratio > 1.3) & (g > 30)
    # ...and the soft ground shadow under the feet is darker still, but
    # always greener than it is red or blue (the ink lines are not)
    ground = (g > r + 3) & (g > b + 3) & (np.maximum(g, other) < 95)
    alpha = np.where(shadow | ground, 0.0, alpha)
    return alpha


def despill(rgb, alpha):
    out = rgb.copy()
    r, g, b = out[..., 0], out[..., 1], out[..., 2]
    limit = np.maximum(r, b)
    fringe = (alpha < 0.999) & (alpha > 0)
    g2 = np.where(fringe & (g > limit), limit, g)
    out[..., 1] = g2
    return out


def erase_rope(rgb, alpha, dark=95, reach=24):
    """Remove the drawn rope: dark pixels in horizontal runs that leave the
    character's columns, plus dark pixels on those rows that have nothing
    of the character right above them (the stretch between the feet)."""
    h, w = alpha.shape
    value = rgb.max(axis=2)
    darkpx = (alpha > 0.5) & (value < dark)
    colour = (alpha > 0.5) & (value >= dark)          # the painted character
    cols = np.where(colour.any(axis=0))[0]
    if len(cols) == 0:
        return alpha
    left, right = cols.min(), cols.max()
    rope_rows = []
    for y in range(h):
        row = darkpx[y]
        if not row.any():
            continue
        # runs of dark pixels
        padded = np.concatenate([[False], row, [False]])
        edges = np.diff(padded.astype(int))
        starts, ends = np.where(edges == 1)[0], np.where(edges == -1)[0]
        for s, e in zip(starts, ends):
            if (s < left - reach or e > right + reach) and e - s > 12:
                alpha[y, s:e] = 0.0
                rope_rows.append(y)
    if rope_rows:
        lo, hi = max(0, min(rope_rows) - 2), min(h, max(rope_rows) + 3)
        # on the rope rows only the shoes' own outline survives: dark pixels
        # with shoe colour (blue or yellow) right above them
        r, g, b = rgb[..., 0], rgb[..., 1], rgb[..., 2]
        shoe = colour & (((b > r + 30) & (b > g + 10)) | ((r > 150) & (g > 120) & (b < 110)))
        above = np.zeros_like(colour)
        for d in range(1, 8):
            above[d:, :] |= shoe[:-d, :]
        zone = np.zeros_like(alpha, dtype=bool)
        zone[lo:hi, :] = True
        kill = zone & darkpx & ~above
        alpha[kill] = 0.0
    # The stretch of rope under the feet is darkened by the shoes' shadow and
    # keyed as neither green nor character: on the bottom rows of the figure,
    # drop dull dark pixels that have no shoe colour right above them.
    rows = np.where((alpha > 0.5).any(axis=1))[0]
    if len(rows):
        bottom = rows.max()
        r, g, b = rgb[..., 0], rgb[..., 1], rgb[..., 2]
        shoe = colour & (((b > r + 30) & (b > g + 10)) | ((r > 150) & (g > 120) & (b < 110)))
        above = np.zeros_like(colour)
        for d in range(1, 9):
            above[d:, :] |= shoe[:-d, :]
        dull = (alpha > 0.5) & (value < 100) & ((rgb.max(axis=2) - rgb.min(axis=2)) < 45)
        zone = np.zeros_like(alpha, dtype=bool)
        zone[max(0, bottom - 18):bottom + 1, :] = True
        alpha[zone & dull & ~above] = 0.0
    return alpha


def keep_character(alpha, min_area=150):
    """Drop compression specks: keep the big connected blobs only."""
    solid = alpha > 0.15
    labels, count = ndimage.label(solid)
    if count == 0:
        return alpha
    areas = ndimage.sum(solid, labels, index=np.arange(1, count + 1))
    keep = np.zeros(count + 1, dtype=bool)
    keep[1:] = areas >= min_area
    mask = keep[labels]
    # grow the kept mask a little so soft edges survive
    mask = ndimage.binary_dilation(mask, iterations=2)
    return np.where(mask, alpha, 0.0)


def process(frame, crop, rope):
    rgb = np.asarray(frame).astype(np.float32)
    if crop:
        x0, x1 = crop
        rgb = rgb[:, x0:x1]
    alpha = chroma_alpha(rgb)
    rgb = despill(rgb, alpha)
    if rope:
        alpha = erase_rope(rgb, alpha)
    alpha = keep_character(alpha)
    out = np.dstack([rgb, alpha * 255.0]).clip(0, 255).astype(np.uint8)
    img = Image.fromarray(out, "RGBA")
    bbox = Image.fromarray((alpha > 0.02).astype(np.uint8) * 255).getbbox()
    return img.crop(bbox) if bbox else img


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("clip")
    ap.add_argument("out_dir")
    ap.add_argument("--times", help="comma-separated seconds of the frames to cut")
    ap.add_argument("--crop", help="x0,x1 columns to keep (drops a watermark)")
    ap.add_argument("--rope", action="store_true", help="erase the drawn rope/floor line")
    ap.add_argument("--contact", type=float, help="dump a contact sheet every N seconds instead")
    args = ap.parse_args()
    os.makedirs(args.out_dir, exist_ok=True)
    crop = tuple(int(v) for v in args.crop.split(",")) if args.crop else None

    if args.contact:
        duration = float(subprocess.run(["ffprobe", "-v", "error", "-show_entries", "format=duration",
                                         "-of", "csv=p=0", args.clip], capture_output=True,
                                        text=True, check=True).stdout.strip())
        times = []
        t = 0.0
        while t < duration - 0.08:
            times.append(round(t, 3))
            t += args.contact
        frames = [process(grab(args.clip, t), crop, args.rope) for t in times]
        th = 220
        tiles = [f.resize((max(1, int(f.width * th / f.height)), th), Image.LANCZOS) for f in frames]
        cols = 10
        tw = max(t.width for t in tiles) + 6
        rows = (len(tiles) + cols - 1) // cols
        sheet = Image.new("RGB", (cols * tw, rows * (th + 16)), (70, 70, 70))
        d = ImageDraw.Draw(sheet)
        for i, (tile, t) in enumerate(zip(tiles, times)):
            x, y = (i % cols) * tw, (i // cols) * (th + 16)
            sheet.paste(tile, (x, y + 16), tile)
            d.text((x + 2, y + 2), f"{i} {t:.2f}", fill=(255, 255, 0))
        sheet.save(os.path.join(args.out_dir, "contact.png"))
        print("wrote", os.path.join(args.out_dir, "contact.png"))
        return 0

    times = [float(v) for v in args.times.split(",")]
    frames = []
    for i, t in enumerate(times):
        img = process(grab(args.clip, t), crop, args.rope)
        img.save(os.path.join(args.out_dir, f"frame-{i:02d}.png"))
        frames.append(img)
    gap = 12
    w = max(f.width for f in frames) + gap
    h = max(f.height for f in frames)
    sheet = Image.new("RGBA", (w * len(frames), h), (0, 0, 0, 0))
    for i, f in enumerate(frames):
        sheet.paste(f, (i * w + (w - f.width) // 2, h - f.height), f)
    sheet.save(os.path.join(args.out_dir, "sheet.png"))
    print(f"wrote {len(frames)} frames and sheet.png to {args.out_dir}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

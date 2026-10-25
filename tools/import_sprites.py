#!/usr/bin/env python3
"""Turn a generated sprite sheet (green screen or transparent, any layout)
into a clean atlas of uniform cells the game can draw with drawSheetFrame.

  python3 tools/import_sprites.py SHEET.png OUT.png [options]

The sheet is keyed (the background colour is sampled from its border unless
--key R,G,B is given), split into frames by connected regions of foreground
(pieces that overlap horizontally, like a hat's pompom, are merged), the
frames are ordered left to right inside each row band, and every frame is
placed in a cell of the same size with its feet on the cell's baseline and
its body centred.  A JSON file next to OUT.png records each frame's source
box and anchor.

Options:
  --key R,G,B        background colour to remove (default: sampled)
  --tolerance N      colour distance that counts as background (default 70)
  --soft N           extra distance over which the edge fades (default 50)
  --columns N        cells per atlas row (default: all frames in one row)
  --select a,b,c     keep only these frame indices (0-based), in this order
  --skip-smaller N   drop regions whose box is under N pixels (default 24)
  --rows N           force N row bands when auto-grouping guesses wrong
  --crop x0,y0,x1,y1 use only this part of the sheet
  --pad N            transparent margin inside every cell (default 8)
  --baseline N       pixels from the cell bottom to the feet (default = pad)
  --preview P.png    also write a contact sheet with the frame numbers
  --scale F          resize every frame by F (to match another atlas)
  --anchor top       align frames by their top rows (hands on a bar) instead
  --strip-gray-top F remove grey pixels (a bar, a rope) in the top F of a frame
  --cut-above-body   drop a drawn bar and ropes above the hands
  --merge-x          merge regions that overlap horizontally (split figures)
"""
import argparse
import json
import math
import os
import sys

import numpy as np
from PIL import Image, ImageDraw


def sample_key(rgb):
    h, w, _ = rgb.shape
    border = np.concatenate([rgb[0, :], rgb[-1, :], rgb[:, 0], rgb[:, -1]])
    # The most common border colour (quantised to 8 levels per channel).
    q = (border // 32).astype(np.int32)
    codes = q[:, 0] * 64 + q[:, 1] * 8 + q[:, 2]
    best = np.bincount(codes).argmax()
    mask = codes == best
    return border[mask].mean(axis=0)


def key_alpha(rgb, key, tolerance, soft):
    diff = rgb.astype(np.float32) - key.astype(np.float32)
    dist = np.sqrt((diff * diff).sum(axis=2))
    alpha = np.clip((dist - tolerance) / max(1.0, soft), 0.0, 1.0)
    return alpha


def chroma_alpha(rgb, threshold, soft):
    # A pure-green screen by its green excess (G above the larger of R and
    # B): the screen sits near 220, painted greens (olive trousers, grass)
    # well under 120, so the cut between them keeps the clothes.
    f = rgb.astype(np.float32)
    excess = f[..., 1] - np.maximum(f[..., 0], f[..., 2])
    return np.clip((threshold - excess) / max(1.0, soft), 0.0, 1.0)


def despill(rgb, key, alpha=None):
    # For a green key, pull the green channel down to the max of red/blue
    # where it dominates (the fringe left by the key).  Given the alpha,
    # only the fringe itself -- the pixels within a few of the key -- is
    # touched, so green clothes keep their colour.
    out = rgb.astype(np.float32).copy()
    kr, kg, kb = key
    if kg > kr and kg > kb:
        limit = np.maximum(out[..., 0], out[..., 2])
        excess = out[..., 1] - limit
        mask = excess > 0
        if alpha is not None:
            def grown(seed, steps):
                near = seed.copy()
                for _ in range(steps):
                    g = near.copy()
                    g[1:, :] |= near[:-1, :]; g[:-1, :] |= near[1:, :]
                    g[:, 1:] |= near[:, :-1]; g[:, :-1] |= near[:, 1:]
                    near = g
                return near
            fringe = alpha < 0.98
            # The ink outline is drawn against the screen, so its dark
            # pixels come out olive: neutralise dark greenish pixels within
            # the outline's reach of the edge as well as the fringe itself.
            dark = (np.max(out, axis=2) < 120) & (excess > 3)
            mask &= grown(fringe, 3) | (dark & grown(fringe, 10))
            out[..., 1][mask] = limit[mask]
            # What is left of a screen-tinted outline is a dark olive (red
            # and green alike, little blue): make those pixels plain dark
            # grey so the edge reads as ink.
            olive = grown(fringe, 10) & (np.max(out, axis=2) < 110) & \
                    (out[..., 2] < np.minimum(out[..., 0], out[..., 1]) - 8)
            grey = out.mean(axis=2) * 0.85
            for c in range(3):
                out[..., c][olive] = grey[olive]
            return np.clip(out, 0, 255).astype(np.uint8)
        out[..., 1][mask] = limit[mask]
    return np.clip(out, 0, 255).astype(np.uint8)


def label_regions(mask):
    """4-connected labelling without scipy: a simple union-find sweep."""
    h, w = mask.shape
    labels = np.zeros((h, w), dtype=np.int32)
    parent = [0]
    next_label = 1

    def find(a):
        while parent[a] != a:
            parent[a] = parent[parent[a]]
            a = parent[a]
        return a

    def union(a, b):
        ra, rb = find(a), find(b)
        if ra != rb:
            parent[max(ra, rb)] = min(ra, rb)

    for y in range(h):
        row = mask[y]
        for x in range(w):
            if not row[x]:
                continue
            up = labels[y - 1, x] if y > 0 else 0
            left = labels[y, x - 1] if x > 0 else 0
            if up and left:
                labels[y, x] = min(up, left)
                union(up, left)
            elif up or left:
                labels[y, x] = up or left
            else:
                labels[y, x] = next_label
                parent.append(next_label)
                next_label += 1
    # Resolve.
    flat = labels.reshape(-1)
    nonzero = flat > 0
    roots = np.array([find(i) for i in range(next_label)], dtype=np.int32)
    flat[nonzero] = roots[flat[nonzero]]
    return labels


def boxes_from_labels(labels, min_fill=0.08):
    """Boxes of the labelled regions; outlines and rules (regions that fill
    less than min_fill of their own box) are dropped."""
    boxes = {}
    counts = {}
    ys, xs = np.nonzero(labels)
    for y, x in zip(ys, xs):
        l = labels[y, x]
        counts[l] = counts.get(l, 0) + 1
        b = boxes.get(l)
        if b is None:
            boxes[l] = [x, y, x, y]
        else:
            if x < b[0]: b[0] = x
            if y < b[1]: b[1] = y
            if x > b[2]: b[2] = x
            if y > b[3]: b[3] = y
    out = []
    for l, b in boxes.items():
        area = (b[2] - b[0] + 1) * (b[3] - b[1] + 1)
        if counts[l] < min_fill * area:
            continue
        out.append((tuple(int(v) for v in b), int(l)))
    return out


def split_by_gaps(boxes, mask, gap=2):
    """Frames that touch are one region; cut such a region where its box has
    columns without any foreground."""
    out = []
    for x0, y0, x1, y1 in boxes:
        occupancy = mask[y0:y1 + 1, x0:x1 + 1].any(axis=0)
        pieces = []
        start = None
        empty = 0
        for i, filled in enumerate(occupancy):
            if filled:
                if start is None:
                    start = i
                empty = 0
            else:
                empty += 1
                if start is not None and empty >= gap:
                    pieces.append((start, i - empty))
                    start = None
        if start is not None:
            pieces.append((start, len(occupancy) - 1))
        for a, b in pieces:
            sub = mask[y0:y1 + 1, x0 + a:x0 + b + 1]
            rows = np.nonzero(sub.any(axis=1))[0]
            out.append((x0 + a, y0 + int(rows[0]), x0 + b, y0 + int(rows[-1])))
    return out


def merge_small_pieces(boxes, ratio=0.2):
    """A detached piece (a pompom, a hand) joins the nearest big frame whose
    x-range it overlaps; big frames never merge with each other."""
    if not boxes:
        return []
    areas = sorted(((b[2] - b[0] + 1) * (b[3] - b[1] + 1)) for b in boxes)
    typical = areas[len(areas) // 2] if len(areas) < 4 else float(np.mean(areas[-len(areas) // 2:]))
    big = [b for b in boxes if (b[2] - b[0] + 1) * (b[3] - b[1] + 1) >= ratio * typical]
    small = [b for b in boxes if b not in big]
    big = [list(b) for b in big]
    for piece in small:
        best = None
        best_score = None
        pcx = (piece[0] + piece[2]) / 2.0
        pcy = (piece[1] + piece[3]) / 2.0
        for b in big:
            overlap = min(piece[2], b[2]) - max(piece[0], b[0])
            dx = 0 if overlap >= 0 else -overlap
            dy = 0 if b[1] <= pcy <= b[3] else min(abs(pcy - b[1]), abs(pcy - b[3]))
            score = dx * 2 + dy
            if best is None or score < best_score:
                best, best_score = b, score
        if best is not None and best_score < 60:
            best[0] = min(best[0], piece[0]); best[1] = min(best[1], piece[1])
            best[2] = max(best[2], piece[2]); best[3] = max(best[3], piece[3])
    return [tuple(b) for b in big]


def group_rows(boxes, rows=None):
    """Order frames by row band (by vertical centre), then by x."""
    if not boxes:
        return []
    centres = sorted(((b[1] + b[3]) / 2.0, b) for b in boxes)
    bands = []
    heights = [b[3] - b[1] for b in boxes]
    typical = float(np.median(heights))
    for c, b in centres:
        if bands and abs(c - bands[-1][0]) < typical * 0.6:
            bands[-1][1].append(b)
            bands[-1][0] = float(np.mean([(x[1] + x[3]) / 2.0 for x in bands[-1][1]]))
        else:
            bands.append([c, [b]])
    if rows is not None and len(bands) != rows:
        # Fall back to equal bands by vertical position.
        ys = [b[1] for b in boxes]
        y0, y1 = min(ys), max(b[3] for b in boxes)
        bands = [[0, []] for _ in range(rows)]
        for b in boxes:
            c = (b[1] + b[3]) / 2.0
            i = min(rows - 1, int((c - y0) / max(1, (y1 - y0)) * rows))
            bands[i][1].append(b)
    ordered = []
    for _, group in bands:
        ordered.extend(sorted(group, key=lambda b: b[0]))
    return ordered


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("sheet")
    ap.add_argument("out")
    ap.add_argument("--key")
    ap.add_argument("--chroma", action="store_true",
                    help="key a pure-green screen by its green excess instead of "
                         "colour distance, and despill only the fringe: keeps "
                         "green and olive clothes")
    ap.add_argument("--tolerance", type=float, default=70)
    ap.add_argument("--soft", type=float, default=50)
    ap.add_argument("--columns", type=int)
    ap.add_argument("--select")
    ap.add_argument("--skip-smaller", type=int, default=24)
    ap.add_argument("--rows", type=int)
    ap.add_argument("--crop")
    ap.add_argument("--pad", type=int, default=8)
    ap.add_argument("--baseline", type=int)
    ap.add_argument("--preview")
    ap.add_argument("--scale", type=float, default=1.0,
                    help="resize every frame by this factor (to match another atlas)")
    ap.add_argument("--anchor", choices=["feet", "top", "center"], default="feet",
                    help="feet: feet on the baseline, body centred; top: the top "
                         "rows (hands on a bar) at the cell top, centred on them")
    ap.add_argument("--keep-bar", action="store_true",
                    help="with --cut-above-body: keep the drawn bar, cut only above it")
    ap.add_argument("--merge-x", action="store_true",
                    help="merge every region that overlaps another horizontally "
                         "(a figure split into pieces by a thin waist)")
    ap.add_argument("--cut-above-body", action="store_true",
                    help="drop a drawn bar and ropes above the hands: the frame "
                         "starts at the first band of rows at least 14 deep that "
                         "is wider than a fifth of the frame")
    ap.add_argument("--strip-gray-top", type=float, default=0.0,
                    help="remove grey pixels (a drawn bar or rope) in this top "
                         "fraction of every frame before anchoring")
    args = ap.parse_args()

    image = Image.open(args.sheet).convert("RGBA")
    if args.crop:
        x0, y0, x1, y1 = (int(v) for v in args.crop.split(","))
        image = image.crop((x0, y0, x1, y1))
    rgba = np.array(image)
    rgb = rgba[..., :3]
    alpha_in = rgba[..., 3].astype(np.float32) / 255.0

    has_alpha = (alpha_in < 0.5).mean() > 0.05
    if args.chroma:
        key = np.array([0.0, 255.0, 0.0])
        keyed = chroma_alpha(rgb, 150.0, 40.0)
        alpha = np.minimum(alpha_in, keyed) if has_alpha else keyed
        rgb = despill(rgb, key, alpha)
        print("chroma key by green excess")
    elif has_alpha and not args.key:
        # A sheet that already has transparency is taken as is: keying its
        # colours would eat olive or khaki clothes next to a green key.
        alpha = alpha_in
        print("sheet has transparency: no colour key")
    else:
        if args.key:
            key = np.array([float(v) for v in args.key.split(",")])
        else:
            key = sample_key(rgb)
        keyed = key_alpha(rgb, key, args.tolerance, args.soft)
        alpha = np.minimum(alpha_in, keyed) if has_alpha else keyed
        rgb = despill(rgb, key)
        print(f"key colour {key.round().astype(int).tolist()}")

    mask = alpha > 0.5
    # Shrink the mask for labelling so faint fringes do not join frames.
    labels = label_regions(mask)
    boxes = [b for b, _ in boxes_from_labels(labels)
             if (b[2] - b[0]) >= args.skip_smaller and (b[3] - b[1]) >= args.skip_smaller]
    boxes = split_by_gaps(boxes, mask)
    if args.merge_x:
        merged = True
        while merged:
            merged = False
            out = []
            for b in sorted(boxes):
                for i, o in enumerate(out):
                    if b[0] <= o[2] and o[0] <= b[2]:
                        out[i] = (min(o[0], b[0]), min(o[1], b[1]), max(o[2], b[2]), max(o[3], b[3]))
                        merged = True
                        break
                else:
                    out.append(b)
            boxes = out
    boxes = merge_small_pieces(boxes)
    boxes = [b for b in boxes if (b[2] - b[0]) >= args.skip_smaller and (b[3] - b[1]) >= args.skip_smaller]
    boxes = group_rows(boxes, args.rows)
    print(f"{len(boxes)} frames found")
    if args.select:
        picked = [int(v) for v in args.select.split(",")]
        boxes = [boxes[i] for i in picked]

    # Neighbouring frames may reach into each other's box (an arm over the
    # next frame), so every crop keeps only the regions centred inside it.
    all_boxes = boxes_from_labels(labels, min_fill=0.0)
    label_boxes = {}
    for b, l in all_boxes:
        label_boxes[l] = b
    frames = []
    for b in boxes:
        x0, y0, x1, y1 = b
        keep = np.zeros(labels.max() + 1, dtype=bool)
        for l, lb in label_boxes.items():
            cx = (lb[0] + lb[2]) / 2.0
            cy = (lb[1] + lb[3]) / 2.0
            if x0 - 1 <= cx <= x1 + 1 and y0 - 1 <= cy <= y1 + 1:
                keep[l] = True
        region = keep[labels[y0:y1 + 1, x0:x1 + 1]]
        # Grow the region by a few pixels so the soft edge survives.
        grown = region.copy()
        for _ in range(3):
            g = grown.copy()
            g[1:, :] |= grown[:-1, :]; g[:-1, :] |= grown[1:, :]
            g[:, 1:] |= grown[:, :-1]; g[:, :-1] |= grown[:, 1:]
            grown = g
        crop_rgb = rgb[y0:y1 + 1, x0:x1 + 1]
        crop_a = (alpha[y0:y1 + 1, x0:x1 + 1] * grown * 255).astype(np.uint8)
        frame = np.dstack([crop_rgb, crop_a])
        if args.cut_above_body:
            widths = (crop_a > 40).sum(axis=1)
            limit = 0.2 * crop_a.shape[1]
            start = 0
            for r in range(crop_a.shape[0] - 14):
                if all(widths[r:r + 14] > limit):
                    start = max(0, r - 2)
                    break
            # A drawn bar: a band wider than half the frame, under 18 rows
            # deep, in the top quarter.  The frame starts under it.
            wide = widths > 0.5 * crop_a.shape[1]
            r = start
            top_quarter = crop_a.shape[0] // 4
            while r < top_quarter and not wide[r]:
                r += 1
            if r < top_quarter:
                end = r
                while end < crop_a.shape[0] and wide[end]:
                    end += 1
                if end - r <= 18:
                    start = r if args.keep_bar else end
            crop_rgb = crop_rgb[start:]
            crop_a = crop_a[start:]
            cols = np.nonzero(crop_a.max(axis=0) > 40)[0]
            if len(cols):
                crop_rgb = crop_rgb[:, cols[0]:cols[-1] + 1]
                crop_a = crop_a[:, cols[0]:cols[-1] + 1]
            frame = np.dstack([crop_rgb, crop_a])
        if args.strip_gray_top > 0:
            top = int(crop_rgb.shape[0] * args.strip_gray_top)
            part = crop_rgb[:top].astype(np.int32)
            grey = ((np.abs(part[..., 0] - part[..., 1]) < 22) &
                    (np.abs(part[..., 1] - part[..., 2]) < 22) &
                    (part.max(axis=2) > 110) & (part.max(axis=2) < 245))
            crop_a = crop_a.copy()
            crop_a[:top][grey] = 0
            # Re-trim the frame to what is left.
            rows = np.nonzero(crop_a.max(axis=1) > 40)[0]
            cols = np.nonzero(crop_a.max(axis=0) > 40)[0]
            if len(rows) and len(cols):
                crop_rgb = crop_rgb[rows[0]:rows[-1] + 1, cols[0]:cols[-1] + 1]
                crop_a = crop_a[rows[0]:rows[-1] + 1, cols[0]:cols[-1] + 1]
            frame = np.dstack([crop_rgb, crop_a])
        image_frame = Image.fromarray(frame, "RGBA")
        if args.scale != 1.0:
            image_frame = image_frame.resize(
                (max(1, round(image_frame.width * args.scale)),
                 max(1, round(image_frame.height * args.scale))), Image.LANCZOS)
        frames.append((b, image_frame))

    widths = [f.width for _, f in frames]
    heights = [f.height for _, f in frames]
    cell_w = max(widths) + 2 * args.pad
    cell_h = max(heights) + 2 * args.pad

    def hands_of(frame):
        """The x of the hands: the middle of the frame's top rows."""
        fa = np.array(frame)[..., 3]
        head = fa[:max(1, frame.height // 10)]
        cols = np.nonzero(head.max(axis=0) > 40)[0]
        return (cols[0] + cols[-1]) / 2.0 if len(cols) else frame.width / 2.0

    if args.anchor == "top":
        # Hung by the hands, a frame reaches further to one side than the
        # other (legs swung back): the cell must hold the longer reach on
        # both sides or the overhang lands in the neighbouring cell.
        reach = max(max(hands_of(f), f.width - hands_of(f)) for _, f in frames)
        cell_w = int(math.ceil(2 * reach)) + 2 * args.pad
    baseline = args.baseline if args.baseline is not None else args.pad
    columns = args.columns or len(frames)
    rows = int(math.ceil(len(frames) / columns))
    atlas = Image.new("RGBA", (cell_w * columns, cell_h * rows), (0, 0, 0, 0))
    manifest = {"source": os.path.basename(args.sheet), "cell": [cell_w, cell_h],
                "columns": columns, "rows": rows, "baseline": baseline,
                "frames": []}
    for index, (b, frame) in enumerate(frames):
        if args.anchor == "center":
            cx = (index % columns) * cell_w + (cell_w - frame.width) // 2
            cy = (index // columns) * cell_h + (cell_h - frame.height) // 2
        elif args.anchor == "top":
            # Centre on the top rows of the frame (the hands), hang from the
            # cell top.
            hands_x = hands_of(frame)
            cx = (index % columns) * cell_w + int(round(cell_w / 2.0 - hands_x))
            cy = (index // columns) * cell_h + args.pad
        else:
            cx = (index % columns) * cell_w + (cell_w - frame.width) // 2
            cy = (index // columns) * cell_h + cell_h - baseline - frame.height
        atlas.paste(frame, (cx, cy))
        manifest["frames"].append({"index": index, "source": list(b),
                                   "cell": [index % columns, index // columns],
                                   "size": [frame.width, frame.height],
                                   "feet": [cx + frame.width // 2, cy + frame.height]})
    atlas.save(args.out)
    with open(os.path.splitext(args.out)[0] + ".json", "w") as handle:
        json.dump(manifest, handle, indent=1)
    print(f"wrote {args.out}: {columns}x{rows} cells of {cell_w}x{cell_h}")

    if args.preview:
        preview = Image.new("RGBA", atlas.size, (70, 110, 70, 255))
        preview.alpha_composite(atlas)
        draw = ImageDraw.Draw(preview)
        for index in range(len(frames)):
            x = (index % columns) * cell_w
            y = (index // columns) * cell_h
            draw.rectangle([x, y, x + cell_w - 1, y + cell_h - 1], outline=(255, 255, 255, 120))
            draw.text((x + 4, y + 4), str(index), fill=(255, 255, 0, 255))
            draw.line([x, y + cell_h - baseline, x + cell_w, y + cell_h - baseline], fill=(255, 80, 80, 200))
        preview.convert("RGB").save(args.preview)
    return 0


if __name__ == "__main__":
    sys.exit(main())

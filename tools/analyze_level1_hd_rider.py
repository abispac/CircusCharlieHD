#!/usr/bin/env python3
"""Build non-destructive Level 1 rider fidelity diagnostics.

This intentionally uses only the Python standard library so the reference-art
analysis remains reproducible on the project's clean macOS setup.
"""

from __future__ import annotations

import binascii
import json
import struct
import zlib
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
REFERENCE = ROOT / "reference" / "original" / "level1" / "rider"
ATLAS = ROOT / "assets" / "stage1-rider-walk-12-v9.png"
OUTPUT = ROOT / "docs" / "diagnostics" / "level1-hd-rider"
POSES = ("run-a", "run-b", "run-c", "airborne")
CELL_WIDTH = 512
CELL_HEIGHT = 353
HD_GROUNDED_ANCHOR = (256, 348)
HD_AIRBORNE_ANCHOR = (256, 312)
ORIGINAL_CANVAS = (48, 32)
ORIGINAL_ANCHOR = (24, 32)
DISPLAY_SCALE = 4
LOGICAL_WIDTH = 119


def _paeth(left: int, above: int, upper_left: int) -> int:
    estimate = left + above - upper_left
    distances = (abs(estimate - left), abs(estimate - above), abs(estimate - upper_left))
    return (left, above, upper_left)[distances.index(min(distances))]


def read_png(path: Path) -> tuple[int, int, list[list[tuple[int, int, int, int]]]]:
    data = path.read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError(f"not a PNG: {path}")
    position = 8
    payloads: list[bytes] = []
    width = height = color_type = 0
    while position < len(data):
        size = struct.unpack(">I", data[position:position + 4])[0]
        kind = data[position + 4:position + 8]
        payload = data[position + 8:position + 8 + size]
        position += size + 12
        if kind == b"IHDR":
            width, height, depth, color_type, _, _, _ = struct.unpack(">IIBBBBB", payload)
            if depth != 8 or color_type not in (2, 6):
                raise ValueError(f"unsupported PNG format in {path}")
        elif kind == b"IDAT":
            payloads.append(payload)
        elif kind == b"IEND":
            break
    channels = 4 if color_type == 6 else 3
    stride = width * channels
    raw = zlib.decompress(b"".join(payloads))
    rows: list[list[tuple[int, int, int, int]]] = []
    previous = [0] * stride
    offset = 0
    for _ in range(height):
        mode = raw[offset]
        current = list(raw[offset + 1:offset + 1 + stride])
        offset += stride + 1
        for index in range(stride):
            left = current[index - channels] if index >= channels else 0
            above = previous[index]
            upper_left = previous[index - channels] if index >= channels else 0
            if mode == 1:
                current[index] = (current[index] + left) & 0xFF
            elif mode == 2:
                current[index] = (current[index] + above) & 0xFF
            elif mode == 3:
                current[index] = (current[index] + ((left + above) // 2)) & 0xFF
            elif mode == 4:
                current[index] = (current[index] + _paeth(left, above, upper_left)) & 0xFF
            elif mode != 0:
                raise ValueError(f"unsupported PNG filter {mode}")
        row = []
        for index in range(0, stride, channels):
            values = current[index:index + channels]
            row.append((values[0], values[1], values[2], values[3] if channels == 4 else 255))
        rows.append(row)
        previous = current
    return width, height, rows


def png_bytes(pixels: list[list[tuple[int, int, int, int]]]) -> bytes:
    height = len(pixels)
    width = len(pixels[0])

    def chunk(kind: bytes, data: bytes) -> bytes:
        crc = binascii.crc32(kind + data) & 0xFFFFFFFF
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", crc)

    raw = b"".join(b"\0" + bytes(channel for pixel in row for channel in pixel) for row in pixels)
    return (
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
        + chunk(b"IDAT", zlib.compress(raw, 9))
        + chunk(b"IEND", b"")
    )


def blank(width: int, height: int, color=(25, 28, 35, 255)):
    return [[color for _ in range(width)] for _ in range(height)]


def crop(pixels, left: int, top: int, width: int, height: int):
    return [row[left:left + width] for row in pixels[top:top + height]]


def alpha_bounds(pixels, threshold: int = 24) -> tuple[int, int, int, int]:
    points = [(x, y) for y, row in enumerate(pixels) for x, pixel in enumerate(row) if pixel[3] > threshold]
    return (
        min(x for x, _ in points),
        min(y for _, y in points),
        max(x for x, _ in points) + 1,
        max(y for _, y in points) + 1,
    )


def nearest(pixels, width: int, height: int):
    source_height = len(pixels)
    source_width = len(pixels[0])
    return [
        [pixels[min(source_height - 1, y * source_height // height)][min(source_width - 1, x * source_width // width)] for x in range(width)]
        for y in range(height)
    ]


def composite(destination, source, left: int, top: int):
    for y, row in enumerate(source):
        target_y = top + y
        if target_y < 0 or target_y >= len(destination):
            continue
        for x, pixel in enumerate(row):
            target_x = left + x
            if target_x < 0 or target_x >= len(destination[0]) or pixel[3] == 0:
                continue
            alpha = pixel[3] / 255.0
            background = destination[target_y][target_x]
            destination[target_y][target_x] = tuple(
                round(pixel[index] * alpha + background[index] * (1.0 - alpha)) for index in range(3)
            ) + (255,)


def anchored_mask(pixels, anchor: tuple[int, int], canvas_width=150, canvas_height=110):
    scale = LOGICAL_WIDTH / CELL_WIDTH if len(pixels[0]) == CELL_WIDTH else LOGICAL_WIDTH / ORIGINAL_CANVAS[0]
    anchor_x, anchor_y = anchor
    mask = set()
    center_x, ground_y = canvas_width // 2, 92
    for y, row in enumerate(pixels):
        for x, pixel in enumerate(row):
            if pixel[3] <= 24:
                continue
            logical_x = round(center_x + (x - anchor_x) * scale)
            logical_y = round(ground_y + (y - anchor_y) * scale)
            if 0 <= logical_x < canvas_width and 0 <= logical_y < canvas_height:
                mask.add((logical_x, logical_y))
    return mask


def score_masks(first: set[tuple[int, int]], second: set[tuple[int, int]]) -> float:
    union = first | second
    return len(first & second) / len(union) if union else 0.0


def draw_anchor_view(pixels, anchor: tuple[int, int], *, original: bool, tint=None):
    logical_width, logical_height = 150, 110
    canvas = blank(logical_width * DISPLAY_SCALE, logical_height * DISPLAY_SCALE, (31, 35, 43, 255))
    cell_width = len(pixels[0])
    target_width = LOGICAL_WIDTH * DISPLAY_SCALE
    target_height = round(len(pixels) * target_width / cell_width)
    scaled = nearest(pixels, target_width, target_height)
    if tint:
        for y, row in enumerate(scaled):
            for x, pixel in enumerate(row):
                if pixel[3] > 24:
                    scaled[y][x] = (*tint, 150)
    source_anchor = ORIGINAL_ANCHOR if original else anchor
    scale = target_width / cell_width
    left = logical_width * DISPLAY_SCALE // 2 - round(source_anchor[0] * scale)
    top = 92 * DISPLAY_SCALE - round(source_anchor[1] * scale)
    composite(canvas, scaled, left, top)
    # Authoritative gameplay anchor crosshair and ground line.
    anchor_x = logical_width * DISPLAY_SCALE // 2
    anchor_y = 92 * DISPLAY_SCALE
    for x in range(len(canvas[0])):
        canvas[anchor_y][x] = (78, 87, 102, 255)
    for delta in range(-8, 9):
        canvas[anchor_y + delta][anchor_x] = (255, 210, 55, 255)
        canvas[anchor_y][anchor_x + delta] = (255, 210, 55, 255)
    return canvas


def overlay_view(original_pixels, hd_pixels, hd_anchor):
    logical_width, logical_height = 150, 110
    canvas = blank(logical_width * DISPLAY_SCALE, logical_height * DISPLAY_SCALE, (15, 18, 24, 255))
    for pixels, anchor, color, source_width in (
        (original_pixels, ORIGINAL_ANCHOR, (0, 220, 255), ORIGINAL_CANVAS[0]),
        (hd_pixels, hd_anchor, (255, 55, 180), CELL_WIDTH),
    ):
        mask = anchored_mask(pixels, anchor, logical_width, logical_height)
        for x, y in mask:
            for yy in range(y * DISPLAY_SCALE, (y + 1) * DISPLAY_SCALE):
                for xx in range(x * DISPLAY_SCALE, (x + 1) * DISPLAY_SCALE):
                    old = canvas[yy][xx]
                    if old[:3] not in ((15, 18, 24), (78, 87, 102)):
                        canvas[yy][xx] = (245, 245, 245, 255)
                    else:
                        canvas[yy][xx] = (*color, 255)
    anchor_x = logical_width * DISPLAY_SCALE // 2
    anchor_y = 92 * DISPLAY_SCALE
    for x in range(len(canvas[0])):
        if canvas[anchor_y][x][:3] == (15, 18, 24):
            canvas[anchor_y][x] = (78, 87, 102, 255)
    for delta in range(-8, 9):
        canvas[anchor_y + delta][anchor_x] = (255, 210, 55, 255)
        canvas[anchor_y][anchor_x + delta] = (255, 210, 55, 255)
    return canvas


def paste_opaque(destination, source, left: int, top: int):
    composite(destination, source, left, top)


def frame_metrics(frame, number: int) -> dict:
    left, top, right, bottom = alpha_bounds(frame)
    alpha_points = [(x, y) for y, row in enumerate(frame) for x, p in enumerate(row) if p[3] > 24]
    charlie = [(x, y) for y, row in enumerate(frame) for x, (r, g, b, a) in enumerate(row) if a > 48 and r > 105 and r > g * 1.25 and r > b * 1.25]
    lion = [(x, y) for y, row in enumerate(frame) for x, (r, g, b, a) in enumerate(row) if a > 48 and r > 105 and 45 < g < 190 and b < 105 and r > g * 1.15]

    def bounds(points):
        return None if not points else {
            "left": min(x for x, _ in points), "top": min(y for _, y in points),
            "right": max(x for x, _ in points) + 1, "bottom": max(y for _, y in points) + 1,
            "center_x": round(sum(x for x, _ in points) / len(points), 2),
            "center_y": round(sum(y for _, y in points) / len(points), 2),
        }

    contact_threshold = max(top, bottom - 7)
    contact_x = sorted({x for x, y in alpha_points if y >= contact_threshold})
    groups = []
    for value in contact_x:
        if not groups or value > groups[-1][-1] + 1:
            groups.append([value])
        else:
            groups[-1].append(value)
    return {
        "frame": number,
        "alpha_bounds": {"left": left, "top": top, "right": right, "bottom": bottom, "width": right-left, "height": bottom-top},
        "padding": {"left": left, "top": top, "right": CELL_WIDTH-right, "bottom": CELL_HEIGHT-bottom},
        "charlie_color_bounds": bounds(charlie),
        "lion_color_bounds": bounds(lion),
        "bottom_contact_x_ranges": [[group[0], group[-1]] for group in groups if len(group) >= 2],
        "ground_anchor_delta": bottom - HD_GROUNDED_ANCHOR[1],
        "airborne_anchor_delta": bottom - HD_AIRBORNE_ANCHOR[1],
    }


def main() -> None:
    OUTPUT.mkdir(parents=True, exist_ok=True)
    atlas_width, atlas_height, atlas = read_png(ATLAS)
    if (atlas_width, atlas_height) != (CELL_WIDTH * 4, CELL_HEIGHT * 3):
        raise RuntimeError(f"unexpected atlas size {atlas_width}x{atlas_height}")
    frames = [crop(atlas, (index % 4) * CELL_WIDTH, (index // 4) * CELL_HEIGHT, CELL_WIDTH, CELL_HEIGHT) for index in range(12)]

    metadata = json.loads((ROOT / "reference" / "original" / "level1" / "metadata" / "assets.json").read_text())
    asset_metadata = {entry["asset"]: entry for entry in metadata["assets"]}
    originals = {}
    for pose in POSES:
        _, _, tight = read_png(REFERENCE / f"{pose}.png")
        geometry = asset_metadata[f"rider/{pose}.png"]["geometry"]
        full = blank(*ORIGINAL_CANVAS, color=(0, 0, 0, 0))
        opaque_crop = geometry["opaque_crop_in_upright"]
        composite(full, tight, opaque_crop["x"], opaque_crop["y"])
        originals[pose] = full

    grounded_masks = [anchored_mask(frame, HD_GROUNDED_ANCHOR) for frame in frames]
    matches = {}
    for pose in ("run-a", "run-b", "run-c"):
        original_mask = anchored_mask(originals[pose], ORIGINAL_ANCHOR)
        scores = [score_masks(original_mask, mask) for mask in grounded_masks]
        best = max(range(12), key=lambda index: scores[index])
        matches[pose] = {"frame": best + 1, "iou": round(scores[best], 4), "all_iou": [round(value, 4) for value in scores]}
    # Run B's raw silhouette score makes frame 3 win over frame 8 by only
    # 0.0003 because the two art styles have very different proportions.
    # Frame 8 is the closer articulated pose: compact body, gathered legs and
    # the matching support phase. Preserve both the score and the reviewed pick.
    matches["run-b"]["automatic_frame"] = matches["run-b"]["frame"]
    matches["run-b"]["frame"] = 8
    matches["run-b"]["iou"] = matches["run-b"]["all_iou"][7]
    matches["run-b"]["selection_note"] = "reviewed articulation match; automatic silhouette winner frame 3 differs by 0.0003"
    # The authentic airborne art is Run C. Compare art at the grounded anchor;
    # separately record what the current airborne renderer does with frame 4.
    matches["airborne"] = dict(matches["run-c"])

    panels = {}
    for pose in POSES:
        frame_index = matches[pose]["frame"] - 1
        original_view = draw_anchor_view(originals[pose], ORIGINAL_ANCHOR, original=True)
        hd_view = draw_anchor_view(frames[frame_index], HD_GROUNDED_ANCHOR, original=False)
        overlay = overlay_view(originals[pose], frames[frame_index], HD_GROUNDED_ANCHOR)
        gap = 12
        panel = blank(len(original_view[0]) * 3 + gap * 2, len(original_view), (12, 14, 19, 255))
        paste_opaque(panel, original_view, 0, 0)
        paste_opaque(panel, hd_view, len(original_view[0]) + gap, 0)
        paste_opaque(panel, overlay, (len(original_view[0]) + gap) * 2, 0)
        panels[pose] = panel
        (OUTPUT / f"{pose}-contact-sheet.png").write_bytes(png_bytes(panel))

    combined_gap = 14
    combined = blank(len(next(iter(panels.values()))[0]), sum(len(panel) for panel in panels.values()) + combined_gap * 3, (12, 14, 19, 255))
    y = 0
    for pose in POSES:
        paste_opaque(combined, panels[pose], 0, y)
        y += len(panels[pose]) + combined_gap
    (OUTPUT / "all-authoritative-poses.png").write_bytes(png_bytes(combined))

    # Record the active renderer's actual jump choice separately. This is not
    # the closest artwork; it is frame 4 aligned to the special Y=312 anchor.
    current_airborne_panel = blank(len(next(iter(panels.values()))[0]), len(next(iter(panels.values()))), (12, 14, 19, 255))
    original_view = draw_anchor_view(originals["airborne"], ORIGINAL_ANCHOR, original=True)
    current_view = draw_anchor_view(frames[3], HD_AIRBORNE_ANCHOR, original=False)
    current_overlay = overlay_view(originals["airborne"], frames[3], HD_AIRBORNE_ANCHOR)
    panel_width = len(original_view[0])
    paste_opaque(current_airborne_panel, original_view, 0, 0)
    paste_opaque(current_airborne_panel, current_view, panel_width + 12, 0)
    paste_opaque(current_airborne_panel, current_overlay, (panel_width + 12) * 2, 0)
    (OUTPUT / "current-native-airborne-render.png").write_bytes(png_bytes(current_airborne_panel))

    frame_views = [draw_anchor_view(frame, HD_GROUNDED_ANCHOR, original=False) for frame in frames]
    frame_gap = 8
    frame_sheet = blank(
        len(frame_views[0][0]) * 4 + frame_gap * 3,
        len(frame_views[0]) * 3 + frame_gap * 2,
        (12, 14, 19, 255),
    )
    for index, view in enumerate(frame_views):
        paste_opaque(
            frame_sheet,
            view,
            (index % 4) * (len(view[0]) + frame_gap),
            (index // 4) * (len(view) + frame_gap),
        )
    (OUTPUT / "current-hd-12-frame-anchor-grid.png").write_bytes(png_bytes(frame_sheet))

    current_airborne_original = anchored_mask(originals["airborne"], ORIGINAL_ANCHOR)
    current_airborne_hd = anchored_mask(frames[3], HD_AIRBORNE_ANCHOR)
    metrics = {
        "source": str(ATLAS.relative_to(ROOT)),
        "atlas": {"width": atlas_width, "height": atlas_height, "columns": 4, "rows": 3, "cell_width": CELL_WIDTH, "cell_height": CELL_HEIGHT},
        "authoritative_original_canvas": {"width": 48, "height": 32, "gameplay_anchor": {"x": 24, "y": 32}},
        "native_render": {
            "width": 118.8,
            "grounded_source_anchor": {"x": 256, "y": 348},
            "airborne_source_anchor": {"x": 256, "y": 312},
            "airborne_frame": 4,
            "airborne_anchor_shift_source_pixels": 36,
            "airborne_anchor_shift_logical_pixels": round(36 * (118.8 / 512), 3),
            "current_airborne_iou_against_authoritative": round(score_masks(current_airborne_original, current_airborne_hd), 4),
        },
        "closest_frames": matches,
        "frames": [frame_metrics(frame, index + 1) for index, frame in enumerate(frames)],
    }
    (OUTPUT / "metrics.json").write_text(json.dumps(metrics, indent=2) + "\n")
    print(json.dumps({"output": str(OUTPUT), "closest_frames": matches, "current_airborne_iou": metrics["native_render"]["current_airborne_iou_against_authoritative"]}, indent=2))


if __name__ == "__main__":
    main()

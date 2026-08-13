#!/usr/bin/env python3
"""Create non-destructive structural guides for the three Level 1 rider poses."""

from __future__ import annotations

import json
from pathlib import Path

from analyze_level1_hd_rider import blank, composite, nearest, png_bytes, read_png


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "reference" / "original" / "level1" / "rider"
OUTPUT = ROOT / "docs" / "diagnostics" / "level1-hd-rider-reconstruction"
SOURCE_CANVAS = (48, 32)
SOURCE_ANCHOR = (24.0, 32.0)
FINAL_CANVAS = (1024, 768)
FINAL_ANCHOR = (512, 640)
CONTENT_SCALE = 16
GUIDE_SCALE = 16
GUIDE_MARGIN = 48


POSES = {
    "run-a": {
        "charlie": {
            "overall_bounds": (11, 0, 30, 25),
            "head_center": (21.0, 7.0), "head_bounds": (11, 0, 30, 14),
            "body_center": (20.5, 16.5), "body_bounds": (14, 10, 29, 24),
            "seat": (22.5, 21.5), "grip": (28.5, 13.5), "boot": (25.5, 22.5),
        },
        "lion": {
            "overall_bounds": (0, 9, 47, 32), "torso_bounds": (9, 13, 35, 25),
            "head_center": (39.0, 16.5), "head_bounds": (31, 9, 47, 24),
            "nose_muzzle": (45.0, 17.5), "shoulder": (32.0, 17.0),
            "hip": (13.5, 18.0), "tail_base": (10.0, 17.0), "tail_tip": (0.5, 24.5),
            "rear_paw_1": (15.0, 30.0), "rear_paw_2": (20.0, 30.0),
            "front_paw_1": (30.0, 31.0), "front_paw_2": (35.0, 31.0),
        },
        "back_line": [(9.5, 14.0), (16.0, 13.0), (24.5, 14.0), (31.5, 13.0)],
        "belly_line": [(11.5, 22.0), (18.0, 24.0), (26.5, 23.0), (31.0, 22.0)],
        "limbs": [((13.5, 20.0), (15.0, 30.0)), ((18.5, 21.5), (20.0, 30.0)),
                  ((29.5, 20.5), (30.0, 31.0)), ((33.0, 20.5), (35.0, 31.0))],
        "negative_spaces": {
            "between_rear_legs": (16, 24, 19, 30),
            "under_belly": (22, 24, 28, 30),
            "between_front_legs": (31, 24, 34, 30),
        },
    },
    "run-b": {
        "charlie": {
            "overall_bounds": (10, 0, 29, 25),
            "head_center": (20.0, 6.5), "head_bounds": (10, 0, 29, 13),
            "body_center": (19.5, 16.0), "body_bounds": (15, 9, 28, 24),
            "seat": (21.5, 21.5), "grip": (27.5, 12.5), "boot": (25.0, 23.0),
        },
        "lion": {
            "overall_bounds": (0, 9, 45, 32), "torso_bounds": (9, 13, 34, 25),
            "head_center": (38.0, 15.5), "head_bounds": (30, 9, 45, 23),
            "nose_muzzle": (43.5, 16.5), "shoulder": (31.0, 16.5),
            "hip": (12.5, 18.0), "tail_base": (8.5, 20.0), "tail_tip": (0.5, 22.0),
            "rear_paw_1": (10.5, 30.5), "rear_paw_2": (17.5, 30.5),
            "front_paw_1": (24.5, 29.5), "front_paw_2": (33.0, 29.5),
        },
        "back_line": [(9.0, 15.0), (15.0, 13.5), (23.0, 13.5), (30.5, 12.5)],
        "belly_line": [(10.0, 22.0), (17.0, 24.0), (25.0, 23.5), (30.0, 22.0)],
        "limbs": [((12.0, 20.0), (10.5, 30.5)), ((17.0, 22.0), (17.5, 30.5)),
                  ((28.0, 20.5), (24.5, 29.5)), ((31.0, 20.0), (33.0, 29.5))],
        "negative_spaces": {
            "between_rear_legs": (12, 25, 16, 31),
            "under_belly": (19, 25, 24, 29),
            "between_front_legs": (27, 24, 32, 29),
        },
    },
    "run-c": {
        "charlie": {
            "overall_bounds": (8, 0, 28, 25),
            "head_center": (18.5, 7.0), "head_bounds": (8, 0, 28, 14),
            "body_center": (18.5, 16.5), "body_bounds": (11, 10, 28, 24),
            "seat": (20.5, 21.5), "grip": (27.0, 13.0), "boot": (24.0, 22.5),
        },
        "lion": {
            "overall_bounds": (0, 9, 46, 29), "torso_bounds": (8, 13, 34, 25),
            "head_center": (38.0, 16.0), "head_bounds": (29, 9, 46, 24),
            "nose_muzzle": (44.5, 17.0), "shoulder": (31.0, 17.0),
            "hip": (12.0, 18.0), "tail_base": (8.0, 19.0), "tail_tip": (0.5, 21.0),
            "rear_paw_1": (2.0, 26.0), "rear_paw_2": (7.0, 27.0),
            "front_paw_1": (40.5, 27.0), "front_paw_2": (44.0, 25.0),
        },
        "back_line": [(8.0, 15.5), (15.0, 14.0), (23.5, 14.0), (30.0, 13.5)],
        "belly_line": [(9.0, 22.0), (18.0, 24.0), (27.0, 23.0), (31.0, 21.5)],
        "limbs": [((11.0, 20.0), (2.0, 26.0)), ((16.0, 22.0), (7.0, 27.0)),
                  ((30.0, 20.5), (40.5, 27.0)), ((32.0, 19.5), (44.0, 25.0))],
        "negative_spaces": {
            "between_rear_legs": (4, 23, 8, 27),
            "under_belly": (20, 24, 28, 27),
            "between_front_legs": (38, 23, 42, 27),
        },
    },
}


COLORS = {
    "anchor": (255, 218, 55, 255), "silhouette": (255, 255, 255, 255),
    "charlie": (0, 225, 255, 255), "lion": (255, 78, 185, 255),
    "skeleton": (90, 255, 125, 255), "negative": (181, 105, 255, 255),
}


def put(canvas, x, y, color):
    if 0 <= x < len(canvas[0]) and 0 <= y < len(canvas):
        canvas[y][x] = color


def line(canvas, first, second, color, width=2):
    x0, y0 = first; x1, y1 = second
    steps = max(1, int(max(abs(x1 - x0), abs(y1 - y0))))
    for step in range(steps + 1):
        x = round(x0 + (x1 - x0) * step / steps)
        y = round(y0 + (y1 - y0) * step / steps)
        for yy in range(y - width, y + width + 1):
            for xx in range(x - width, x + width + 1): put(canvas, xx, yy, color)


def circle(canvas, center, radius, color):
    cx, cy = center
    for y in range(round(cy - radius), round(cy + radius) + 1):
        for x in range(round(cx - radius), round(cx + radius) + 1):
            distance = (x - cx) ** 2 + (y - cy) ** 2
            if (radius - 2) ** 2 <= distance <= (radius + 1) ** 2:
                put(canvas, x, y, color)


def source_to_guide(point):
    return (GUIDE_MARGIN + point[0] * GUIDE_SCALE,
            GUIDE_MARGIN + point[1] * GUIDE_SCALE)


def normalize_point(point):
    return {
        "source": [point[0], point[1]],
        "anchor_relative": [round((point[0] - SOURCE_ANCHOR[0]) / 48.0, 6),
                            round((point[1] - SOURCE_ANCHOR[1]) / 32.0, 6)],
        "final_hd": [round(FINAL_ANCHOR[0] + (point[0] - SOURCE_ANCHOR[0]) * CONTENT_SCALE, 2),
                     round(FINAL_ANCHOR[1] + (point[1] - SOURCE_ANCHOR[1]) * CONTENT_SCALE, 2)],
    }


def normalize_rect(rect):
    left, top, right, bottom = rect
    return {
        "source": [left, top, right, bottom],
        "anchor_relative": [
            round((left - SOURCE_ANCHOR[0]) / SOURCE_CANVAS[0], 6),
            round((top - SOURCE_ANCHOR[1]) / SOURCE_CANVAS[1], 6),
            round((right - SOURCE_ANCHOR[0]) / SOURCE_CANVAS[0], 6),
            round((bottom - SOURCE_ANCHOR[1]) / SOURCE_CANVAS[1], 6),
        ],
        "final_hd": [
            round(FINAL_ANCHOR[0] + (left - SOURCE_ANCHOR[0]) * CONTENT_SCALE, 2),
            round(FINAL_ANCHOR[1] + (top - SOURCE_ANCHOR[1]) * CONTENT_SCALE, 2),
            round(FINAL_ANCHOR[0] + (right - SOURCE_ANCHOR[0]) * CONTENT_SCALE, 2),
            round(FINAL_ANCHOR[1] + (bottom - SOURCE_ANCHOR[1]) * CONTENT_SCALE, 2),
        ],
        "size_source": [right - left, bottom - top],
        "size_normalized": [
            round((right - left) / SOURCE_CANVAS[0], 6),
            round((bottom - top) / SOURCE_CANVAS[1], 6),
        ],
        "size_final_hd": [(right - left) * CONTENT_SCALE, (bottom - top) * CONTENT_SCALE],
    }


def bounds(pixels):
    points = [(x, y) for y, row in enumerate(pixels) for x, p in enumerate(row) if p[3]]
    return (min(x for x, _ in points), min(y for _, y in points),
            max(x for x, _ in points) + 1, max(y for _, y in points) + 1)


def add_rect(canvas, rect, color):
    left, top, right, bottom = rect
    p0 = source_to_guide((left, top)); p1 = source_to_guide((right, bottom))
    line(canvas, p0, (p1[0], p0[1]), color, 1)
    line(canvas, (p1[0], p0[1]), p1, color, 1)
    line(canvas, p1, (p0[0], p1[1]), color, 1)
    line(canvas, (p0[0], p1[1]), p0, color, 1)


def guide_for(pose, pixels, spec):
    width = 48 * GUIDE_SCALE + GUIDE_MARGIN * 2
    height = 32 * GUIDE_SCALE + GUIDE_MARGIN * 2
    canvas = blank(width, height, (17, 21, 29, 255))
    restored = blank(48, 32, (0, 0, 0, 0))
    composite(restored, pixels, 0, 0)
    enlarged = nearest(restored, 48 * GUIDE_SCALE, 32 * GUIDE_SCALE)
    composite(canvas, enlarged, GUIDE_MARGIN, GUIDE_MARGIN)

    for source_x in range(49):
        x = GUIDE_MARGIN + source_x * GUIDE_SCALE
        value = (61, 68, 82, 150) if source_x % 4 else (91, 101, 119, 190)
        line(canvas, (x, GUIDE_MARGIN), (x, GUIDE_MARGIN + 32 * GUIDE_SCALE), value, 0)
    for source_y in range(33):
        y = GUIDE_MARGIN + source_y * GUIDE_SCALE
        value = (61, 68, 82, 150) if source_y % 4 else (91, 101, 119, 190)
        line(canvas, (GUIDE_MARGIN, y), (GUIDE_MARGIN + 48 * GUIDE_SCALE, y), value, 0)

    for y, row in enumerate(restored):
        for x, pixel in enumerate(row):
            if not pixel[3]: continue
            if any(nx < 0 or nx >= 48 or ny < 0 or ny >= 32 or not restored[ny][nx][3]
                   for nx, ny in ((x-1,y),(x+1,y),(x,y-1),(x,y+1))):
                left = GUIDE_MARGIN + x * GUIDE_SCALE
                top = GUIDE_MARGIN + y * GUIDE_SCALE
                for offset in range(GUIDE_SCALE):
                    put(canvas, left + offset, top, COLORS["silhouette"])
                    put(canvas, left + offset, top + GUIDE_SCALE - 1, COLORS["silhouette"])
                    put(canvas, left, top + offset, COLORS["silhouette"])
                    put(canvas, left + GUIDE_SCALE - 1, top + offset, COLORS["silhouette"])

    add_rect(canvas, spec["charlie"]["overall_bounds"], COLORS["charlie"])
    add_rect(canvas, spec["charlie"]["head_bounds"], COLORS["charlie"])
    add_rect(canvas, spec["charlie"]["body_bounds"], COLORS["charlie"])
    add_rect(canvas, spec["lion"]["torso_bounds"], COLORS["lion"])
    add_rect(canvas, spec["lion"]["head_bounds"], COLORS["lion"])
    for key, point in spec["charlie"].items():
        if not key.endswith("bounds"): circle(canvas, source_to_guide(point), 7, COLORS["charlie"])
    for key, point in spec["lion"].items():
        if not key.endswith("bounds"): circle(canvas, source_to_guide(point), 7, COLORS["lion"])
    for named_line in ("back_line", "belly_line"):
        points = [source_to_guide(point) for point in spec[named_line]]
        for first, second in zip(points, points[1:]): line(canvas, first, second, COLORS["skeleton"], 2)
    for first, second in spec["limbs"]:
        line(canvas, source_to_guide(first), source_to_guide(second), COLORS["skeleton"], 2)
    for rect in spec["negative_spaces"].values(): add_rect(canvas, rect, COLORS["negative"])

    anchor = source_to_guide(SOURCE_ANCHOR)
    line(canvas, (anchor[0] - 18, anchor[1]), (anchor[0] + 18, anchor[1]), COLORS["anchor"], 2)
    line(canvas, (anchor[0], anchor[1] - 18), (anchor[0], anchor[1] + 18), COLORS["anchor"], 2)
    return canvas


def main() -> int:
    OUTPUT.mkdir(parents=True, exist_ok=True)
    report = {
        "authority": "verified original 48x32 hardware composites",
        "source_canvas": [48, 32], "source_gameplay_anchor": [24, 32],
        "recommended_final_canvas": list(FINAL_CANVAS),
        "recommended_final_anchor": list(FINAL_ANCHOR),
        "source_to_final_scale": CONTENT_SCALE, "poses": {},
    }
    guides = []
    for pose, spec in POSES.items():
        _, _, tight = read_png(SOURCE / f"{pose}.png")
        restored = blank(48, 32, (0, 0, 0, 0)); composite(restored, tight, 0, 0)
        exact_bounds = bounds(restored)
        enlarged = nearest(restored, 48 * GUIDE_SCALE, 32 * GUIDE_SCALE)
        (OUTPUT / f"{pose}-nearest-16x.png").write_bytes(png_bytes(enlarged))
        guide = guide_for(pose, tight, spec)
        (OUTPUT / f"{pose}-structural-guide.png").write_bytes(png_bytes(guide))
        guides.append(guide)

        normalized = {
            "exact_silhouette_bounds_source": list(exact_bounds),
            "exact_silhouette_bounds_anchor_relative": [
                round((exact_bounds[0] - 24) / 48, 6), round((exact_bounds[1] - 32) / 32, 6),
                round((exact_bounds[2] - 24) / 48, 6), round((exact_bounds[3] - 32) / 32, 6),
            ],
            "exact_silhouette_bounds_final_hd": [
                FINAL_ANCHOR[0] + (exact_bounds[0] - 24) * CONTENT_SCALE,
                FINAL_ANCHOR[1] + (exact_bounds[1] - 32) * CONTENT_SCALE,
                FINAL_ANCHOR[0] + (exact_bounds[2] - 24) * CONTENT_SCALE,
                FINAL_ANCHOR[1] + (exact_bounds[3] - 32) * CONTENT_SCALE,
            ],
            "charlie": {}, "lion": {},
            "back_line": [normalize_point(point) for point in spec["back_line"]],
            "belly_line": [normalize_point(point) for point in spec["belly_line"]],
            "limbs": [[normalize_point(first), normalize_point(second)] for first, second in spec["limbs"]],
            "negative_spaces": {
                name: normalize_rect(rect) for name, rect in spec["negative_spaces"].items()
            },
        }
        for subject in ("charlie", "lion"):
            for name, value in spec[subject].items():
                if name.endswith("bounds"):
                    normalized[subject][name] = normalize_rect(value)
                else:
                    normalized[subject][name] = normalize_point(value)
        report["poses"][pose] = normalized

    gap = 16
    sheet = blank(max(len(g[0]) for g in guides), sum(len(g) for g in guides) + gap * 2, (10, 13, 18, 255))
    y = 0
    for guide in guides:
        composite(sheet, guide, 0, y); y += len(guide) + gap
    (OUTPUT / "all-structural-guides.png").write_bytes(png_bytes(sheet))
    (OUTPUT / "measurements.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(OUTPUT)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

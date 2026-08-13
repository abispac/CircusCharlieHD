#!/usr/bin/env python3
"""Assemble the native Level 1 rider integration captures without editing art."""

from __future__ import annotations

import json
from pathlib import Path

from analyze_level1_hd_rider import blank, composite, nearest, png_bytes, read_png


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "docs" / "diagnostics" / "level1-hd-rider-integration" / "native-sequence"
OUTPUT = ROOT / "docs" / "diagnostics" / "level1-hd-rider-integration"

CAPTURES = (
    (24, 1346, "Run A, jump input staged"),
    (25, 1347, "Run C, takeoff"),
    (53, 1375, "Run C, ascent"),
    (73, 1395, "Run C, hoop crossing/descent"),
    (87, 1409, "Run C, final airborne sample"),
    (88, 1410, "Run A, landing"),
    (96, 1418, "Run B, grounded cadence"),
    (103, 1425, "Run C, grounded cadence"),
)


def main() -> int:
    OUTPUT.mkdir(parents=True, exist_ok=True)
    panel_width, panel_height = 240, 320
    gap = 8
    sheet = blank(
        panel_width * 4 + gap * 5,
        panel_height * 2 + gap * 3,
        (14, 17, 23, 255),
    )
    manifest = []
    for index, (native_frame, mame_frame, description) in enumerate(CAPTURES):
        filename = f"native-frame-{native_frame:03d}-mame-{mame_frame}.png"
        width, height, pixels = read_png(SOURCE / filename)
        panel = nearest(pixels, panel_width, panel_height)
        column, row = index % 4, index // 4
        left = gap + column * (panel_width + gap)
        top = gap + row * (panel_height + gap)
        composite(sheet, panel, left, top)
        manifest.append(
            {
                "native_frame": native_frame,
                "mame_frame": mame_frame,
                "description": description,
                "source": str((SOURCE / filename).relative_to(ROOT)),
                "sheet_cell": {"column": column, "row": row},
                "source_size": [width, height],
            }
        )
    (OUTPUT / "corrected-native-hoop-jump.png").write_bytes(png_bytes(sheet))
    (OUTPUT / "corrected-native-hoop-jump.json").write_text(
        json.dumps({"order": "left-to-right, top-to-bottom", "captures": manifest}, indent=2) + "\n",
        encoding="utf-8",
    )
    print(OUTPUT / "corrected-native-hoop-jump.png")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

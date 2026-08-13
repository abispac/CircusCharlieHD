#!/usr/bin/env python3
"""Build checked-in visual evidence for the Level 1 rider integration."""

from __future__ import annotations

import argparse
import csv
import json
import shutil
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "docs/diagnostics/level1-rider-production"
ANCHOR = (512, 640)
CAPTURES = (
    (0, "Grounded Run A"),
    (6, "Grounded Run B"),
    (14, "Grounded Run C"),
    (25, "Takeoff"),
    (33, "Early ascent"),
    (57, "Apex"),
    (73, "Hoop crossing / descent"),
    (87, "Final airborne frame"),
    (88, "Landing / Run A"),
    (96, "Resumed Run B"),
    (103, "Resumed Run C"),
)


def font(size: int):
    for candidate in ("/System/Library/Fonts/Supplemental/Arial.ttf", "/System/Library/Fonts/Helvetica.ttc"):
        if Path(candidate).exists():
            return ImageFont.truetype(candidate, size)
    return ImageFont.load_default()


def labelled(image: Image.Image, title: str, size=(360, 480)) -> Image.Image:
    canvas = Image.new("RGB", (size[0], size[1] + 42), (16, 18, 23))
    fitted = image.convert("RGB").resize(size, Image.Resampling.LANCZOS)
    canvas.paste(fitted, (0, 42))
    ImageDraw.Draw(canvas).text((12, 10), title, fill=(245, 245, 245), font=font(20))
    return canvas


def gameplay_sheet(capture_dir: Path) -> None:
    panels = []
    sequence_dir = OUTPUT / "native-sequence"
    sequence_dir.mkdir(parents=True, exist_ok=True)
    for native_frame, title in CAPTURES:
        matches = sorted(capture_dir.glob(f"native-frame-{native_frame:03d}-mame-*.png"))
        if len(matches) != 1:
            raise SystemExit(f"expected one capture for native frame {native_frame}, found {len(matches)}")
        destination = sequence_dir / matches[0].name
        shutil.copyfile(matches[0], destination)
        panels.append(labelled(Image.open(destination), title))
    columns, rows = 4, 3
    sheet = Image.new("RGB", (columns * 360, rows * 522), (8, 9, 12))
    for index, panel in enumerate(panels):
        sheet.paste(panel, ((index % columns) * 360, (index // columns) * 522))
    sheet.save(OUTPUT / "successful-large-hoop-native-contact-sheet.jpg", quality=94)

    runtime_frames = (
        (0, "Run A"),
        (6, "Run B"),
        (14, "Run C grounded"),
        (57, "Run C airborne"),
        (88, "Landing Run A"),
    )
    runtime_sheet = Image.new("RGB", (360 * 5, 522), (8, 9, 12))
    for column, (native_frame, title) in enumerate(runtime_frames):
        matches = sorted(capture_dir.glob(f"native-frame-{native_frame:03d}-mame-*.png"))
        if len(matches) != 1:
            raise SystemExit(f"expected one runtime capture for native frame {native_frame}")
        runtime_sheet.paste(labelled(Image.open(matches[0]), title), (column * 360, 0))
    runtime_sheet.save(OUTPUT / "common-scale-runtime-contact-sheet.jpg", quality=94)


def anchored_original(path: Path) -> Image.Image:
    source = Image.open(path).convert("RGBA")
    scale = 12
    enlarged = source.resize((source.width * scale, source.height * scale), Image.Resampling.NEAREST)
    canvas = Image.new("RGBA", (1024, 768), (0, 0, 0, 0))
    x = ANCHOR[0] - 24 * scale
    y = ANCHOR[1] - 32 * scale
    canvas.alpha_composite(enlarged, (x, y))
    return canvas


def checkerboard() -> Image.Image:
    canvas = Image.new("RGBA", (1024, 768), (232, 232, 232, 255))
    draw = ImageDraw.Draw(canvas)
    tile = 32
    for y in range(0, 768, tile):
        for x in range(0, 1024, tile):
            if (x // tile + y // tile) % 2:
                draw.rectangle((x, y, x + tile - 1, y + tile - 1), fill=(198, 198, 198, 255))
    return canvas


def pose_comparison_sheet() -> None:
    pairs = (
        ("Run A", ROOT / "reference/original/level1/rider/run-a.png", ROOT / "assets/stage1-rider-run-a-hd.png"),
        ("Run B", ROOT / "reference/original/level1/rider/run-b.png", ROOT / "assets/stage1-rider-run-b-hd.png"),
        ("Run C / Airborne", ROOT / "reference/original/level1/rider/run-c.png", ROOT / "assets/stage1-rider-run-c-hd.png"),
    )
    panel_size = (512, 384)
    sheet = Image.new("RGB", (panel_size[0] * 2, (panel_size[1] + 44) * 3), (12, 13, 17))
    label_font = font(20)
    for row, (pose, original_path, hd_path) in enumerate(pairs):
        for column, (kind, subject) in enumerate((
            ("Original arcade", anchored_original(original_path)),
            ("Final HD", Image.open(hd_path).convert("RGBA")),
        )):
            base = checkerboard()
            base.alpha_composite(subject)
            resized = base.convert("RGB").resize(panel_size, Image.Resampling.LANCZOS)
            top = row * (panel_size[1] + 44)
            sheet.paste(resized, (column * panel_size[0], top + 44))
            ImageDraw.Draw(sheet).text(
                (column * panel_size[0] + 12, top + 10),
                f"{pose} — {kind} — anchor (512,640)",
                fill=(245, 245, 245),
                font=label_font,
            )
    sheet.save(OUTPUT / "original-vs-production-hd-anchor-comparison.jpg", quality=95)


def trace_summary(trace_path: Path, comparison_path: Path) -> None:
    trace = list(csv.DictReader(trace_path.open(newline="", encoding="utf-8")))
    comparison = list(csv.DictReader(comparison_path.open(newline="", encoding="utf-8")))
    airborne = [row for row in trace if row["grounded"] == "0"]
    landing = [row for row in trace if row["landing_transition"] == "1"]
    score = [row for row in trace if int(row["hoop_score_event"]) != 0]
    result = {
        "comparison_rows": len(comparison),
        "comparison_all_match": all(row["match"] == "1" for row in comparison),
        "airborne_frame_range": [int(airborne[0]["frame"]), int(airborne[-1]["frame"])],
        "airborne_assets": sorted({int(row["rider_hd_frame"]) for row in airborne}),
        "airborne_states": sorted({row["rider_animation_state"] for row in airborne}),
        "airborne_anchors": sorted({(float(row["rider_anchor_x"]), float(row["rider_anchor_y"])) for row in airborne}),
        "landing_frames": [int(row["frame"]) for row in landing],
        "landing_assets": [int(row["rider_hd_frame"]) for row in landing],
        "score_events": [{"frame": int(row["frame"]), "points": int(row["hoop_score_event"])} for row in score],
    }
    (OUTPUT / "verification-summary.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    shutil.copyfile(trace_path, OUTPUT / "native-successful-large-hoop.csv")
    shutil.copyfile(comparison_path, OUTPUT / "mame-native-state-comparison.csv")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--capture-dir", type=Path, required=True)
    parser.add_argument("--native-trace", type=Path, required=True)
    parser.add_argument("--comparison", type=Path, required=True)
    args = parser.parse_args()
    OUTPUT.mkdir(parents=True, exist_ok=True)
    gameplay_sheet(args.capture_dir)
    pose_comparison_sheet()
    trace_summary(args.native_trace, args.comparison)
    print(f"wrote production diagnostics to {OUTPUT}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

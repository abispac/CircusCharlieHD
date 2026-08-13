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
    # Preserve each renderer capture at its exact native backing-store pixels. No
    # diagnostic resizing is permitted in this physical-size proof.
    first_matches = sorted(capture_dir.glob(f"native-frame-{runtime_frames[0][0]:03d}-mame-*.png"))
    if len(first_matches) != 1:
        raise SystemExit("could not determine native runtime capture dimensions")
    native_size = Image.open(first_matches[0]).size
    header = 42
    runtime_sheet = Image.new(
        "RGB", (native_size[0] * 5, native_size[1] + header), (8, 9, 12)
    )
    for column, (native_frame, title) in enumerate(runtime_frames):
        matches = sorted(capture_dir.glob(f"native-frame-{native_frame:03d}-mame-*.png"))
        if len(matches) != 1:
            raise SystemExit(f"expected one runtime capture for native frame {native_frame}")
        capture = Image.open(matches[0]).convert("RGB")
        if capture.size != native_size:
            raise SystemExit(
                f"runtime capture {matches[0]} is {capture.size}, expected {native_size}"
            )
        panel = Image.new(
            "RGB", (native_size[0], native_size[1] + header), (16, 18, 23)
        )
        panel.paste(capture, (0, header))
        ImageDraw.Draw(panel).text(
            (12, 10), title, fill=(245, 245, 245), font=font(20)
        )
        runtime_sheet.paste(panel, (column * native_size[0], 0))
    runtime_sheet.save(OUTPUT / "identity-scale-runtime-contact-sheet.jpg", quality=94)

    # Actual renderer pixels, cropped around the rider only. Cropping makes the
    # small gameplay sprite inspectable without rescaling or synthesizing a
    # preview; every subject pixel is copied directly from the real frame.
    crop_specs = (
        (0, "Run A", (80, 1000, 300, 1220)),
        (6, "Run B", (55, 995, 285, 1225)),
        (14, "Run C grounded", (10, 995, 285, 1225)),
        (57, "Run C airborne", (75, 720, 350, 950)),
        (88, "Landing Run A", (80, 1000, 300, 1220)),
    )
    crop_w, crop_h, crop_header = 300, 260, 42
    crop_sheet = Image.new(
        "RGB", (crop_w * len(crop_specs), crop_h + crop_header), (8, 9, 12)
    )
    for column, (native_frame, title, box) in enumerate(crop_specs):
        matches = sorted(capture_dir.glob(f"native-frame-{native_frame:03d}-mame-*.png"))
        if len(matches) != 1:
            raise SystemExit(f"expected one cropped runtime capture for native frame {native_frame}")
        capture = Image.open(matches[0]).convert("RGB")
        cropped = capture.crop(box)
        panel = Image.new("RGB", (crop_w, crop_h + crop_header), (16, 18, 23))
        # Center the unscaled crop. It may be smaller than the common panel.
        panel.paste(cropped, ((crop_w - cropped.width) // 2, crop_header + (crop_h - cropped.height) // 2))
        ImageDraw.Draw(panel).text((12, 10), title, fill=(245, 245, 245), font=font(20))
        crop_sheet.paste(panel, (column * crop_w, 0))
    crop_sheet.save(OUTPUT / "actual-runtime-rider-pixel-crops.jpg", quality=97)

    # Two complete grounded A -> B -> C -> A cycles, copied from actual native
    # renderer frames. This exposes size pops without using synthetic previews.
    grounded_cycle = (
        (0, "A1"), (6, "B1"), (14, "C1"), (21, "A2"),
        (6, "B2"), (14, "C2"), (21, "A3"),
    )
    cycle_crop = (0, 985, 300, 1235)
    cycle_w, cycle_h = 300, 250
    cycle_sheet = Image.new(
        "RGB", (cycle_w * len(grounded_cycle), cycle_h + crop_header), (8, 9, 12)
    )
    charlie_sheet = Image.new(
        "RGB", (180 * len(grounded_cycle), 180 + crop_header), (8, 9, 12)
    )
    for column, (native_frame, title) in enumerate(grounded_cycle):
        matches = sorted(capture_dir.glob(f"native-frame-{native_frame:03d}-mame-*.png"))
        if len(matches) != 1:
            raise SystemExit(f"expected grounded-cycle frame {native_frame}")
        capture = Image.open(matches[0]).convert("RGB")
        rider = capture.crop(cycle_crop)
        panel = Image.new("RGB", (cycle_w, cycle_h + crop_header), (16, 18, 23))
        panel.paste(rider, (0, crop_header))
        ImageDraw.Draw(panel).text((12, 10), title, fill=(245, 245, 245), font=font(20))
        cycle_sheet.paste(panel, (column * cycle_w, 0))

        # Charlie-only close crop at the same native pixel scale. A/B/C boxes
        # follow the measured identity regions; they are crops, never resizes.
        state = title[0]
        charlie_boxes = {
            "A": (90, 1030, 210, 1150),
            "B": (80, 1020, 200, 1140),
            "C": (85, 1015, 205, 1135),
        }
        charlie = capture.crop(charlie_boxes[state])
        close = Image.new("RGB", (180, 180 + crop_header), (16, 18, 23))
        close.paste(charlie, ((180 - charlie.width) // 2,
                             crop_header + (180 - charlie.height) // 2))
        ImageDraw.Draw(close).text((12, 10), title, fill=(245, 245, 245), font=font(20))
        charlie_sheet.paste(close, (column * 180, 0))
    cycle_sheet.save(OUTPUT / "actual-runtime-grounded-abca-cycles.jpg", quality=97)
    charlie_sheet.save(OUTPUT / "actual-runtime-grounded-charlie-closeups.jpg", quality=97)


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

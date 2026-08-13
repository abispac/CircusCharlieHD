#!/usr/bin/env python3
"""Diagnostic-only apparent-size study for A2/B2 and the original Run C.

This tool never writes production assets.  It preserves the supplied pixels and
alpha exactly while applying proposed offline preview transforms only.
"""

from __future__ import annotations

import hashlib
import json
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "docs/diagnostics/level1-rider-original-c-size-study"
PREVIEW_SIZE = (1024, 768)
PREVIEW_ANCHOR = (512, 438)

POSES = {
    "run-a": {
        "path": ROOT / "run/run a 2.png",
        "sha256": "50f2b9b381c630d8740c1b947e1ce0e97824dc987370b2715a93948b6d09d207",
        "anchor": (767.5, 582.5),
        "features": {
            "charlie_head": (545, 70, 1070, 370),
            "charlie_torso": (650, 305, 1010, 570),
            "lion_head": (925, 305, 1440, 745),
            "mane": (890, 305, 1295, 795),
            "saddle": (635, 475, 900, 690),
            "body_depth": (400, 480, 900, 820),
            "paw": (1090, 805, 1375, 1005),
        },
    },
    "run-b": {
        "path": ROOT / "run/Run b 2.png",
        "sha256": "add70517b36a9b921d7a43d8286d7e35f26ba8d132aad4746dca7a1261429a90",
        "anchor": (777.5, 572.5),
        "features": {
            "charlie_head": (550, 65, 1070, 350),
            "charlie_torso": (650, 285, 1010, 555),
            "lion_head": (930, 290, 1445, 720),
            "mane": (900, 290, 1305, 765),
            "saddle": (650, 470, 905, 675),
            "body_depth": (430, 480, 920, 795),
            "paw": (1065, 805, 1270, 985),
        },
    },
    "run-c-original": {
        "path": ROOT / "run/run c.png",
        "sha256": "51881d33b803c8dec3d6c3177db3767fd4e84642a6e4077aab583d6a677cc9f9",
        "anchor": (850.0, 520.0),
        "features": {
            "charlie_head": (775, 55, 1155, 340),
            "charlie_torso": (770, 250, 1100, 500),
            "lion_head": (1050, 285, 1490, 650),
            "mane": (980, 280, 1440, 710),
            "saddle": (710, 415, 1010, 625),
            "body_depth": (390, 420, 1060, 735),
            "paw": (1260, 695, 1490, 875),
        },
    },
}

FIT_FEATURES = (
    "charlie_head", "charlie_torso", "lion_head", "mane", "saddle",
    "body_depth", "paw",
)
# Horizontal width is stable for compact identity features.  For torso/mane
# mass, only vertical depth is stable in an extended C pose; their horizontal
# span deliberately changes and must never shrink the whole character.
FIT_AXES = {
    "charlie_head": (0, 1),
    "charlie_torso": (1,),
    "lion_head": (0, 1),
    "mane": (1,),
    "saddle": (0, 1),
    "body_depth": (1,),
    "paw": (0, 1),
}


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def font(size: int):
    candidates = (
        "/System/Library/Fonts/Supplemental/Arial.ttf",
        "/System/Library/Fonts/Helvetica.ttc",
    )
    for candidate in candidates:
        if Path(candidate).exists():
            return ImageFont.truetype(candidate, size)
    return ImageFont.load_default()


def dimensions(bounds):
    left, top, right, bottom = bounds
    return np.asarray((right - left, bottom - top), dtype=np.float64)


def proposed_scales():
    raw = {
        pose: {name: dimensions(bounds) for name, bounds in spec["features"].items()}
        for pose, spec in POSES.items()
    }
    grounded_target = {
        name: (raw["run-a"][name] + raw["run-b"][name]) / 2.0
        for name in FIT_FEATURES
    }
    factors = {}
    for pose in POSES:
        source = np.asarray([
            raw[pose][name][axis]
            for name in FIT_FEATURES for axis in FIT_AXES[name]
        ], dtype=np.float64)
        target = np.asarray([
            grounded_target[name][axis]
            for name in FIT_FEATURES for axis in FIT_AXES[name]
        ], dtype=np.float64)
        factors[pose] = float(np.dot(source, target) / np.dot(source, source))

    # A/B are allowed tiny density compensation around their joint mean.  Set
    # their geometric mean to the convenient diagnostic density of 0.43 and
    # carry the independently measured C factor through unchanged.
    base = 0.43 / ((factors["run-a"] * factors["run-b"]) ** 0.5)
    scales = {pose: factors[pose] * base for pose in POSES}
    return raw, grounded_target, factors, scales


def alpha_stats(image: Image.Image):
    alpha = np.asarray(image.getchannel("A"))
    return {
        "dimensions": list(image.size),
        "alpha_range": [int(alpha.min()), int(alpha.max())],
        "fully_transparent_percent": round(float((alpha == 0).mean() * 100), 6),
        "fully_opaque_percent": round(float((alpha == 255).mean() * 100), 6),
    }


def checkerboard(size=PREVIEW_SIZE, tile=24):
    result = Image.new("RGBA", size, (225, 227, 232, 255))
    draw = ImageDraw.Draw(result)
    for y in range(0, size[1], tile):
        for x in range(0, size[0], tile):
            if (x // tile + y // tile) % 2:
                draw.rectangle((x, y, x + tile - 1, y + tile - 1), fill=(181, 185, 193, 255))
    return result


def normalized_preview(image: Image.Image, spec: dict, scale: float) -> Image.Image:
    anchor_x, anchor_y = spec["anchor"]
    translation_x = PREVIEW_ANCHOR[0] - anchor_x * scale
    translation_y = PREVIEW_ANCHOR[1] - anchor_y * scale
    inverse = 1.0 / scale
    return image.transform(
        PREVIEW_SIZE,
        Image.Transform.AFFINE,
        (inverse, 0.0, -translation_x * inverse,
         0.0, inverse, -translation_y * inverse),
        Image.Resampling.BICUBIC,
    )


def labelled(subject: Image.Image, title: str, size=(512, 384), feature_boxes=None):
    background = checkerboard(PREVIEW_SIZE)
    background.alpha_composite(subject)
    draw = ImageDraw.Draw(background)
    draw.line((PREVIEW_ANCHOR[0] - 18, PREVIEW_ANCHOR[1], PREVIEW_ANCHOR[0] + 18, PREVIEW_ANCHOR[1]), fill=(0, 255, 255, 255), width=3)
    draw.line((PREVIEW_ANCHOR[0], PREVIEW_ANCHOR[1] - 18, PREVIEW_ANCHOR[0], PREVIEW_ANCHOR[1] + 18), fill=(0, 255, 255, 255), width=3)
    panel = Image.new("RGB", (size[0], size[1] + 44), (15, 17, 22))
    panel.paste(background.convert("RGB").resize(size, Image.Resampling.LANCZOS), (0, 44))
    ImageDraw.Draw(panel).text((12, 10), title, fill="white", font=font(20))
    return panel


def original_sheet(images):
    panels = []
    for pose in POSES:
        image = images[pose]
        canvas = Image.new("RGBA", PREVIEW_SIZE, (0, 0, 0, 0))
        scale = min(900 / image.width, 650 / image.height)
        resized = image.resize((round(image.width * scale), round(image.height * scale)), Image.Resampling.LANCZOS)
        canvas.alpha_composite(resized, ((1024 - resized.width) // 2, (768 - resized.height) // 2))
        panels.append(labelled(canvas, f"Original pixels — {pose}"))
    sheet = Image.new("RGB", (1536, 428), (8, 9, 12))
    for index, panel in enumerate(panels):
        sheet.paste(panel, (index * 512, 0))
    sheet.save(OUT / "original-a-b-c.jpg", quality=95)


def normalized_sheet(previews, scales):
    sheet = Image.new("RGB", (1536, 428), (8, 9, 12))
    for index, pose in enumerate(POSES):
        title = f"Proposed {pose} — offline scale {scales[pose]:.6f}"
        sheet.paste(labelled(previews[pose], title), (index * 512, 0))
    sheet.save(OUT / "proposed-normalized-a-b-c.jpg", quality=95)


def overlay_sheet(previews):
    pairs = (("run-a", "run-b"), ("run-b", "run-c-original"), ("run-c-original", "run-a"))
    colors = ((255, 50, 50), (50, 220, 255))
    sheet = Image.new("RGB", (1536, 428), (8, 9, 12))
    for index, (first, second) in enumerate(pairs):
        base = checkerboard(PREVIEW_SIZE)
        for pose, color in ((first, colors[0]), (second, colors[1])):
            image = previews[pose]
            alpha = image.getchannel("A").point(lambda value: round(value * 0.52))
            tint = Image.new("RGBA", PREVIEW_SIZE, (*color, 0))
            tint.putalpha(alpha)
            # Preserve the artwork's luminance beneath a colored identity veil.
            art = image.copy()
            art.putalpha(alpha)
            base.alpha_composite(art)
            base.alpha_composite(tint)
        sheet.paste(labelled(base, f"Anchor overlay: {first} / {second}"), (index * 512, 0))
    sheet.save(OUT / "proposed-anchor-overlays.jpg", quality=95)


def sequence_sheet(previews):
    sequence = (
        ("run-a", "A"), ("run-b", "B"), ("run-c-original", "C"),
        ("run-c-original", "Airborne C (same image)"), ("run-a", "Landing A"),
    )
    panel_size = (360, 270)
    sheet = Image.new("RGB", (panel_size[0] * len(sequence), panel_size[1] + 44), (8, 9, 12))
    for column, (pose, title) in enumerate(sequence):
        sheet.paste(labelled(previews[pose], title, panel_size), (column * panel_size[0], 0))
    sheet.save(OUT / "simulated-a-b-c-airborne-c-a.jpg", quality=95)


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    images = {}
    for pose, spec in POSES.items():
        actual = digest(spec["path"])
        if actual != spec["sha256"]:
            raise SystemExit(f"{pose} master changed: expected {spec['sha256']}, got {actual}")
        images[pose] = Image.open(spec["path"]).convert("RGBA")

    raw, target, factors, scales = proposed_scales()
    previews = {
        pose: normalized_preview(images[pose], POSES[pose], scales[pose])
        for pose in POSES
    }
    original_sheet(images)
    normalized_sheet(previews, scales)
    overlay_sheet(previews)
    sequence_sheet(previews)

    report = {
        "status": "diagnostic-only; no production asset or gameplay modification",
        "authoritative_sources": {
            pose: {"path": str(spec["path"].relative_to(ROOT)), "sha256": spec["sha256"], "alpha": alpha_stats(images[pose])}
            for pose, spec in POSES.items()
        },
        "method": {
            "size_target": "A2/B2 arithmetic mean for stable identity-feature width and height",
            "fit_features": list(FIT_FEATURES),
            "fit_axes": {name: list(axes) for name, axes in FIT_AXES.items()},
            "excluded": ["total image size", "outer silhouette", "tail length", "leg extension", "total pose width"],
            "anchor": "center of saddle/body relationship, translated to diagnostic point (512,438)",
            "rotation": "none",
            "runtime_changes": "none",
        },
        "source_feature_dimensions_px": {
            pose: {name: [float(v) for v in values] for name, values in features.items()}
            for pose, features in raw.items()
        },
        "a_b_target_feature_dimensions_px": {
            name: [float(v) for v in values] for name, values in target.items()
        },
        "identity_density_factor": factors,
        "proposed_offline_scale": scales,
        "proposed_rendered_feature_dimensions_px": {
            pose: {
                name: [round(float(v * scales[pose]), 3) for v in values]
                for name, values in raw[pose].items()
            }
            for pose in POSES
        },
        "diagnostics": [
            "original-a-b-c.jpg", "proposed-normalized-a-b-c.jpg",
            "proposed-anchor-overlays.jpg", "simulated-a-b-c-airborne-c-a.jpg",
        ],
    }
    (OUT / "measurements.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"proposed_offline_scale": scales}, indent=2))


if __name__ == "__main__":
    main()

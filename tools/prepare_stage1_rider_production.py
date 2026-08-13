#!/usr/bin/env python3
"""Prepare accepted Level 1 rider art without modifying supplied masters.

The cleanup and one-transform normalization are deterministic. This tool is
intentionally specific to the accepted A2/B2/original-C production test and
writes only production copies plus diagnostics/metadata.
"""

from __future__ import annotations

import hashlib
import json
import math
import base64
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFilter


ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "assets"
DIAGNOSTICS = ROOT / "docs/diagnostics/level1-rider-production"
CANVAS = (1536, 1024)
GAMEPLAY_ANCHOR = (768, 714)
# Stable saddle/central-body point inside the production canvas. The runtime
# samples every sprite at GAMEPLAY_ANCHOR; keeping this body point at a common
# canvas location makes the body-to-gameplay-anchor vector identical in A/B/C.
PRODUCTION_BODY_ANCHOR = (768, 512)
# Fixed LittleCMS sRGB profile bytes. Creating a fresh profile at runtime puts
# the current time in its ICC header and makes otherwise identical PNG builds
# hash differently.
SRGB_ICC = base64.b64decode(
    "AAACTGxjbXMEQAAAbW50clJHQiBYWVogB+oACAANAAcABQAhYWNzcEFQUEwAAAAAAAAA"
    "AAAAAAAAAAAAAAAAAAAAAAAAAPbWAAEAAAAA0y1sY21zAAAAAAAAAAAAAAAAAAAAAAAA"
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAALZGVzYwAAAQgAAAA2Y3BydAAAAUAA"
    "AABMd3RwdAAAAYwAAAAUY2hhZAAAAaAAAAAsclhZWgAAAcwAAAAUYlhZWgAAAeAAAAAU"
    "Z1hZWgAAAfQAAAAUclRSQwAAAggAAAAgZ1RSQwAAAggAAAAgYlRSQwAAAggAAAAgY2hy"
    "bQAAAigAAAAkbWx1YwAAAAAAAAABAAAADGVuVVMAAAAaAAAAHABzAFIARwBCACAAYgB1"
    "AGkAbAB0AC0AaQBuAABtbHVjAAAAAAAAAAEAAAAMZW5VUwAAADAAAAAcAE4AbwAgAGMA"
    "bwBwAHkAcgBpAGcAaAB0ACwAIAB1AHMAZQAgAGYAcgBlAGUAbAB5WFlaIAAAAAAAAPbW"
    "AAEAAAAA0y1zZjMyAAAAAAABDEIAAAXe///zJQAAB5MAAP2Q///7of///aIAAAPcAADA"
    "blhZWiAAAAAAAABvoAAAOPUAAAOQWFlaIAAAAAAAACSfAAAPhAAAtsNYWVogAAAAAAAA"
    "YpcAALeHAAAY2XBhcmEAAAAAAAMAAAACZmYAAPKnAAANWQAAE9AAAApbY2hybQAAAAAA"
    "AwAAAACj1wAAVHsAAEzNAACZmgAAJmYAAA9c"
)
POSES = {
    "run-a": {
        "source": ROOT / "run/run a 2.png",
        "source_sha256": "50f2b9b381c630d8740c1b947e1ce0e97824dc987370b2715a93948b6d09d207",
        "output": OUTPUT / "stage1-rider-run-a-hd.png",
        "production_scale": 0.713273428608,
        "rotation_degrees": 0.0,
        "source_anchor": (767.5, 582.5),
    },
    "run-b": {
        "source": ROOT / "run/Run b 2.png",
        "source_sha256": "add70517b36a9b921d7a43d8286d7e35f26ba8d132aad4746dca7a1261429a90",
        "output": OUTPUT / "stage1-rider-run-b-hd.png",
        "production_scale": 0.740379311399,
        "rotation_degrees": 0.0,
        "source_anchor": (777.5, 572.5),
    },
    "run-c": {
        "source": ROOT / "run/run c.png",
        "source_sha256": "51881d33b803c8dec3d6c3177db3767fd4e84642a6e4077aab583d6a677cc9f9",
        "output": OUTPUT / "stage1-rider-run-c-hd.png",
        "production_scale": 0.7596178670345461,
        "rotation_degrees": 0.0,
        "source_anchor": (850.0, 520.0),
    },
}

# Accepted visual-identity envelopes measured on the supplied masters. These
# are diagnostic landmarks only; they never drive production transforms.
IDENTITY_FEATURES = {
    "run-a": {
        "charlie_head": (545, 70, 1070, 370),
        "charlie_torso": (650, 305, 1010, 570),
        "lion_head": (925, 305, 1440, 745),
        "mane": (890, 305, 1295, 795),
        "saddle": (635, 475, 900, 690),
        "torso_depth": (400, 480, 900, 820),
        "paw": (1090, 805, 1375, 1005),
    },
    "run-b": {
        "charlie_head": (550, 65, 1070, 350),
        "charlie_torso": (650, 285, 1010, 555),
        "lion_head": (930, 290, 1445, 720),
        "mane": (900, 290, 1305, 765),
        "saddle": (650, 470, 905, 675),
        "torso_depth": (430, 480, 920, 795),
        "paw": (1065, 805, 1270, 985),
    },
    "run-c": {
        "charlie_head": (775, 55, 1155, 340),
        "charlie_torso": (770, 250, 1100, 500),
        "lion_head": (1050, 285, 1490, 650),
        "mane": (980, 280, 1440, 710),
        "saddle": (710, 415, 1010, 625),
        "torso_depth": (390, 420, 1060, 735),
        "paw": (1260, 695, 1490, 875),
    },
}


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def clean_alpha(image: Image.Image) -> tuple[Image.Image, dict]:
    alpha = np.asarray(image.getchannel("A"), dtype=np.float32)
    restored = np.clip(np.rint(alpha * (255.0 / 253.0)), 0, 255).astype(np.uint8)
    support_image = Image.fromarray((alpha >= 32).astype(np.uint8) * 255, "L")
    support = np.asarray(support_image.filter(ImageFilter.MaxFilter(9))) > 0
    remove = (alpha < 32) & ~support
    restored[remove] = 0
    result = image.copy()
    result.putalpha(Image.fromarray(restored, "L"))
    return result, {
        "continuous_restoration": "round(alpha * 255 / 253), clipped to 255",
        "support": "alpha >= 32 expanded with deterministic 9x9 max filter",
        "removed_pixels": int(remove.sum()),
        "removed_percent": round(float(remove.mean() * 100.0), 6),
        "changed_pixels": int((restored != alpha).sum()),
        "changed_percent": round(float((restored != alpha).mean() * 100.0), 6),
    }


def clean_original_c_alpha(image: Image.Image) -> tuple[Image.Image, dict]:
    """Remove the historical backdrop while preserving the subject fringe.

    The original C export contains a broad translucent colored backdrop. Its
    real subject is the dense alpha component. Build a spatial support region
    from alpha >= 224, expand that support by five source pixels, and retain
    the continuously restored alpha only inside it. This preserves the genuine
    low-alpha fur/hair/whisker fringe adjacent to the subject without retaining
    the distant glow/background. Character RGB pixels are never edited.
    """
    alpha = np.asarray(image.getchannel("A"), dtype=np.float32)
    restored = np.clip(np.rint(alpha * (255.0 / 253.0)), 0, 255).astype(np.uint8)
    subject_core = Image.fromarray((alpha >= 224).astype(np.uint8) * 255, "L")
    spatial_support = np.asarray(subject_core.filter(ImageFilter.MaxFilter(11))) > 0
    remove = ~spatial_support
    restored[remove] = 0
    result = image.copy()
    result.putalpha(Image.fromarray(restored, "L"))
    return result, {
        "continuous_restoration": "round(alpha * 255 / 253), clipped to 255",
        "subject_core": "alpha >= 224",
        "spatial_support": "deterministic 11x11 max filter (5 source-pixel fringe)",
        "removed_pixels": int(remove.sum()),
        "removed_percent": round(float(remove.mean() * 100.0), 6),
        "preservation_rule": "RGB unchanged; restored alpha retained only in subject support",
    }


def feature_dimensions(bounds):
    left, top, right, bottom = bounds
    return [float(right - left), float(bottom - top)]


def feature_measurements():
    raw = {
        pose: {
            feature: feature_dimensions(bounds)
            for feature, bounds in features.items()
        }
        for pose, features in IDENTITY_FEATURES.items()
    }
    target = {
        feature: [
            (raw["run-a"][feature][axis] + raw["run-b"][feature][axis]) / 2.0
            for axis in range(2)
        ]
        for feature in (
            "charlie_head", "charlie_torso", "lion_head", "mane", "saddle",
            "torso_depth", "paw",
        )
    }
    scales = {pose: specification["production_scale"] for pose, specification in POSES.items()}
    return raw, target, scales


def production_transform(image: Image.Image, specification: dict,
                         production_scale: float) -> tuple[Image.Image, list[float]]:
    angle = math.radians(specification["rotation_degrees"])
    rotation = np.asarray(
        ((math.cos(angle), -math.sin(angle)),
         (math.sin(angle), math.cos(angle))),
        dtype=np.float64,
    )
    source_anchor = np.asarray(specification["source_anchor"], dtype=np.float64)
    translation = np.asarray(PRODUCTION_BODY_ANCHOR, dtype=np.float64) - (
        source_anchor @ (production_scale * rotation)
    )
    forward = production_scale * rotation.T
    inverse = np.linalg.inv(forward)
    offset = -inverse @ translation
    production = image.transform(
        CANVAS,
        Image.Transform.AFFINE,
        (inverse[0, 0], inverse[0, 1], offset[0],
         inverse[1, 0], inverse[1, 1], offset[1]),
        Image.Resampling.BICUBIC,
    )
    return production, [float(value) for value in translation]


def alpha_stats(image: Image.Image) -> dict:
    alpha = np.asarray(image.getchannel("A"))
    rows, columns = np.where(alpha > 0)
    strong_rows, strong_columns = np.where(alpha >= 224)

    def bounds(xs, ys):
        if not len(xs):
            return None
        return [int(xs.min()), int(ys.min()), int(xs.max()) + 1, int(ys.max()) + 1]

    return {
        "dimensions": list(image.size), "mode": image.mode,
        "alpha_range": [int(alpha.min()), int(alpha.max())],
        "fully_transparent_percent": round(float((alpha == 0).mean() * 100), 6),
        "fully_opaque_percent": round(float((alpha == 255).mean() * 100), 6),
        "low_alpha_1_31_percent": round(float(((alpha > 0) & (alpha < 32)).mean() * 100), 6),
        "alpha_gt_0_bounds": bounds(columns, rows),
        "alpha_gte_224_bounds": bounds(strong_columns, strong_rows),
        "canvas_edge_contact": bool(np.any(alpha[0, :]) or np.any(alpha[-1, :]) or np.any(alpha[:, 0]) or np.any(alpha[:, -1])),
    }


def checkerboard(size, step=24):
    result = Image.new("RGBA", size, (224, 226, 230, 255))
    draw = ImageDraw.Draw(result)
    for y in range(0, size[1], step):
        for x in range(0, size[0], step):
            if (x // step + y // step) % 2:
                draw.rectangle((x, y, x + step - 1, y + step - 1), fill=(178, 182, 190, 255))
    return result


def save_srgb(image: Image.Image, path: Path) -> None:
    image.save(path, "PNG", optimize=False, compress_level=9, icc_profile=SRGB_ICC)


def transformed_feature_size(bounds, angle_degrees, production_scale):
    left, top, right, bottom = bounds
    width, height = right - left, bottom - top
    angle = math.radians(abs(angle_degrees))
    rendered_width = production_scale * (
        width * math.cos(angle) + height * math.sin(angle)
    )
    rendered_height = production_scale * (
        width * math.sin(angle) + height * math.cos(angle)
    )
    return [round(rendered_width, 2), round(rendered_height, 2)]


def composite_on_checker(image):
    background = checkerboard(CANVAS)
    background.alpha_composite(image)
    return background.convert("RGB")


def feature_canvas_bounds(pose, feature, production_scale):
    specification = POSES[pose]
    angle = math.radians(specification["rotation_degrees"])
    rotation = np.asarray(
        ((math.cos(angle), -math.sin(angle)),
         (math.sin(angle), math.cos(angle))),
        dtype=np.float64,
    )
    source_anchor = np.asarray(specification["source_anchor"], dtype=np.float64)
    translation = np.asarray(PRODUCTION_BODY_ANCHOR, dtype=np.float64) - (
        source_anchor @ (production_scale * rotation)
    )
    left, top, right, bottom = IDENTITY_FEATURES[pose][feature]
    corners = np.asarray(
        ((left, top), (right, top), (right, bottom), (left, bottom)),
        dtype=np.float64,
    )
    transformed = corners @ (production_scale * rotation) + translation
    return (
        float(transformed[:, 0].min()), float(transformed[:, 1].min()),
        float(transformed[:, 0].max()), float(transformed[:, 1].max()),
    )


def build_identity_scale_diagnostics(prepared, raw, target, scales):
    cell = (512, 384)
    side_by_side = Image.new("RGB", (cell[0] * 3, cell[1]), (20, 20, 20))
    for column, pose in enumerate(("run-a", "run-b", "run-c")):
        panel = composite_on_checker(prepared[pose]).resize(
            cell, Image.Resampling.LANCZOS
        )
        side_by_side.paste(panel, (column * cell[0], 0))
    side_by_side.save(DIAGNOSTICS / "identity-scale-side-by-side.jpg", quality=95)

    pairs = (("run-a", "run-b"), ("run-b", "run-c"), ("run-c", "run-a"))
    overlays = Image.new("RGB", (cell[0] * 3, cell[1]), (20, 20, 20))
    for column, (first, second) in enumerate(pairs):
        canvas = checkerboard(CANVAS)
        first_image = prepared[first].copy()
        second_image = prepared[second].copy()
        first_color = Image.new("RGBA", CANVAS, (0, 210, 255, 0))
        second_color = Image.new("RGBA", CANVAS, (255, 64, 130, 0))
        first_color.putalpha(first_image.getchannel("A").point(lambda value: value // 2))
        second_color.putalpha(second_image.getchannel("A").point(lambda value: value // 2))
        canvas.alpha_composite(first_color)
        canvas.alpha_composite(second_color)
        draw = ImageDraw.Draw(canvas)
        for pose, color in ((first, (0, 210, 255, 255)),
                            (second, (255, 64, 130, 255))):
            production_scale = scales[pose]
            for feature in ("charlie_head", "lion_head", "saddle"):
                draw.rectangle(
                    feature_canvas_bounds(pose, feature, production_scale),
                    outline=color, width=6,
                )
        draw.line(
            ((GAMEPLAY_ANCHOR[0] - 16, GAMEPLAY_ANCHOR[1]),
             (GAMEPLAY_ANCHOR[0] + 16, GAMEPLAY_ANCHOR[1])),
            fill=(255, 255, 255, 255), width=4,
        )
        draw.line(
            ((GAMEPLAY_ANCHOR[0], GAMEPLAY_ANCHOR[1] - 16),
             (GAMEPLAY_ANCHOR[0], GAMEPLAY_ANCHOR[1] + 16)),
            fill=(255, 255, 255, 255), width=4,
        )
        panel = canvas.convert("RGB").resize(cell, Image.Resampling.LANCZOS)
        ImageDraw.Draw(panel).text(
            (12, 12), f"{first} (cyan) / {second} (pink)",
            fill=(255, 255, 255),
        )
        overlays.paste(panel, (column * cell[0], 0))
    overlays.save(DIAGNOSTICS / "identity-scale-anchor-overlays.jpg", quality=95)

    measurements = {}
    for pose, features in IDENTITY_FEATURES.items():
        measurements[pose] = {
            feature: transformed_feature_size(
                bounds, POSES[pose]["rotation_degrees"],
                scales[pose],
            )
            for feature, bounds in features.items()
        }
    final_target = {
        feature: [round(value * ((scales["run-a"] + scales["run-b"]) / 2.0), 2) for value in dimensions]
        for feature, dimensions in target.items()
    }
    differences = {}
    for pose, features in measurements.items():
        differences[pose] = {
            feature: [
                round((features[feature][axis] / final_target[feature][axis] - 1.0) * 100.0, 2)
                for axis in range(2)
            ]
            for feature in target
        }
    (DIAGNOSTICS / "identity-scale-measurements.json").write_text(
        json.dumps(
            {
                "method": "A/B accepted scales unchanged; C-only scale derived from A/B mean Charlie head height and torso height; no runtime scaling",
                "units": "production canvas pixels [width,height]",
                "raw_source_feature_pixels": raw,
                "grounded_target_source_pixels": target,
                "effective_source_to_canvas_scales": {
                    pose: scale for pose, scale in scales.items()
                },
                "final_target_pixels": final_target,
                "final_feature_pixels": measurements,
                "percent_difference_from_grounded_target": differences,
            },
            indent=2,
        ) + "\n",
        encoding="utf-8",
    )


def main() -> int:
    OUTPUT.mkdir(parents=True, exist_ok=True)
    DIAGNOSTICS.mkdir(parents=True, exist_ok=True)
    raw_features, target_features, effective_scales = feature_measurements()
    metadata = {"canvas": list(CANVAS), "gameplay_anchor": list(GAMEPLAY_ANCHOR), "production_body_anchor": list(PRODUCTION_BODY_ANCHOR), "body_to_gameplay_anchor_vector": [GAMEPLAY_ANCHOR[0] - PRODUCTION_BODY_ANCHOR[0], GAMEPLAY_ANCHOR[1] - PRODUCTION_BODY_ANCHOR[1]], "production_scale_rule": "corrected A/B/C relationship enlarged together by 1.30 twice (1.69 total); identical runtime pixel density", "effective_source_to_canvas_scales": effective_scales, "alpha_representation": "straight/unassociated RGBA", "color_profile": "embedded sRGB ICC", "poses": {}}
    prepared = {}
    for pose, specification in POSES.items():
        source = specification["source"]
        actual_source_hash = sha256(source)
        if actual_source_hash != specification["source_sha256"]:
            raise SystemExit(
                f"{pose}: supplied master hash changed: {actual_source_hash}"
            )
        image = Image.open(source).convert("RGBA")
        cleaned, cleanup = (
            clean_original_c_alpha(image) if pose == "run-c" else clean_alpha(image)
        )
        production_scale = effective_scales[pose]
        production, translation = production_transform(
            cleaned, specification, production_scale
        )
        save_srgb(production, specification["output"])
        prepared[pose] = production
        metadata["poses"][pose] = {
            "source": str(source.relative_to(ROOT)), "source_sha256": actual_source_hash,
            "output": str(specification["output"].relative_to(ROOT)), "output_sha256": sha256(specification["output"]),
            "cleanup": cleanup,
            "production_transform": {"source_to_canvas_scale": production_scale, "rotation_degrees": specification["rotation_degrees"], "translation": [round(value, 6) for value in translation], "source_anchor": list(specification["source_anchor"])},
            "production_anchor": list(GAMEPLAY_ANCHOR), "output_stats": alpha_stats(production),
        }

    cell = (512, 384)
    sheet = Image.new("RGB", (cell[0] * 3, cell[1] * 3), (20, 20, 20))
    backgrounds = ((248, 248, 245, 255), (16, 18, 24, 255), None)
    for column, pose in enumerate(("run-a", "run-b", "run-c")):
        for row, background in enumerate(backgrounds):
            base = checkerboard(CANVAS) if background is None else Image.new("RGBA", CANVAS, background)
            base.alpha_composite(prepared[pose])
            sheet.paste(base.resize(cell, Image.Resampling.LANCZOS).convert("RGB"), (column * cell[0], row * cell[1]))
    sheet.save(DIAGNOSTICS / "alpha-light-dark-checkerboard.jpg", quality=95)
    build_identity_scale_diagnostics(
        prepared, raw_features, target_features, effective_scales
    )
    (DIAGNOSTICS / "production-metadata.json").write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")

    failures = [pose for pose, item in metadata["poses"].items() if item["output_stats"]["dimensions"] != list(CANVAS) or item["output_stats"]["mode"] != "RGBA" or item["output_stats"]["canvas_edge_contact"]]
    if failures:
        raise SystemExit("production validation failed: " + ", ".join(failures))
    print(json.dumps({"status": "ok", "outputs": [str(POSES[p]["output"]) for p in POSES]}, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

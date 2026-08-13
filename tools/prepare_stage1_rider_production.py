#!/usr/bin/env python3
"""Prepare accepted Level 1 rider art without modifying supplied masters.

The cleanup and one-transform normalization are deterministic. This tool is
intentionally specific to the accepted A2/B2/C8 set and writes only production
copies plus diagnostics/metadata.
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
CANVAS = (1024, 768)
GAMEPLAY_ANCHOR = (512, 640)
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
# This is only the base source-to-canvas density. Each master receives an
# offline identity-normalization factor derived below from stable head/mane/
# saddle feature sizes. Runtime still draws every 1024x768 PNG identically.
BASE_PRODUCTION_SCALE = 0.41865765732

POSES = {
    "run-a": {
        "source": ROOT / "run/run a 2.png",
        "source_sha256": "50f2b9b381c630d8740c1b947e1ce0e97824dc987370b2715a93948b6d09d207",
        "output": OUTPUT / "stage1-rider-run-a-hd.png",
        "old_effective_scale": 0.41336292208,
        "rotation_degrees": -2.734871,
        "source_anchor": (718.131548, 999.403464),
    },
    "run-b": {
        "source": ROOT / "run/Run b 2.png",
        "source_sha256": "add70517b36a9b921d7a43d8286d7e35f26ba8d132aad4746dca7a1261429a90",
        "output": OUTPUT / "stage1-rider-run-b-hd.png",
        "old_effective_scale": 0.42395239256,
        "rotation_degrees": -1.406612,
        "source_anchor": (749.021806, 974.150163),
    },
    "run-c": {
        "source": ROOT / "run/run c 8.png",
        "source_sha256": "949ff7cd42c37a02980cea7cd3f103288dfbe1e785d365da111ad7574e2b4d9d",
        "output": OUTPUT / "stage1-rider-run-c-hd.png",
        "old_effective_scale": 0.57708686360,
        "rotation_degrees": -6.077358,
        "source_anchor": (969.088108, 834.642175),
    },
}

# Accepted visual-identity envelopes measured on the supplied masters. These
# are diagnostic landmarks only; they never drive production transforms.
IDENTITY_FEATURES = {
    "run-a": {
        "charlie_head": (545, 70, 1070, 370),
        "lion_head": (925, 305, 1440, 745),
        "mane": (890, 305, 1295, 795),
        "saddle": (635, 475, 900, 690),
        "torso_depth": (400, 480, 900, 820),
    },
    "run-b": {
        "charlie_head": (550, 65, 1070, 350),
        "lion_head": (930, 290, 1445, 720),
        "mane": (900, 290, 1305, 765),
        "saddle": (650, 470, 905, 675),
        "torso_depth": (430, 480, 920, 795),
    },
    "run-c": {
        "charlie_head": (880, 45, 1260, 350),
        "lion_head": (1050, 295, 1580, 690),
        "mane": (1000, 295, 1435, 720),
        "saddle": (745, 420, 1030, 610),
        "torso_depth": (510, 425, 1120, 700),
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


def feature_dimensions(bounds):
    left, top, right, bottom = bounds
    return [float(right - left), float(bottom - top)]


def identity_normalization_factors():
    # A2/B2 establish the grounded physical-size target. Use all dimensions of
    # the stable identity envelopes together; no pose span, limb, tail, torso,
    # hip/shoulder/seat triangle, or outer silhouette participates.
    fitted_features = ("charlie_head", "lion_head", "mane", "saddle")
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
        for feature in fitted_features
    }
    factors = {}
    for pose in POSES:
        source_values = []
        target_values = []
        for feature in fitted_features:
            source_values.extend(raw[pose][feature])
            target_values.extend(target[feature])
        source_vector = np.asarray(source_values, dtype=np.float64)
        target_vector = np.asarray(target_values, dtype=np.float64)
        factors[pose] = float(
            np.dot(source_vector, target_vector) /
            np.dot(source_vector, source_vector)
        )
    return raw, target, factors


def production_transform(image: Image.Image, specification: dict,
                         production_scale: float) -> tuple[Image.Image, list[float]]:
    angle = math.radians(specification["rotation_degrees"])
    rotation = np.asarray(
        ((math.cos(angle), -math.sin(angle)),
         (math.sin(angle), math.cos(angle))),
        dtype=np.float64,
    )
    source_anchor = np.asarray(specification["source_anchor"], dtype=np.float64)
    translation = np.asarray(GAMEPLAY_ANCHOR, dtype=np.float64) - (
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
    translation = np.asarray(GAMEPLAY_ANCHOR, dtype=np.float64) - (
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


def build_identity_scale_diagnostics(prepared, raw, target, factors):
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
            production_scale = BASE_PRODUCTION_SCALE * factors[pose]
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
                BASE_PRODUCTION_SCALE * factors[pose],
            )
            for feature, bounds in features.items()
        }
    final_target = {
        feature: [round(value * BASE_PRODUCTION_SCALE, 2) for value in dimensions]
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
                "method": "least-squares offline per-source scalar using Charlie head, lion head, mane and saddle dimensions against the A2/B2 mean; dimensions include retained pose rotation",
                "units": "production canvas pixels [width,height]",
                "raw_source_feature_pixels": raw,
                "grounded_target_source_pixels": target,
                "base_production_scale": BASE_PRODUCTION_SCALE,
                "identity_normalization_factors": factors,
                "effective_source_to_canvas_scales": {
                    pose: BASE_PRODUCTION_SCALE * factor
                    for pose, factor in factors.items()
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
    raw_features, target_features, normalization_factors = identity_normalization_factors()
    effective_scales = {
        pose: BASE_PRODUCTION_SCALE * factor
        for pose, factor in normalization_factors.items()
    }
    metadata = {"canvas": list(CANVAS), "gameplay_anchor": list(GAMEPLAY_ANCHOR), "production_scale_rule": "offline per-source identity-feature normalization; identical 1024x768 runtime rendering", "base_production_scale": BASE_PRODUCTION_SCALE, "identity_normalization_factors": normalization_factors, "effective_source_to_canvas_scales": effective_scales, "old_effective_scales": {pose: specification["old_effective_scale"] for pose, specification in POSES.items()}, "alpha_representation": "straight/unassociated RGBA", "color_profile": "embedded sRGB ICC", "poses": {}}
    prepared = {}
    for pose, specification in POSES.items():
        source = specification["source"]
        actual_source_hash = sha256(source)
        if actual_source_hash != specification["source_sha256"]:
            raise SystemExit(
                f"{pose}: supplied master hash changed: {actual_source_hash}"
            )
        image = Image.open(source).convert("RGBA")
        cleaned, cleanup = clean_alpha(image)
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
            "production_transform": {"offline_identity_factor": normalization_factors[pose], "source_to_canvas_scale": production_scale, "rotation_degrees": specification["rotation_degrees"], "translation": [round(value, 6) for value in translation], "source_anchor": list(specification["source_anchor"])},
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
        prepared, raw_features, target_features, normalization_factors
    )
    (DIAGNOSTICS / "production-metadata.json").write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")

    failures = [pose for pose, item in metadata["poses"].items() if item["output_stats"]["dimensions"] != list(CANVAS) or item["output_stats"]["mode"] != "RGBA" or item["output_stats"]["canvas_edge_contact"]]
    if failures:
        raise SystemExit("production validation failed: " + ", ".join(failures))
    print(json.dumps({"status": "ok", "outputs": [str(POSES[p]["output"]) for p in POSES]}, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

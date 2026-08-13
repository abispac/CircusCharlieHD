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
from pathlib import Path

import numpy as np
from PIL import Image, ImageCms, ImageDraw, ImageFilter


ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "assets"
DIAGNOSTICS = ROOT / "docs/diagnostics/level1-rider-production"
CANVAS = (1024, 768)
GAMEPLAY_ANCHOR = (512, 640)
# C8 is intentionally much wider than A/B. After the accepted anatomical
# normalization, apply one common uniform scale to every pose about the shared
# gameplay anchor so the complete C8 tail/paws fit without cropping. Using the
# same factor for all three preserves their accepted relative scale.
COMMON_PRODUCTION_SCALE = 0.92

POSES = {
    "run-a": {
        "source": ROOT / "run/run a 2.png",
        "source_sha256": "50f2b9b381c630d8740c1b947e1ce0e97824dc987370b2715a93948b6d09d207",
        "output": OUTPUT / "stage1-rider-run-a-hd.png",
        "landmarks": {"hip": (410, 600), "shoulder": (1010, 485), "seat": (525, 515)},
        "targets": {"hip": (344, 416), "shoulder": (640, 400), "seat": (488, 472)},
    },
    "run-b": {
        "source": ROOT / "run/Run b 2.png",
        "source_sha256": "add70517b36a9b921d7a43d8286d7e35f26ba8d132aad4746dca7a1261429a90",
        "output": OUTPUT / "stage1-rider-run-b-hd.png",
        "landmarks": {"hip": (430, 580), "shoulder": (1010, 460), "seat": (530, 500)},
        "targets": {"hip": (328, 416), "shoulder": (624, 392), "seat": (472, 472)},
    },
    "run-c": {
        "source": ROOT / "run/run c 8.png",
        "source_sha256": "949ff7cd42c37a02980cea7cd3f103288dfbe1e785d365da111ad7574e2b4d9d",
        "output": OUTPUT / "stage1-rider-run-c-hd.png",
        "landmarks": {"hip": (625, 570), "shoulder": (1110, 485), "seat": (850, 470)},
        "targets": {"hip": (320, 416), "shoulder": (624, 400), "seat": (456, 472)},
    },
}


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def similarity(source: list[tuple[int, int]], target: list[tuple[int, int]]):
    x = np.asarray(source, dtype=np.float64)
    y = np.asarray(target, dtype=np.float64)
    mean_x, mean_y = x.mean(axis=0), y.mean(axis=0)
    x0, y0 = x - mean_x, y - mean_y
    u, singular, vt = np.linalg.svd(x0.T @ y0)
    rotation = u @ vt
    if np.linalg.det(rotation) < 0:
        u[:, -1] *= -1
        rotation = u @ vt
    scale = float(singular.sum() / (x0 * x0).sum())
    translation = mean_y - scale * mean_x @ rotation
    return scale, rotation, translation


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


def transform(image: Image.Image, fit) -> Image.Image:
    scale, rotation, translation = fit
    forward = scale * rotation.T
    inverse = np.linalg.inv(forward)
    offset = -inverse @ translation
    normalized = image.transform(
        CANVAS,
        Image.Transform.AFFINE,
        (inverse[0, 0], inverse[0, 1], offset[0],
         inverse[1, 0], inverse[1, 1], offset[1]),
        Image.Resampling.BICUBIC,
    )
    # PIL's affine transform below maps destination pixels back into the
    # normalized canvas. This is a uniform scale around (512,640), never a
    # per-pose runtime correction or nonuniform geometry change.
    inverse_scale = 1.0 / COMMON_PRODUCTION_SCALE
    anchor_x, anchor_y = GAMEPLAY_ANCHOR
    return normalized.transform(
        CANVAS,
        Image.Transform.AFFINE,
        (
            inverse_scale, 0.0, anchor_x * (1.0 - inverse_scale),
            0.0, inverse_scale, anchor_y * (1.0 - inverse_scale),
        ),
        Image.Resampling.BICUBIC,
    )


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
    profile = ImageCms.ImageCmsProfile(ImageCms.createProfile("sRGB")).tobytes()
    image.save(path, "PNG", optimize=False, compress_level=9, icc_profile=profile)


def main() -> int:
    OUTPUT.mkdir(parents=True, exist_ok=True)
    DIAGNOSTICS.mkdir(parents=True, exist_ok=True)
    metadata = {"canvas": list(CANVAS), "gameplay_anchor": list(GAMEPLAY_ANCHOR), "common_post_normalization_scale": COMMON_PRODUCTION_SCALE, "alpha_representation": "straight/unassociated RGBA", "color_profile": "embedded sRGB ICC", "poses": {}}
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
        keys = ("hip", "shoulder", "seat")
        fit = similarity([specification["landmarks"][key] for key in keys], [specification["targets"][key] for key in keys])
        production = transform(cleaned, fit)
        save_srgb(production, specification["output"])
        prepared[pose] = production
        metadata["poses"][pose] = {
            "source": str(source.relative_to(ROOT)), "source_sha256": actual_source_hash,
            "output": str(specification["output"].relative_to(ROOT)), "output_sha256": sha256(specification["output"]),
            "cleanup": cleanup,
            "similarity_transform": {"scale": round(fit[0], 9), "rotation_degrees": round(math.degrees(math.atan2(fit[1][1, 0], fit[1][0, 0])), 6), "translation": [round(float(value), 6) for value in fit[2]]},
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
    (DIAGNOSTICS / "production-metadata.json").write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")

    failures = [pose for pose, item in metadata["poses"].items() if item["output_stats"]["dimensions"] != list(CANVAS) or item["output_stats"]["mode"] != "RGBA" or item["output_stats"]["canvas_edge_contact"]]
    if failures:
        raise SystemExit("production validation failed: " + ", ".join(failures))
    print(json.dumps({"status": "ok", "outputs": [str(POSES[p]["output"]) for p in POSES]}, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""Normalize generated Stage 1 rider sheets into deterministic game atlases."""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path

import numpy as np
from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
SOURCE_ROOT = ROOT / "assets" / "source-art"
ASSET_ROOT = ROOT / "assets"
FRAME_WIDTH = 512


@dataclass(frozen=True)
class Sheet:
    source: str
    output: str
    columns: int
    rows: int
    frames: str | None = None


SHEETS = (
    Sheet(
        "stage1-rider-walk-12-v9-alpha.png",
        "stage1-rider-walk-12-v9.png",
        4,
        3,
        "stage1-rider-walk-12-v9-frames",
    ),
    Sheet(
        "stage1-burn-rider-v1-alpha.png",
        "stage1-burn-rider-v1.png",
        4,
        1,
    ),
    Sheet(
        "stage1-finish-rider-v2-alpha.png",
        "stage1-finish-rider-v2.png",
        4,
        1,
    ),
)


def cell_bounds(length: int, count: int, index: int) -> tuple[int, int]:
    return round(index * length / count), round((index + 1) * length / count)


def occupied_vertical_range(image: Image.Image, sheet: Sheet) -> tuple[int, int]:
    alpha = np.asarray(image)[:, :, 3]
    occupied_top: list[int] = []
    occupied_bottom: list[int] = []
    for row in range(sheet.rows):
        y0, y1 = cell_bounds(image.height, sheet.rows, row)
        for column in range(sheet.columns):
            x0, x1 = cell_bounds(image.width, sheet.columns, column)
            mask = alpha[y0:y1, x0:x1] > 24
            ys = np.flatnonzero(mask.any(axis=1))
            if not len(ys):
                raise RuntimeError(
                    f"Empty frame at row {row}, column {column} in {sheet.source}"
                )
            occupied_top.append(int(ys[0]))
            occupied_bottom.append(int(ys[-1]) + 1)
    padding = 5
    smallest_cell_height = image.height // sheet.rows
    return (
        max(0, min(occupied_top) - padding),
        min(smallest_cell_height, max(occupied_bottom) + padding),
    )


def alpha_box(image: Image.Image) -> tuple[int, int, int, int]:
    mask = np.asarray(image)[:, :, 3] > 24
    ys, xs = np.nonzero(mask)
    if not len(xs):
        raise RuntimeError("Generated frame has no visible pixels")
    return int(xs.min()), int(ys.min()), int(xs.max()) + 1, int(ys.max()) + 1


def mane_anchor(image: Image.Image) -> tuple[int, int]:
    pixels = np.asarray(image)
    alpha = pixels[:, :, 3] > 96
    rgb = pixels[:, :, :3]
    dark_gold = (
        alpha
        & (rgb[:, :, 0] < 205)
        & (rgb[:, :, 1] < 135)
        & (rgb[:, :, 2] < 70)
    )
    dark_gold[:, : int(image.width * 0.48)] = False
    ys, xs = np.nonzero(dark_gold)
    if not len(xs):
        ys, xs = np.nonzero(alpha)
    return int(np.median(xs)), int(np.median(ys))


def normalize_rider(sheet: Sheet, source: Image.Image) -> None:
    source_cell_width = source.width / sheet.columns
    scale = FRAME_WIDTH / source_cell_width
    target_anchor = (382, 170)
    prepared: list[tuple[Image.Image, int, int]] = []
    min_y = 10_000
    max_y = -10_000

    for row in range(sheet.rows):
        y0, y1 = cell_bounds(source.height, sheet.rows, row)
        for column in range(sheet.columns):
            x0, x1 = cell_bounds(source.width, sheet.columns, column)
            cell = source.crop((x0, y0, x1, y1))
            bx0, by0, bx1, by1 = alpha_box(cell)
            padding = 4
            crop = cell.crop(
                (
                    max(0, bx0 - padding),
                    max(0, by0 - padding),
                    min(cell.width, bx1 + padding),
                    min(cell.height, by1 + padding),
                )
            )
            crop = crop.resize(
                (round(crop.width * scale), round(crop.height * scale)),
                Image.Resampling.LANCZOS,
            )
            anchor_x, anchor_y = mane_anchor(crop)
            paste_x = target_anchor[0] - anchor_x
            paste_y = target_anchor[1] - anchor_y
            visible = alpha_box(crop)
            min_y = min(min_y, paste_y + visible[1])
            max_y = max(max_y, paste_y + visible[3])
            prepared.append((crop, paste_x, paste_y))

    vertical_padding = 5
    frame_height = max_y - min_y + vertical_padding * 2
    y_shift = vertical_padding - min_y
    atlas = Image.new(
        "RGBA",
        (FRAME_WIDTH * sheet.columns, frame_height * sheet.rows),
        (0, 0, 0, 0),
    )
    frame_root = SOURCE_ROOT / sheet.frames if sheet.frames else None
    if frame_root:
        frame_root.mkdir(parents=True, exist_ok=True)

    alpha_bottoms: list[int] = []
    for index, (crop, paste_x, paste_y) in enumerate(prepared):
        frame = Image.new("RGBA", (FRAME_WIDTH, frame_height), (0, 0, 0, 0))
        frame.alpha_composite(crop, (paste_x, paste_y + y_shift))
        alpha_bottoms.append(alpha_box(frame)[3])
        row, column = divmod(index, sheet.columns)
        atlas.alpha_composite(
            frame, (column * FRAME_WIDTH, row * frame_height)
        )
        if frame_root:
            frame.save(
                frame_root / f"rider-walk-{index + 1:02d}.png",
                optimize=True,
            )

    output = ASSET_ROOT / sheet.output
    atlas.save(output, optimize=True)
    print(
        f"{output.name}: {sheet.columns}x{sheet.rows}, "
        f"cell={FRAME_WIDTH}x{frame_height}, alpha_bottoms={alpha_bottoms}"
    )


def normalize(sheet: Sheet) -> None:
    source = Image.open(SOURCE_ROOT / sheet.source).convert("RGBA")
    if sheet.rows > 1:
        normalize_rider(sheet, source)
        return
    crop_top, crop_bottom = occupied_vertical_range(source, sheet)
    source_cell_width = source.width / sheet.columns
    scale = FRAME_WIDTH / source_cell_width
    frame_height = round((crop_bottom - crop_top) * scale)
    atlas = Image.new(
        "RGBA",
        (FRAME_WIDTH * sheet.columns, frame_height * sheet.rows),
        (0, 0, 0, 0),
    )

    frame_root = SOURCE_ROOT / sheet.frames if sheet.frames else None
    if frame_root:
        frame_root.mkdir(parents=True, exist_ok=True)

    alpha_bottoms: list[int] = []
    frame_number = 0
    for row in range(sheet.rows):
        cell_y0, _ = cell_bounds(source.height, sheet.rows, row)
        for column in range(sheet.columns):
            x0, x1 = cell_bounds(source.width, sheet.columns, column)
            frame = source.crop(
                (x0, cell_y0 + crop_top, x1, cell_y0 + crop_bottom)
            ).resize((FRAME_WIDTH, frame_height), Image.Resampling.LANCZOS)
            atlas.alpha_composite(
                frame, (column * FRAME_WIDTH, row * frame_height)
            )
            mask = np.asarray(frame)[:, :, 3] > 24
            ys = np.flatnonzero(mask.any(axis=1))
            alpha_bottoms.append(int(ys[-1]) + 1)
            frame_number += 1
            if frame_root:
                frame.save(
                    frame_root / f"rider-walk-{frame_number:02d}.png",
                    optimize=True,
                )

    output = ASSET_ROOT / sheet.output
    atlas.save(output, optimize=True)
    print(
        f"{output.name}: {sheet.columns}x{sheet.rows}, "
        f"cell={FRAME_WIDTH}x{frame_height}, alpha_bottoms={alpha_bottoms}"
    )


def main() -> None:
    for sheet in SHEETS:
        normalize(sheet)


if __name__ == "__main__":
    main()

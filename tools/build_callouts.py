#!/usr/bin/env python3
"""HD versions of the board's balloon callouts (GREAT, FAR OUT, OH NO!!),
traced from the arcade's own pixels (art/source/refs/callout-*-arcade.png,
MAME frames) and smoothed 8x.  Each callout is three white layers the game
tints at draw time -- the cloud, the letters and the outline -- because the
board cycles the goal callouts' palette every two frames.

    assets/callout-<name>-{cloud,letters,outline}.png
    assets/callout-<name>.json     {"width": board columns, "height": rows}
"""
import json, os
import numpy as np
from PIL import Image, ImageFilter
from scipy import ndimage

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REFS = os.path.join(ROOT, "art", "source", "refs")
OUT = os.path.join(ROOT, "assets")
SCALE = 8

# name: (file, box x0,y0,x1,y1 on the 224x256 frame, cloud rgb, letters rgb,
# outline rgb).  Each callout is three 16x16 cells (GREAT and the left OH NO
# on row $AA, FAR OUT and the right OH NO eight rows lower): the box is
# exactly those 48 columns by 16 rows, so the game draws the layers over the
# cells' own place.
CALLOUTS = {
    "great":  ("callout-great-arcade.png", (80, 170, 128, 186), (255, 71, 0), (255, 255, 255), (184, 0, 0)),
    "farout": ("callout-great-arcade.png", (152, 178, 200, 194), (255, 71, 0), (255, 255, 255), (184, 0, 0)),
    "ohno":   ("callout-ohno-arcade.png", (32, 170, 80, 186), (184, 255, 0), (255, 71, 0), (184, 0, 0)),
}


def near(a, rgb, tol=40):
    return (np.abs(a - np.array(rgb)).sum(axis=2) < tol)


def smooth(mask):
    big = np.kron(mask.astype(np.float32), np.ones((SCALE, SCALE), np.float32))
    img = Image.fromarray((big * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(SCALE * 0.5))
    soft = np.asarray(img).astype(np.float32) / 255.0
    # pull the blurred edge back to a crisp, rounded one
    return np.clip((soft - 0.5) * 4.0 + 0.5, 0.0, 1.0)


for name, (src, box, cloud_rgb, letters_rgb, outline_rgb) in CALLOUTS.items():
    a = np.asarray(Image.open(os.path.join(REFS, src)).convert("RGB")).astype(int)
    x0, y0, x1, y1 = box
    a = a[y0:y1, x0:x1]
    cloud, letters, outline = near(a, cloud_rgb), near(a, letters_rgb), near(a, outline_rgb)
    # the callout is the one big blob of its three colours; stray crowd
    # pixels of the same colours are dropped with it
    any_ = ndimage.binary_closing(cloud | letters | outline, iterations=1)
    lab, n = ndimage.label(any_)
    sizes = ndimage.sum(any_, lab, range(1, n + 1))
    keep = lab == (int(np.argmax(sizes)) + 1)
    keep = ndimage.binary_dilation(keep, iterations=1)
    cloud, letters, outline = cloud & keep, letters & keep, outline & keep
    body = ndimage.binary_fill_holes(cloud | letters | outline)
    s_body = smooth(body)
    s_letters = smooth(letters)
    s_outline = np.maximum(smooth(outline), np.clip(smooth(body) - smooth(ndimage.binary_erosion(body)), 0, 1))
    h, w = s_body.shape
    for layer, arr in (("cloud", s_body), ("letters", s_letters), ("outline", s_outline)):
        rgba = np.dstack([np.full((h, w), 255, np.uint8)] * 3 + [(np.clip(arr, 0, 1) * 255).astype(np.uint8)])
        Image.fromarray(rgba, "RGBA").save(os.path.join(OUT, f"callout-{name}-{layer}.png"))
    json.dump({"width": int(body.shape[1]), "height": int(body.shape[0])},
              open(os.path.join(OUT, f"callout-{name}.json"), "w"))
    print(name, body.shape[::-1], "->", (w, h))

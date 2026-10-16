#!/usr/bin/env python3
"""Render the HD art of Event 3's water round (the dolphins) from SVG.

Writes into assets/:
  stage3-dolphin-4-v1.png     four tail poses of a dolphin facing left (4x1)
  stage3-splash-4-v1.png      four stages of a splash (4x1)
  stage3-water-tile-v1.png    a seamless water texture (tiles horizontally)
  stage3-water-surface-v1.png the bright surface band (tiles horizontally)
  stage3-pool-ladder-v1.png   a pool ladder
  stage3-drum-ring-v1.png     the ring of foam/shadow where a drum meets the water

Run from the repository root:  python3 tools/render_stage3_water_art.py
"""
import io
import math
import os
import sys

import cairosvg
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "assets")


def render(svg, width, height):
    png = cairosvg.svg2png(bytestring=svg.encode("utf-8"), output_width=width,
                           output_height=height)
    return Image.open(io.BytesIO(png)).convert("RGBA")


def dolphin_svg(tail_angle):
    # A bottlenose dolphin facing left in a 256x128 box: a melon forehead, a
    # narrow beak, a swept dorsal fin and a fluke that rotates about the tail
    # stock (214, 66) for the four swimming poses.
    return f"""<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 256 128">
<defs>
  <linearGradient id="body" x1="0" y1="0" x2="0" y2="1">
    <stop offset="0" stop-color="#9cc4dc"/>
    <stop offset="0.40" stop-color="#5d93b8"/>
    <stop offset="0.58" stop-color="#9fc4da"/>
    <stop offset="0.70" stop-color="#e2eff6"/>
    <stop offset="1" stop-color="#f7fbfd"/>
  </linearGradient>
  <linearGradient id="fin" x1="0" y1="0" x2="0" y2="1">
    <stop offset="0" stop-color="#7cabc9"/>
    <stop offset="1" stop-color="#3e6f94"/>
  </linearGradient>
  <linearGradient id="shine" x1="0" y1="0" x2="0" y2="1">
    <stop offset="0" stop-color="#ffffff" stop-opacity="0.6"/>
    <stop offset="1" stop-color="#ffffff" stop-opacity="0"/>
  </linearGradient>
</defs>
<!-- fluke: two thin lobes swept back -->
<g transform="translate(208 64) rotate({tail_angle})">
  <path d="M 0 -5 C 12 -16, 28 -30, 48 -36 C 44 -24, 36 -12, 26 -2 C 36 8, 44 22, 48 36 C 28 30, 12 16, 0 5 Z" fill="url(#fin)" stroke="#2b5170" stroke-width="2.5" stroke-linejoin="round"/>
</g>
<!-- body: melon, beak, arched back and a narrow tail stock -->
<path d="M 10 74 C 16 66, 26 60, 42 58 C 52 44, 72 34, 100 32 C 134 30, 168 42, 196 56 C 202 59, 206 62, 209 64 C 206 67, 202 69, 196 71 C 170 84, 136 96, 104 98 C 80 99, 58 94, 44 84 C 32 82, 20 80, 10 74 Z" fill="url(#body)" stroke="#2b5170" stroke-width="3" stroke-linejoin="round"/>
<!-- dorsal fin -->
<path d="M 108 33 C 114 14, 132 6, 150 12 C 140 18, 132 26, 130 36 Z" fill="url(#fin)" stroke="#2b5170" stroke-width="2.5" stroke-linejoin="round"/>
<!-- pectoral fin -->
<path d="M 84 82 C 88 98, 102 110, 120 112 C 110 102, 104 92, 102 80 Z" fill="url(#fin)" stroke="#2b5170" stroke-width="2.5" stroke-linejoin="round"/>
<!-- beak crease and smile -->
<path d="M 12 72 C 24 68, 36 66, 48 64" fill="none" stroke="#2b5170" stroke-width="2.4" stroke-linecap="round"/>
<path d="M 14 78 C 26 84, 42 86, 56 82" fill="none" stroke="#2b5170" stroke-width="2.2" stroke-linecap="round"/>
<!-- eye -->
<circle cx="62" cy="56" r="6" fill="#ffffff" stroke="#2b5170" stroke-width="2"/>
<circle cx="63" cy="57" r="3.3" fill="#1b2a38"/>
<circle cx="61.5" cy="55" r="1.2" fill="#ffffff"/>
<!-- blowhole and highlight -->
<path d="M 92 35 c 3 -3, 7 -3, 10 0" fill="none" stroke="#2b5170" stroke-width="2" stroke-linecap="round"/>
<path d="M 56 46 C 90 36, 150 38, 192 56 C 150 50, 95 48, 56 56 Z" fill="url(#shine)"/>
</svg>"""


def splash_svg(stage):
    # stage 0..3: a growing then thinning crown of water.
    scale = [0.55, 0.85, 1.0, 1.1][stage]
    alpha = [0.95, 0.95, 0.85, 0.6][stage]
    drops = ""
    for i in range(7):
        a = -80 + i * 27
        r = 48 * scale + (10 if i % 2 else 0)
        x = 64 + r * math.sin(math.radians(a))
        y = 100 - r * math.cos(math.radians(a))
        rad = 4 + (i % 3) * 1.5
        drops += f'<circle cx="{x:.1f}" cy="{y:.1f}" r="{rad * scale:.1f}" fill="#e8f6ff" stroke="#8fc8ea" stroke-width="1.5"/>'
    crown = ""
    for i in range(5):
        a = -60 + i * 30
        r = 40 * scale
        x = 64 + r * math.sin(math.radians(a))
        y = 100 - r * math.cos(math.radians(a))
        crown += f'<path d="M 64 100 Q {64 + (x - 64) * 0.5:.1f} {100 - (100 - y) * 0.9:.1f} {x:.1f} {y:.1f}" fill="none" stroke="#dff3ff" stroke-width="{6 * scale:.1f}" stroke-linecap="round"/>'
    return f"""<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 128 128">
<g opacity="{alpha}">
<ellipse cx="64" cy="104" rx="{44 * scale:.1f}" ry="{10 * scale:.1f}" fill="#bfe6fb" stroke="#8fc8ea" stroke-width="2"/>
{crown}
{drops}
</g>
</svg>"""


def water_tile_svg():
    # 256x192, horizontally seamless: wave paths repeat with period 128.
    waves = ""
    for row in range(8):
        y = 12 + row * 24
        phase = (row % 2) * 32
        op = 0.35 if row % 2 else 0.22
        d = f"M {-128 + phase} {y}"
        for k in range(-1, 4):
            x0 = k * 128 + phase
            d += f" c 24 -10, 40 -10, 64 0 c 24 10, 40 10, 64 0"
        waves += f'<path d="{d}" fill="none" stroke="#ffffff" stroke-opacity="{op}" stroke-width="3" stroke-linecap="round"/>'
    return f"""<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 256 192">
<defs>
  <linearGradient id="deep" x1="0" y1="0" x2="0" y2="1">
    <stop offset="0" stop-color="#3fa0e6"/>
    <stop offset="1" stop-color="#1b5aa8"/>
  </linearGradient>
</defs>
<rect width="256" height="192" fill="url(#deep)"/>
{waves}
</svg>"""


def surface_svg():
    # 256x40, seamless: the bright line of the surface with a soft foam edge.
    return """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 256 40">
<defs>
  <linearGradient id="foam" x1="0" y1="0" x2="0" y2="1">
    <stop offset="0" stop-color="#ffffff" stop-opacity="0.9"/>
    <stop offset="0.35" stop-color="#d8f2ff" stop-opacity="0.6"/>
    <stop offset="1" stop-color="#8fd0f5" stop-opacity="0"/>
  </linearGradient>
</defs>
<rect width="256" height="40" fill="url(#foam)"/>
<path d="M -32 6 c 16 -4, 32 4, 48 0 s 32 -4, 48 0 s 32 4, 48 0 s 32 -4, 48 0 s 32 4, 48 0 s 32 -4, 48 0 s 32 4, 48 0" fill="none" stroke="#ffffff" stroke-width="3" stroke-linecap="round" stroke-opacity="0.95"/>
</svg>"""


def ladder_svg():
    return """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 160">
<defs>
  <linearGradient id="chrome" x1="0" y1="0" x2="1" y2="0">
    <stop offset="0" stop-color="#d9e4ea"/>
    <stop offset="0.4" stop-color="#ffffff"/>
    <stop offset="0.7" stop-color="#9fb3bf"/>
    <stop offset="1" stop-color="#6f8793"/>
  </linearGradient>
</defs>
<g stroke="#4f6572" stroke-width="2">
  <path d="M 14 160 L 14 30 Q 14 10, 30 10 L 34 10" fill="none" stroke="url(#chrome)" stroke-width="9"/>
  <path d="M 50 160 L 50 30 Q 50 10, 34 10" fill="none" stroke="url(#chrome)" stroke-width="9"/>
  <rect x="10" y="48" width="44" height="8" rx="4" fill="url(#chrome)"/>
  <rect x="10" y="78" width="44" height="8" rx="4" fill="url(#chrome)"/>
  <rect x="10" y="108" width="44" height="8" rx="4" fill="url(#chrome)"/>
  <rect x="10" y="138" width="44" height="8" rx="4" fill="url(#chrome)"/>
</g>
</svg>"""


def ring_svg():
    return """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 192 48">
<defs>
  <radialGradient id="shadow" cx="0.5" cy="0.5" r="0.5">
    <stop offset="0" stop-color="#0d3a6e" stop-opacity="0.55"/>
    <stop offset="0.75" stop-color="#0d3a6e" stop-opacity="0.35"/>
    <stop offset="1" stop-color="#0d3a6e" stop-opacity="0"/>
  </radialGradient>
</defs>
<ellipse cx="96" cy="24" rx="92" ry="20" fill="url(#shadow)"/>
<ellipse cx="96" cy="22" rx="74" ry="12" fill="none" stroke="#ffffff" stroke-opacity="0.85" stroke-width="5"/>
<ellipse cx="96" cy="22" rx="84" ry="16" fill="none" stroke="#dff3ff" stroke-opacity="0.5" stroke-width="3"/>
</svg>"""


def sheet(frames, cell_w, cell_h, path):
    image = Image.new("RGBA", (cell_w * len(frames), cell_h), (0, 0, 0, 0))
    for index, frame in enumerate(frames):
        image.paste(frame, (index * cell_w, 0))
    image.save(path)
    print(path, image.size)


def main():
    os.makedirs(OUT, exist_ok=True)
    dolphins = [render(dolphin_svg(angle), 256, 128) for angle in (-14, -4, 8, 16)]
    sheet(dolphins, 256, 128, os.path.join(OUT, "stage3-dolphin-4-v1.png"))
    splashes = [render(splash_svg(stage), 128, 128) for stage in range(4)]
    sheet(splashes, 128, 128, os.path.join(OUT, "stage3-splash-4-v1.png"))
    render(water_tile_svg(), 256, 192).save(os.path.join(OUT, "stage3-water-tile-v1.png"))
    render(surface_svg(), 256, 40).save(os.path.join(OUT, "stage3-water-surface-v1.png"))
    render(ladder_svg(), 64, 160).save(os.path.join(OUT, "stage3-pool-ladder-v1.png"))
    render(ring_svg(), 192, 48).save(os.path.join(OUT, "stage3-drum-ring-v1.png"))
    print("done")


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""Render the big-top interior behind Event 2's tightrope (the board leaves
it black).  Writes assets/stage2-backdrop-v1.png, a 960x480 panel that tiles
horizontally: dark canvas ribs, two soft spotlights and a string of bulbs.

Run from the repository root:  python3 tools/render_stage2_backdrop.py
"""
import io
import os
import sys

import cairosvg
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def svg():
    ribs = ""
    for i in range(0, 961, 60):
        shade = "#2a1230" if (i // 60) % 2 else "#1d0f2a"
        ribs += f'<polygon points="{i},0 {i + 60},0 {i + 30 + 25},480 {i + 30 - 25},480" fill="{shade}" opacity="0.9"/>'
    bulbs = ""
    for i in range(0, 961, 48):
        y = 60 + 18 * ((i // 48) % 2)
        bulbs += f'<circle cx="{i + 24}" cy="{y}" r="4" fill="#ffd36b"/><circle cx="{i + 24}" cy="{y}" r="9" fill="#ffd36b" opacity="0.18"/>'
    return f"""<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 960 480">
<defs>
  <linearGradient id="sky" x1="0" y1="0" x2="0" y2="1">
    <stop offset="0" stop-color="#0b0617"/>
    <stop offset="1" stop-color="#1a1033"/>
  </linearGradient>
  <linearGradient id="fade" x1="0" y1="0" x2="0" y2="1">
    <stop offset="0" stop-color="#000" stop-opacity="0"/>
    <stop offset="1" stop-color="#000" stop-opacity="0.45"/>
  </linearGradient>
  <radialGradient id="spot" cx="0.5" cy="0" r="0.9">
    <stop offset="0" stop-color="#fff2c4" stop-opacity="0.32"/>
    <stop offset="0.6" stop-color="#fff2c4" stop-opacity="0.06"/>
    <stop offset="1" stop-color="#fff2c4" stop-opacity="0"/>
  </radialGradient>
</defs>
<rect width="960" height="480" fill="url(#sky)"/>
<g opacity="0.55">{ribs}</g>
<path d="M 0 52 Q 240 30, 480 52 T 960 52" fill="none" stroke="#5a3a1a" stroke-width="6"/>
<path d="M 0 52 Q 240 30, 480 52 T 960 52" fill="none" stroke="#d9a35b" stroke-width="2"/>
{bulbs}
<polygon points="300,0 180,480 420,480" fill="url(#spot)"/>
<polygon points="780,0 660,480 900,480" fill="url(#spot)"/>
<rect y="380" width="960" height="100" fill="url(#fade)"/>
</svg>"""


def main():
    png = cairosvg.svg2png(bytestring=svg().encode("utf-8"), output_width=960, output_height=480)
    out = os.path.join(ROOT, "assets", "stage2-backdrop-v1.png")
    Image.open(io.BytesIO(png)).convert("RGBA").save(out)
    print(out)


if __name__ == "__main__":
    sys.exit(main())

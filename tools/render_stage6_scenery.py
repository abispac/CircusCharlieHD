#!/usr/bin/env python3
"""Vector scenery for Event 6 (the trapeze), drawn to the cabinet's shapes:
  stage6-tub-v1.png       the circus tub the board calls a trampoline (42x18 board px)
  stage6-plank-v1.png     the goal platform's plank (40x4)
  stage6-lattice-v1.png   one bay of the goal tower's lattice, tiles vertically (30x16)
  stage6-truss-v1.png     one bay of the overhead truss, tiles horizontally (16x6)
Run from the repository root:  python3 tools/render_stage6_scenery.py
"""
import io, os, sys
import cairosvg
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "assets")


def render(svg, w, h, name):
    png = cairosvg.svg2png(bytestring=svg.encode("utf-8"), output_width=w, output_height=h)
    Image.open(io.BytesIO(png)).convert("RGBA").save(os.path.join(OUT, name))
    print(name, w, h)


TUB = """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 420 180">
<defs>
  <linearGradient id="base" x1="0" y1="0" x2="1" y2="0">
    <stop offset="0" stop-color="#c9a6e8"/><stop offset="0.5" stop-color="#f3dcff"/><stop offset="1" stop-color="#b48fd6"/>
  </linearGradient>
  <linearGradient id="red" x1="0" y1="0" x2="1" y2="0">
    <stop offset="0" stop-color="#b3101a"/><stop offset="0.5" stop-color="#ff3b3b"/><stop offset="1" stop-color="#b3101a"/>
  </linearGradient>
  <linearGradient id="yellow" x1="0" y1="0" x2="1" y2="0">
    <stop offset="0" stop-color="#d9a400"/><stop offset="0.5" stop-color="#ffe44d"/><stop offset="1" stop-color="#d9a400"/>
  </linearGradient>
</defs>
<!-- base skirt -->
<path d="M 20 150 Q 210 200 400 150 L 390 120 Q 210 160 30 120 Z" fill="url(#base)" stroke="#5a3a7a" stroke-width="3"/>
<!-- yellow band with blue triangles -->
<path d="M 30 120 Q 210 160 390 120 L 384 78 Q 210 110 36 78 Z" fill="url(#yellow)" stroke="#7a5a00" stroke-width="3"/>
<g fill="#1a3fd6">
  <polygon points="70,86 100,86 85,128"/><polygon points="130,90 160,90 145,134"/>
  <polygon points="195,92 225,92 210,136"/><polygon points="260,90 290,90 275,134"/>
  <polygon points="320,86 350,86 335,128"/>
</g>
<!-- red band -->
<path d="M 36 78 Q 210 110 384 78 L 380 56 Q 210 86 40 56 Z" fill="url(#red)" stroke="#5a0a10" stroke-width="3"/>
<!-- top rim -->
<ellipse cx="210" cy="52" rx="172" ry="30" fill="#bfe9ff" stroke="#2b7fc0" stroke-width="4"/>
<ellipse cx="210" cy="50" rx="150" ry="20" fill="#ffffff" opacity="0.9"/>
</svg>"""

PLANK = """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 400 40">
<rect x="0" y="10" width="400" height="30" fill="#8f98a8"/>
<rect x="0" y="10" width="400" height="8" fill="#b9c2d0"/>
<rect x="0" y="0" width="400" height="12" fill="#39c651"/>
<rect x="0" y="0" width="400" height="4" fill="#8df59a"/>
<rect x="0" y="34" width="400" height="6" fill="#5b6373"/>
</svg>"""

LATTICE = """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 300 160">
<rect width="300" height="160" fill="#000000"/>
<g stroke="#d9c6ec" stroke-width="14" fill="none">
  <line x1="12" y1="0" x2="12" y2="160"/><line x1="138" y1="0" x2="138" y2="160"/>
  <line x1="162" y1="0" x2="162" y2="160"/><line x1="288" y1="0" x2="288" y2="160"/>
</g>
<g stroke="#9c8ab0" stroke-width="8" fill="none">
  <line x1="12" y1="0" x2="138" y2="160"/><line x1="138" y1="0" x2="12" y2="160"/>
  <line x1="162" y1="0" x2="288" y2="160"/><line x1="288" y1="0" x2="162" y2="160"/>
  <line x1="0" y1="80" x2="300" y2="80"/>
</g>
</svg>"""

# The overhead truss, as the cabinet draws it: a light rail over a
# lavender lattice of diagonal struts, one 8-column period, six rows.
TRUSS_ROWS = [
    "LLLLLLLL",
    "g..g..gg",
    "....ggg.",
    "...ggg..",
    ".ggg..g.",
    "gggggggg",
]
TRUSS_COLOURS = {"L": (226, 196, 255, 255), "g": (160, 158, 190, 255),
                 ".": (8, 8, 14, 255)}


def render_truss(name, periods=2, scale=24):
    from PIL import ImageFilter
    w = 8 * periods
    h = len(TRUSS_ROWS)
    small = Image.new("RGBA", (w, h))
    for y, row in enumerate(TRUSS_ROWS):
        for x in range(w):
            small.putpixel((x, y), TRUSS_COLOURS[row[x % 8]])
    big = small.resize((w * scale, h * scale), Image.NEAREST)
    # Soften the pixel steps a touch and shade the struts.
    big = big.filter(ImageFilter.GaussianBlur(scale * 0.1))
    px = big.load()
    for y in range(big.height):
        shade = 1.0 - 0.12 * (y / big.height)
        for x in range(big.width):
            r, g, b, a = px[x, y]
            px[x, y] = (int(r * shade), int(g * shade), int(b * shade), a)
    big.save(os.path.join(OUT, name))
    print(name, big.size)


def main():
    render(TUB, 420, 180, "stage6-tub-v1.png")
    render(PLANK, 400, 40, "stage6-plank-v1.png")
    render(LATTICE, 300, 160, "stage6-lattice-v1.png")
    render_truss("stage6-truss-v2.png")


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""Cut an AI-generated (or any) character video into a sprite sheet.

    python3 tools/video_to_sheet.py CLIP.mp4 OUT-sheet.png --frames 9 \
        [--start 0.4 --end 2.1] [--crop x,y,w,h] [--contact contact.png]

Takes `--frames` evenly spaced stills between --start and --end seconds (the
whole clip by default), lays them side by side on one transparent sheet and
writes a numbered contact sheet to look at.  Pick the window that holds one
clean cycle first with `--contact` alone (every 0.1 s is dumped), then cut.
The result goes through tools/import_sprites.py like any other sheet
(`--chroma` when the clip was generated on green, otherwise `--key`).
"""
import argparse
import os
import subprocess
import sys
import tempfile

from PIL import Image, ImageDraw


def probe(path):
    out = subprocess.run(["ffprobe", "-v", "error", "-select_streams", "v:0",
                          "-show_entries", "stream=width,height,r_frame_rate,duration",
                          "-of", "csv=p=0", path], capture_output=True, text=True, check=True).stdout
    width, height, rate, duration = out.strip().split(",")[:4]
    num, den = rate.split("/")
    return int(width), int(height), float(num) / float(den), float(duration)


def grab(path, seconds, out_png, crop=None):
    filters = []
    if crop:
        x, y, w, h = crop
        filters.append(f"crop={w}:{h}:{x}:{y}")
    cmd = ["ffmpeg", "-v", "error", "-y", "-ss", f"{seconds:.3f}", "-i", path, "-frames:v", "1"]
    if filters:
        cmd += ["-vf", ",".join(filters)]
    cmd.append(out_png)
    subprocess.run(cmd, check=True)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("clip")
    ap.add_argument("out", nargs="?")
    ap.add_argument("--frames", type=int, default=9)
    ap.add_argument("--start", type=float, default=0.0)
    ap.add_argument("--end", type=float)
    ap.add_argument("--crop", help="x,y,w,h in source pixels")
    ap.add_argument("--contact", help="write a numbered contact sheet of the sampled frames")
    ap.add_argument("--every", type=float, default=0.1,
                    help="with --contact and no out: sample every N seconds")
    args = ap.parse_args()

    width, height, fps, duration = probe(args.clip)
    end = args.end if args.end is not None else duration
    crop = tuple(int(v) for v in args.crop.split(",")) if args.crop else None
    print(f"{args.clip}: {width}x{height} @ {fps:.2f} fps, {duration:.2f} s")

    if args.out:
        times = [args.start + (end - args.start) * i / args.frames for i in range(args.frames)]
    else:
        times = []
        t = args.start
        while t < end - 1e-6:
            times.append(t)
            t += args.every

    frames = []
    with tempfile.TemporaryDirectory() as tmp:
        for i, t in enumerate(times):
            png = os.path.join(tmp, f"f{i:03d}.png")
            grab(args.clip, t, png, crop)
            frames.append(Image.open(png).convert("RGBA"))
    fw, fh = frames[0].size

    if args.out:
        gap = 8
        sheet = Image.new("RGBA", ((fw + gap) * len(frames), fh), (0, 0, 0, 0))
        for i, f in enumerate(frames):
            sheet.paste(f, (i * (fw + gap), 0))
        sheet.save(args.out)
        print(f"wrote {args.out}: {len(frames)} frames of {fw}x{fh} at "
              + ", ".join(f"{t:.2f}s" for t in times))

    if args.contact:
        cols = min(len(frames), 8)
        rows = (len(frames) + cols - 1) // cols
        scale = min(1.0, 240.0 / fh)
        tw, th = int(fw * scale), int(fh * scale)
        contact = Image.new("RGB", (cols * tw, rows * (th + 16)), (60, 60, 60))
        draw = ImageDraw.Draw(contact)
        for i, (f, t) in enumerate(zip(frames, times)):
            x, y = (i % cols) * tw, (i // cols) * (th + 16)
            contact.paste(f.resize((tw, th), Image.LANCZOS), (x, y + 16))
            draw.text((x + 4, y + 2), f"{i}  {t:.2f}s", fill=(255, 255, 0))
        contact.save(args.contact)
        print(f"wrote {args.contact}")


if __name__ == "__main__":
    sys.exit(main())

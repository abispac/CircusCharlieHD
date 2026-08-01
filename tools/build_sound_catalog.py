#!/usr/bin/env python3
"""Build a private browser-based catalog for a Circus Charlie sound sweep."""

from __future__ import annotations

import argparse
import html
import math
import sys
import wave
from array import array
from pathlib import Path


KNOWN_LABELS = {
    0x13: "Event 1 music",
    0x14: "Event 2 music",
    0x15: "Trombonanza music",
    0x16: "Aerial-bar music",
    0x17: "Miss music",
    0x1D: "Event-selection music",
    0x41: "Credit / extra Charlie",
    0x47: "Jump",
    0x49: "Money bag",
    0x4F: "Miss effect component",
    0x50: "Fire-pot coin launch / collection",
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--clips", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--minimum-rms", type=int, default=30)
    return parser.parse_args()


def rms_for(path: Path) -> int:
    with wave.open(str(path), "rb") as source:
        if source.getsampwidth() != 2:
            raise SystemExit(f"Expected 16-bit PCM audio: {path}")
        samples = array("h", source.readframes(source.getnframes()))
        if sys.byteorder != "little":
            samples.byteswap()
        if not samples:
            return 0
        return round(math.sqrt(sum(sample * sample for sample in samples) /
                               len(samples)))


def main() -> None:
    args = parse_args()
    clips = []
    for path in sorted(args.clips.glob("sound-0x*.wav")):
        sound_id = int(path.stem.removeprefix("sound-0x"), 16)
        rms = rms_for(path)
        if rms > args.minimum_rms:
            clips.append((sound_id, path, rms))

    cards = []
    for sound_id, path, rms in clips:
        label = KNOWN_LABELS.get(sound_id, "Unidentified effect")
        source = html.escape(path.name, quote=True)
        cards.append(
            f"""
            <article>
              <h2>0x{sound_id:02X}</h2>
              <p>{html.escape(label)}</p>
              <audio controls preload="none" src="{source}"></audio>
              <small>RMS {rms}</small>
            </article>
            """
        )

    document = f"""<!doctype html>
<html lang="en">
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Circus Charlie private sound catalog</title>
<style>
body {{ margin: 0; background: #07131f; color: #f7f2e9;
  font-family: -apple-system, BlinkMacSystemFont, sans-serif; }}
main {{ max-width: 980px; margin: auto; padding: 36px 24px 64px; }}
h1 {{ color: #ffd55b; }}
.notice {{ color: #c8d7e5; line-height: 1.5; max-width: 760px; }}
.grid {{ display: grid; grid-template-columns: repeat(auto-fit, minmax(270px, 1fr));
  gap: 16px; margin-top: 28px; }}
article {{ background: #10263a; border: 1px solid #294963; border-radius: 14px;
  padding: 18px; box-shadow: 0 8px 24px #0005; }}
h2 {{ margin: 0; color: #ff6159; }}
p {{ min-height: 1.3em; }} audio {{ width: 100%; }}
small {{ display: block; color: #8fa9bc; margin-top: 10px; }}
</style>
<main>
  <h1>Circus Charlie sound-command catalog</h1>
  <p class="notice">These are private MAME renders of the original sound
  board. Only commands that produced audible output are shown. Listen for the
  money-bag, hidden-coin, and extra-Charlie effects and note their hexadecimal
  IDs. Command 0x41 is only the credit/coin-insert sound.</p>
  <section class="grid">{''.join(cards)}</section>
</main>
</html>
"""
    args.output.write_text(document, encoding="utf-8")
    print(f"Wrote {len(clips)} audible commands to {args.output}")


if __name__ == "__main__":
    main()

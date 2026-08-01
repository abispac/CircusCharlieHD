#!/usr/bin/env python3
"""Split a private Circus Charlie MAME sound-ID sweep into labeled WAVs."""

from __future__ import annotations

import argparse
import csv
import wave
from pathlib import Path


BOARD_REFRESH = 6_144_000.0 / (384.0 * 264.0)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--wav", type=Path, required=True)
    parser.add_argument("--log", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    with args.log.open(newline="") as stream:
        commands = list(csv.DictReader(stream))
    if not commands:
        raise SystemExit("The sound-command log is empty")

    with wave.open(str(args.wav), "rb") as source:
        parameters = source.getparams()
        if parameters.comptype != "NONE":
            raise SystemExit("Only uncompressed PCM WAV input is supported")
        audio = source.readframes(parameters.nframes)

    bytes_per_frame = parameters.sampwidth * parameters.nchannels
    trigger_frames = [int(row["trigger_frame"]) for row in commands]
    default_hold = (
        trigger_frames[-1] - trigger_frames[-2]
        if len(trigger_frames) > 1
        else round(BOARD_REFRESH * 3.0)
    )
    args.out.mkdir(parents=True, exist_ok=True)

    for index, command in enumerate(commands):
        start_board_frame = trigger_frames[index]
        end_board_frame = (
            trigger_frames[index + 1]
            if index + 1 < len(trigger_frames)
            else start_board_frame + default_hold
        )
        start_sample = round(
            start_board_frame * parameters.framerate / BOARD_REFRESH
        )
        end_sample = round(
            end_board_frame * parameters.framerate / BOARD_REFRESH
        )
        start_byte = start_sample * bytes_per_frame
        end_byte = min(len(audio), end_sample * bytes_per_frame)
        sound_id = int(command["sound_id_decimal"])
        destination = args.out / f"sound-0x{sound_id:02x}.wav"
        with wave.open(str(destination), "wb") as output:
            output.setparams(parameters)
            output.writeframes(audio[start_byte:end_byte])

    print(f"Wrote {len(commands)} labeled clips to {args.out}")


if __name__ == "__main__":
    main()

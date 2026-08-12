#!/usr/bin/env python3
"""Check that a manual Circus Charlie Level 1 capture is complete and usable."""

from __future__ import annotations

import argparse
import csv
from collections import Counter
from pathlib import Path


REQUIRED = ("extra-charlie", "hidden-coin", "goal-finish", "score-bonus")


def rows(path: Path) -> list[dict[str, str]]:
    with path.open(newline="") as handle:
        return list(csv.DictReader(handle))


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("prefix", type=Path, help="capture prefix used by the Lua tool")
    args = parser.parse_args()
    prefix = args.prefix

    expected = {
        "state": Path(f"{prefix}-state.csv"),
        "sprites": Path(f"{prefix}-sprites.csv"),
        "objects": Path(f"{prefix}-objects.csv"),
        "markers": Path(f"{prefix}-markers.csv"),
        "marker memory": Path(f"{prefix}-marker-memory.csv"),
        "automatic": Path(f"{prefix}-automatic.csv"),
        "complete": Path(f"{prefix}-complete.txt"),
    }
    missing = [label for label, path in expected.items() if not path.is_file()]
    if missing:
        print("INCOMPLETE: missing " + ", ".join(missing))
        return 1

    marker_rows = rows(expected["markers"])
    counts = Counter(row["label"] for row in marker_rows)
    missing_labels = [label for label in REQUIRED if not counts[label]]
    screenshots = [Path(row["screenshot"]) for row in marker_rows]
    missing_shots = [path for path in screenshots if not path.is_file()]
    state_count = sum(1 for _ in expected["state"].open()) - 1
    sprite_count = sum(1 for _ in expected["sprites"].open()) - 1
    object_count = sum(1 for _ in expected["objects"].open()) - 1

    print(expected["complete"].read_text().strip())
    print("marker_burst_rows=" + str(len(marker_rows)))
    print("marker_labels=" + ",".join(f"{name}:{counts[name]}" for name in sorted(counts)))
    print(f"state_rows={state_count}")
    print(f"sprite_rows={sprite_count}")
    print(f"object_rows={object_count}")

    if missing_labels:
        print("INCOMPLETE: no marker for " + ", ".join(missing_labels))
        return 1
    if missing_shots:
        print(f"INCOMPLETE: {len(missing_shots)} marker screenshots are missing")
        return 1
    print("CAPTURE READY FOR RECONSTRUCTION")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

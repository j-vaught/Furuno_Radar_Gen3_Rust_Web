"""Build the E1 repeatability contract from one analyzer session summary."""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
from collections import OrderedDict
from pathlib import Path
from typing import Any


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()

    raw = args.source.read_bytes()
    groups: OrderedDict[str, dict[str, Any]] = OrderedDict()
    with args.source.open(encoding="utf-8", newline="") as stream:
        for row in csv.DictReader(stream):
            label = row["phase_id"]
            group = groups.setdefault(
                label,
                {"label": label, "rotation_index": [], "zero_fraction": []},
            )
            group["rotation_index"].append(len(group["rotation_index"]))
            group["zero_fraction"].append(float(row["zero_fraction"]))

    rows = [
        {
            "label": group["label"],
            "short_label": group["label"].removeprefix("reference_").replace("__", " "),
            "rotation_index": group["rotation_index"],
            "zero_fraction": group["zero_fraction"],
        }
        for group in groups.values()
    ]
    args.out.write_text(
        json.dumps(
            {
                "status": "ready" if rows else "preview",
                "source": str(args.source),
                "source_sha256": hashlib.sha256(raw).hexdigest(),
                "experiment": "E1_fixed_scene_repeats",
                "metric": "decoded zero fraction",
                "groups": rows,
            },
            indent=2,
        )
        + "\n",
        encoding="utf-8",
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

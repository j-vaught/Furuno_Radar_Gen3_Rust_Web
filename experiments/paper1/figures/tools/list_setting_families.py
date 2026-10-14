"""List campaign control families for compact per-family Typst figures."""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path


def slug(value: str) -> str:
    result = re.sub(r"[^a-z0-9]+", "_", value.lower()).strip("_")
    return result or "family"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--data", type=Path, required=True)
    args = parser.parse_args()
    payload = json.loads(args.data.read_text(encoding="utf-8"))
    if payload.get("status") != "ready":
        return 0
    families: list[str] = []
    for condition in payload.get("conditions", []):
        family = str(condition.get("label", "")).strip()
        if family and family != "reference" and family not in families:
            families.append(family)
    for family in families:
        print(f"{family}\t{slug(family)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

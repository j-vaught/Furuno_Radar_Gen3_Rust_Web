"""Build the EF paired-condition figure contract from analyzer exports.

The pairing rule is explicit. EF0 is the observed reference within each fixed
processing context, and EF1 or EF2 is its recorded companion. No phase is
renamed or treated as a reference when the source records do not establish
that relationship.
"""

from __future__ import annotations

import argparse
import csv
import json
import re
from collections import defaultdict
from pathlib import Path
from typing import Any

PHASE_RE = re.compile(r"^doppler_(?P<index>[012])_(?P<context>.+)$")


def read_rows(path: Path) -> list[dict[str, str]]:
    with path.open(encoding="utf-8", newline="") as stream:
        return list(csv.DictReader(stream))


def number(row: dict[str, str], key: str) -> float:
    value = row.get(key, "")
    if value in ("", "None", "null"):
        raise ValueError(f"missing {key} in {row.get('phase_id', row.get('csv_path', 'row'))}")
    return float(value)


def phase_key(phase_id: str) -> tuple[str, str]:
    match = PHASE_RE.fullmatch(phase_id)
    if match is None:
        raise ValueError(f"unsupported EF phase identifier: {phase_id}")
    return f"EF{match.group('index')}", match.group("context")


def build_contract(
    session_rows: list[dict[str, str]], metric_rows: list[dict[str, str]]
) -> dict[str, Any]:
    metrics_by_sha = {row.get("csv_sha256", ""): row for row in metric_rows}
    records: defaultdict[str, list[dict[str, Any]]] = defaultdict(list)
    phase_counts: defaultdict[str, int] = defaultdict(int)
    for row in session_rows:
        phase_id = row.get("phase_id", "")
        label, context = phase_key(phase_id)
        metric = metrics_by_sha.get(row.get("csv_sha256", ""))
        if metric is None:
            raise ValueError(f"no rotation-metrics row for {phase_id} {row.get('csv_sha256')}")
        records[context].append(
            {
                "label": label,
                "phase_id": phase_id,
                "rotation_index": phase_counts[phase_id],
                "zero_fraction_pct": number(row, "zero_fraction") * 100.0,
                "observed_code_count": number(metric, "observed_code_count"),
                "unique_angle_count": number(row, "unique_angle_count"),
                "csv_sha256": row.get("csv_sha256", ""),
            }
        )
        phase_counts[phase_id] += 1

    pairs = []
    for context, rows in sorted(records.items()):
        by_label: defaultdict[str, list[dict[str, Any]]] = defaultdict(list)
        for row in rows:
            by_label[row["label"]].append(row)
        if set(by_label) != {"EF0", "EF1"} and set(by_label) != {"EF0", "EF2"}:
            raise ValueError(
                f"expected one EF0 pair with EF1 or EF2 in {context}, got {sorted(by_label)}"
            )
        variant = "EF1" if "EF1" in by_label else "EF2"
        pairs.append(
            {
                "context": context,
                "reference_label": "EF0",
                "condition_label": variant,
                "reference": by_label["EF0"],
                "condition": by_label[variant],
            }
        )
    if len(pairs) != 2:
        raise ValueError(f"expected two completed EF pairs, found {len(pairs)}")
    return {
        "status": "ready",
        "reference_rule": "ef0-per-fixed-context",
        "contexts": [pair["context"] for pair in pairs],
        "pairs": pairs,
        "record_count": len(session_rows),
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--session-summary", type=Path, required=True)
    parser.add_argument("--rotation-metrics", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    contract = build_contract(read_rows(args.session_summary), read_rows(args.rotation_metrics))
    contract["source_session_summary"] = str(args.session_summary)
    contract["source_rotation_metrics"] = str(args.rotation_metrics)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(contract, indent=2) + "\n", encoding="utf-8")
    print(f"wrote {args.out} with {contract['record_count']} records")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

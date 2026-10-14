"""Adapt analyzer CSV outputs to the JSON contracts imported by Typst.

The adapter is deliberately numeric-only. Missing or empty analyzer files
produce a preview state; no values are invented for an empirical figure.
"""

from __future__ import annotations

import argparse
import csv
import json
from datetime import datetime
from pathlib import Path
from typing import Any


def read_rows(path: Path) -> list[dict[str, str]]:
    if not path.exists():
        return []
    with path.open(encoding="utf-8", newline="") as stream:
        return list(csv.DictReader(stream))


def number(value: str | None) -> float | None:
    if value in (None, "", "None", "null"):
        return None
    return float(value)


def integer(value: str | None) -> int | None:
    item = number(value)
    return int(item) if item is not None else None


def boolean(value: str | None) -> bool | None:
    if value in (None, "", "None", "null"):
        return None
    return value.strip().lower() in {"1", "true", "yes"}


def elapsed_ms(ack: str | None, readback: str | None) -> float | None:
    if not ack or not readback:
        return None
    try:
        start = datetime.fromisoformat(ack.replace("Z", "+00:00"))
        stop = datetime.fromisoformat(readback.replace("Z", "+00:00"))
    except ValueError:
        return None
    return (stop - start).total_seconds() * 1000.0


def write(path: Path, value: dict[str, Any]) -> None:
    path.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")


def first_value(row: dict[str, str], names: tuple[str, ...]) -> float | None:
    for name in names:
        value = number(row.get(name))
        if value is not None:
            return value
    return None


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--figure-data", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--bit-audit", type=Path)
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)

    session = read_rows(args.figure_data / "session_summary.csv")
    if session:
        rows = sorted(session, key=lambda row: integer(row.get("rotation_index")) or 0)
        write(
            args.out / "repeatability.json",
            {
                "status": "ready",
                "source": str(args.figure_data / "session_summary.csv"),
                "runs": [integer(row.get("rotation_index")) for row in rows],
                "metric": [number(row.get("zero_fraction")) for row in rows],
            },
        )
    else:
        write(
            args.out / "repeatability.json",
            {
                "status": "preview",
                "source": str(args.figure_data / "session_summary.csv"),
                "runs": [],
                "metric": [],
            },
        )

    responses = read_rows(args.figure_data / "setting_response.csv")
    if not responses:
        responses = read_rows(args.figure_data / "setting_effects.csv")
    if responses:
        grouped: dict[tuple[str, str], list[dict[str, Any]]] = {}
        for row in responses:
            family = row.get("control_family", "unknown")
            condition = row.get("condition", row.get("condition_id", "unknown"))
            grouped.setdefault((family, condition), []).append(row)
        conditions = []
        for (family, condition), rows in sorted(grouped.items()):
            rows.sort(key=lambda row: integer(row.get("rotation_index")) or 0)
            conditions.append(
                {
                    "label": f"{family}/{condition}",
                    "rotation_index": [integer(row.get("rotation_index")) for row in rows],
                    "zero_fraction": [
                        first_value(row, ("decoded_zero_fraction", "zero_fraction"))
                        for row in rows
                    ],
                    "mean_code": [
                        first_value(row, ("decoded_mean_code", "mean_code")) for row in rows
                    ],
                    "max_code_fraction": [
                        first_value(
                            row,
                            ("decoded_max_code_fraction", "max_code_fraction"),
                        )
                        for row in rows
                    ],
                    "unique_angle_count": [
                        first_value(row, ("unique_angle_count", "angular_support"))
                        for row in rows
                    ],
                }
            )
        write(
            args.out / "setting_response.json",
            {
                "status": "ready",
                "source": str(args.figure_data / "setting_response.csv"),
                "conditions": conditions,
            },
        )
    else:
        write(
            args.out / "setting_response.json",
            {
                "status": "preview",
                "source": str(args.figure_data / "setting_response.csv"),
                "conditions": [],
            },
        )

    transitions = read_rows(args.figure_data / "setting_transition.csv")
    coverage_rows = read_rows(args.figure_data / "settings_coverage.csv")
    if transitions:
        rows = list(enumerate(transitions))
        latency_rows = [
            (index, elapsed_ms(row.get("ack_time_utc"), row.get("readback_time_utc")))
            for index, row in rows
        ]
        latency_rows = [(index, value) for index, value in latency_rows if value is not None]
        write(
            args.out / "setting_transition.json",
            {
                "status": "ready",
                "source": str(args.figure_data / "setting_transition.csv"),
                "event_index": [index for index, _ in rows],
                "settle_s": [number(row.get("settle_s")) for _, row in rows],
                "latency_event_index": [index for index, _ in latency_rows],
                "latency_ms": [value for _, value in latency_rows],
                "readback_match": [
                    1 if boolean(row.get("readback_match")) else 0 for _, row in rows
                ],
                "setting_labels": [row.get("setting", "") for _, row in rows],
            },
        )
    else:
        write(
            args.out / "setting_transition.json",
            {
                "status": "preview",
                "source": str(args.figure_data / "setting_transition.csv"),
                "event_index": [],
                "settle_s": [],
                "latency_event_index": [],
                "latency_ms": [],
                "readback_match": [],
                "setting_labels": [],
            },
        )

    # A coverage export can arrive before the timing export. Preserve it as a
    # quantitative result while leaving the latency figure in preview state.
    if transitions or coverage_rows:
        outcome_rows = coverage_rows or transitions
        coverage_indexed = list(enumerate(outcome_rows))
        categories = ["accepted", "rejected", "clamped", "deferred", "restored"]
        counts = {category: 0 for category in categories}
        for row in outcome_rows:
            outcome = (row.get("outcome") or row.get("coverage_outcome") or "").strip().lower()
            if not outcome:
                match = boolean(row.get("readback_match"))
                outcome = "accepted" if match else "rejected"
            if outcome in counts:
                counts[outcome] += 1
        write(
            args.out / "settings_coverage.json",
            {
                "status": "ready",
                "source": str(args.figure_data / ("settings_coverage.csv" if coverage_rows else "setting_transition.csv")),
                "event_index": [index for index, _ in coverage_indexed],
                "readback_match": [
                    1 if boolean(row.get("readback_match")) else 0
                    for _, row in coverage_indexed
                ],
                "setting_labels": [row.get("setting", "") for _, row in coverage_indexed],
                "requested_labels": [row.get("requested_value", "") for _, row in coverage_indexed],
                "readback_labels": [row.get("readback_value", "") for _, row in coverage_indexed],
                "categories": categories,
                "category_counts": [counts[category] for category in categories],
            },
        )
    else:
        write(
            args.out / "settings_coverage.json",
            {
                "status": "preview",
                "source": str(args.figure_data / "settings_coverage.csv"),
                "event_index": [],
                "readback_match": [],
                "setting_labels": [],
                "requested_labels": [],
                "readback_labels": [],
                "categories": ["accepted", "rejected", "clamped", "deferred", "restored"],
                "category_counts": [0, 0, 0, 0, 0],
            },
        )

    residuals = read_rows(args.figure_data / "geometry_residuals.csv")
    usable = [
        row
        for row in residuals
        if number(row.get("expected_range_m")) is not None
        and number(row.get("range_error_m")) is not None
        and number(row.get("expected_bearing_deg")) is not None
        and number(row.get("bearing_error_deg")) is not None
    ]
    if usable:
        write(
            args.out / "residuals.json",
            {
                "status": "ready",
                "source": str(args.figure_data / "geometry_residuals.csv"),
                "range_m": [number(row["expected_range_m"]) for row in usable],
                "range_error_m": [number(row["range_error_m"]) for row in usable],
                "bearing_deg": [number(row["expected_bearing_deg"]) for row in usable],
                "bearing_error_deg": [number(row["bearing_error_deg"]) for row in usable],
            },
        )
    else:
        write(
            args.out / "residuals.json",
            {
                "status": "preview",
                "source": str(args.figure_data / "geometry_residuals.csv"),
                "range_m": [],
                "range_error_m": [],
                "bearing_deg": [],
                "bearing_error_deg": [],
            },
        )

    if args.bit_audit and args.bit_audit.exists():
        audit = json.loads(args.bit_audit.read_text(encoding="utf-8"))
        bitplanes = audit.get("bitplanes", {})
        pre = bitplanes.get("pre_shift_codes", {})
        upper = bitplanes.get("upper6_code_counts", {})
        low2 = bitplanes.get("low2_class_counts", {})
        alignment = audit.get("csv_alignment", [])
        write(
            args.out / "bit_planes.json",
            {
                "status": "ready",
                "source": str(args.bit_audit),
                "pre_shift_values": [int(key) for key in pre],
                "pre_shift_counts": [int(value) for value in pre.values()],
                "pre_shift_counts_plot": [max(1, int(value)) for value in pre.values()],
                "upper6_values": [int(key) for key in upper],
                "upper6_counts": [int(value) for value in upper.values()],
                "upper6_counts_plot": [max(1, int(value)) for value in upper.values()],
                "decoded_low2_values": [0, 1, 2, 3],
                "decoded_low2_counts": [int(low2.get(str(index), 0)) for index in range(4)],
                "alignment_index": list(range(len(alignment))),
                "alignment_fraction": [float(row["match_fraction"]) for row in alignment],
                "decoded_value_count": bitplanes.get("decoded_value_count"),
                "low2_nonzero_fraction": bitplanes.get("low2_nonzero_fraction"),
            },
        )
    else:
        write(
            args.out / "bit_planes.json",
            {
                "status": "preview",
                "source": "awaiting doppler bit audit JSON",
                "pre_shift_values": [],
                "pre_shift_counts": [],
                "pre_shift_counts_plot": [],
                "upper6_values": [],
                "upper6_counts": [],
                "upper6_counts_plot": [],
                "decoded_low2_values": [],
                "decoded_low2_counts": [],
                "alignment_index": [],
                "alignment_fraction": [],
                "decoded_value_count": None,
                "low2_nonzero_fraction": None,
            },
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

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


def timestamp_seconds(value: str) -> float | None:
    if value.startswith("unix_ms:"):
        try:
            return float(value.removeprefix("unix_ms:")) / 1000.0
        except ValueError:
            return None
    try:
        return datetime.fromisoformat(value.replace("Z", "+00:00")).timestamp()
    except ValueError:
        return None


def elapsed_ms(ack: str | None, readback: str | None) -> float | None:
    if not ack or not readback:
        return None
    start = timestamp_seconds(ack)
    stop = timestamp_seconds(readback)
    if start is None or stop is None:
        return None
    return (stop - start) * 1000.0


def write(path: Path, value: dict[str, Any]) -> None:
    path.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")


def first_value(row: dict[str, str], names: tuple[str, ...]) -> float | None:
    for name in names:
        value = number(row.get(name))
        if value is not None:
            return value
    return None


OUTCOME_CATEGORIES = (
    "accepted",
    "rejected",
    "clamped",
    "deferred",
    "restored",
    "superseded",
)


def classify_outcome(row: dict[str, str]) -> str | None:
    """Map only documented schema outcomes to coverage categories.

    A missing or unknown outcome is retained in ``unclassified_count`` rather
    than inferred from a readback match.  In particular, ``superseded`` means
    that a source record was replaced by a corrected run and is not deferred.
    """
    raw = (row.get("outcome") or row.get("coverage_outcome") or "").strip().lower()
    return {
        "supported": "accepted",
        "accepted": "accepted",
        "unchanged": "clamped",
        "clamped": "clamped",
        "unsupported": "rejected",
        "rejected": "rejected",
        "deferred": "deferred",
        "restored": "restored",
        "superseded": "superseded",
    }.get(raw)


def recovered_characterization(path: Path) -> list[dict[str, str]]:
    if path.suffix.lower() == ".csv":
        return read_rows(path)
    payload = json.loads(path.read_text(encoding="utf-8"))
    control = payload.get("control_characterization", payload)
    outcomes = control.get("outcomes", []) if isinstance(control, dict) else []
    rows = []
    for item in outcomes:
        command = item.get("command", {})
        readback = item.get("readback", {})
        requested = item.get("requested")
        actual = item.get("actual")
        rows.append(
            {
                "session_id": payload.get("session_id", "unknown"),
                "context": item.get("context", ""),
                "setting": item.get("setting", ""),
                "requested_value": str(requested) if requested is not None else "",
                "previous_value": str(item.get("previous", "")),
                "actual_value": str(actual) if actual is not None else "",
                "outcome": item.get("outcome", ""),
                "command_time_utc": command.get("command_time_utc", ""),
                "ack_time_utc": command.get("ack_time_utc", ""),
                "readback_time_utc": readback.get("timestamp_utc", ""),
                "transition_id": "",
                "readback_match": str(requested == actual) if actual is not None else "",
            }
        )
    return rows


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--figure-data", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--bit-audit", type=Path)
    parser.add_argument("--campaign-data", type=Path)
    parser.add_argument("--characterization", type=Path)
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
    if not responses and args.campaign_data:
        responses = read_rows(args.campaign_data / "setting_response.csv")
    if not responses:
        responses = read_rows(args.figure_data / "setting_effects.csv")
    if responses:
        # Campaign aggregation writes one row per phase. Group by control
        # family so a large campaign does not create a legend entry for every
        # individual setting value.
        grouped: dict[str, list[dict[str, Any]]] = {}
        for row in responses:
            family = row.get("control_family", row.get("setting", "unknown"))
            grouped.setdefault(family, []).append(row)
        conditions = []
        for family, rows in sorted(grouped.items()):
            rows.sort(
                key=lambda row: integer(row.get("phase_index", row.get("rotation_index"))) or 0
            )
            conditions.append(
                {
                    "label": family,
                    "rotation_index": [
                        integer(row.get("phase_index", row.get("rotation_index"))) for row in rows
                    ],
                    "condition_labels": [
                        row.get("condition", row.get("phase_id", "")) for row in rows
                    ],
                    "zero_fraction": [
                        first_value(row, ("decoded_zero_fraction", "zero_fraction")) for row in rows
                    ],
                    "exported_code_support": [
                        first_value(
                            row,
                            (
                                "exported_code_support",
                                "code_support",
                                "observed_code_count",
                            ),
                        )
                        for row in rows
                    ],
                    "angular_support": [
                        first_value(
                            row,
                            ("angular_support", "unique_angle_count"),
                        )
                        for row in rows
                    ],
                    # Retain legacy aliases for downstream readers while the
                    # figure uses representation support explicitly.
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
                        first_value(row, ("unique_angle_count", "angular_support")) for row in rows
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
    characterization_rows = read_rows(args.figure_data / "control_characterization.csv")
    characterization_source = args.figure_data / "control_characterization.csv"
    if args.characterization and args.characterization.exists():
        characterization_rows = recovered_characterization(args.characterization)
        characterization_source = args.characterization
    elif not characterization_rows and args.campaign_data:
        characterization_rows = read_rows(args.campaign_data / "control_characterization.csv")
        characterization_source = args.campaign_data / "control_characterization.csv"
    if not coverage_rows and not transitions and characterization_rows:
        coverage_rows = characterization_rows
    if transitions:
        rows = list(enumerate(transitions))
        latency_rows = [
            (index, elapsed_ms(row.get("ack_time_utc"), row.get("readback_time_utc")))
            for index, row in rows
        ]
        latency_rows = [(index, value) for index, value in latency_rows if value is not None]
        match_rows = [(index, boolean(row.get("readback_match"))) for index, row in rows]
        match_rows = [(index, value) for index, value in match_rows if value is not None]
        write(
            args.out / "setting_transition.json",
            {
                "status": "ready",
                "source": str(args.figure_data / "setting_transition.csv"),
                "event_index": [index for index, _ in rows],
                "settle_s": [number(row.get("settle_s")) for _, row in rows],
                "latency_event_index": [index for index, _ in latency_rows],
                "latency_ms": [value for _, value in latency_rows],
                "match_event_index": [index for index, _ in match_rows],
                "readback_match": [1 if value else 0 for _, value in match_rows],
                "setting_labels": [
                    row.get("setting") or row.get("control_family") or row.get("field") or "control"
                    for _, row in rows
                ],
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
        categories = list(OUTCOME_CATEGORIES)
        counts = {category: 0 for category in categories}
        unclassified_count = 0
        for row in outcome_rows:
            outcome = classify_outcome(row)
            if outcome in counts:
                counts[outcome] += 1
            else:
                unclassified_count += 1
        write(
            args.out / "settings_coverage.json",
            {
                "status": "ready",
                "source": str(
                    args.figure_data / "settings_coverage.csv"
                    if read_rows(args.figure_data / "settings_coverage.csv")
                    else characterization_source
                    if characterization_rows
                    else args.figure_data / "setting_transition.csv"
                ),
                "event_index": [index for index, _ in coverage_indexed],
                "readback_match": [
                    1
                    if boolean(row.get("readback_match"))
                    or (
                        row.get("actual_value") not in (None, "")
                        and row.get("actual_value") == row.get("requested_value")
                    )
                    else 0
                    for _, row in coverage_indexed
                ],
                "setting_labels": [row.get("setting", "") for _, row in coverage_indexed],
                "requested_labels": [row.get("requested_value", "") for _, row in coverage_indexed],
                "readback_labels": [
                    row.get("readback_value", row.get("actual_value", ""))
                    for _, row in coverage_indexed
                ],
                "categories": categories,
                "category_counts": [counts[category] for category in categories],
                "unclassified_count": unclassified_count,
                "classification_semantics": {
                    "accepted": "schema outcome supported or accepted",
                    "rejected": "schema outcome unsupported or rejected",
                    "clamped": "schema outcome unchanged or clamped",
                    "deferred": "schema outcome deferred",
                    "restored": "schema outcome restored",
                    "superseded": "source record replaced by a corrected run",
                    "unclassified": (
                        "missing or unknown schema outcome; excluded from category bars"
                    ),
                },
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
                "categories": list(OUTCOME_CATEGORIES),
                "category_counts": [0] * len(OUTCOME_CATEGORIES),
                "unclassified_count": 0,
                "classification_semantics": {
                    "accepted": "schema outcome supported or accepted",
                    "rejected": "schema outcome unsupported or rejected",
                    "clamped": "schema outcome unchanged or clamped",
                    "deferred": "schema outcome deferred",
                    "restored": "schema outcome restored",
                    "superseded": "source record replaced by a corrected run",
                    "unclassified": (
                        "missing or unknown schema outcome; excluded from category bars"
                    ),
                },
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

"""Aggregate completed campaign phases into the setting-response figure contract.

The input is the campaign session manifest plus analyzer rotation metrics. This
tool only summarizes exported representation metrics. It does not infer range,
bearing, velocity, noise, or saturation units.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
from collections import defaultdict
from collections.abc import Iterable
from pathlib import Path
from typing import Any


def read_rows(path: Path) -> list[dict[str, str]]:
    if not path.exists():
        return []
    with path.open(encoding="utf-8", newline="") as stream:
        return list(csv.DictReader(stream))


def as_number(value: Any) -> float | None:
    if value in (None, "", "null", "None"):
        return None
    try:
        return float(value)
    except (TypeError, ValueError):
        return None


def as_int(value: Any) -> int | None:
    number = as_number(value)
    return int(number) if number is not None else None


def canonical(value: Any) -> str:
    return str(value).replace("\\", "/").strip()


def path_keys(value: Any) -> set[str]:
    path = canonical(value)
    if not path:
        return set()
    return {path, path.rsplit("/", 1)[-1]}


def phase_id(phase: dict[str, Any], index: int) -> str:
    return str(phase.get("phase_id", phase.get("id", f"phase_{index}")))


def phase_files(phase: dict[str, Any]) -> set[str]:
    values: list[Any] = []
    for key in ("rotation_files", "rotations", "csv_files", "files"):
        item = phase.get(key, [])
        values.extend(item if isinstance(item, list) else [item])
    keys: set[str] = set()
    for item in values:
        if isinstance(item, dict):
            for key in ("csv_path", "path", "file", "filename"):
                keys.update(path_keys(item.get(key)))
        else:
            keys.update(path_keys(item))
    return keys


def phase_settings(phase: dict[str, Any]) -> tuple[dict[str, Any], str]:
    for key in ("settings", "commanded_settings"):
        if isinstance(phase.get(key), dict):
            settings = phase[key]
            return settings, key
    for key in ("device_readback", "readback", "observed_settings"):
        value = phase.get(key)
        if isinstance(value, dict):
            settings = value.get("settings", value)
            if isinstance(settings, dict):
                return settings, f"{key}.settings"
    return {}, "missing"


def settings_hash(settings: dict[str, Any]) -> str | None:
    if not settings:
        return None
    encoded = json.dumps(settings, sort_keys=True, separators=(",", ":"))
    return hashlib.sha256(encoded.encode("utf-8")).hexdigest()


def metric_value(row: dict[str, str], names: Iterable[str]) -> float | None:
    for name in names:
        value = as_number(row.get(name))
        if value is not None:
            return value
    return None


def mean(values: Iterable[float | None]) -> float | None:
    values = [value for value in values if value is not None]
    return sum(values) / len(values) if values else None


def load_metrics(path: Path) -> list[dict[str, Any]]:
    if path.suffix.lower() == ".json":
        payload = json.loads(path.read_text(encoding="utf-8"))
        if isinstance(payload, dict):
            rows = payload.get("rotations", payload.get("metrics", []))
            return rows if isinstance(rows, list) else []
        return payload if isinstance(payload, list) else []
    return read_rows(path)


def related_reference(
    index: int,
    phases: list[dict[str, Any]],
    settings_by_index: dict[int, dict[str, Any]],
) -> tuple[int | None, str]:
    references = []
    for phase_index, phase in enumerate(phases):
        label = phase_id(phase, phase_index).lower()
        if any(token in label for token in ("reference", "baseline", "control")):
            references.append(phase_index)
    if not references:
        return None, "none"
    if index in references:
        return index, "self"
    previous = [item for item in references if item < index]
    following = [item for item in references if item > index]
    if previous:
        return previous[-1], "previous"
    return following[0], "next"


def family_for(phase: dict[str, Any], reference: dict[str, Any] | None) -> str:
    explicit = phase.get("control_family", phase.get("setting_family"))
    if explicit:
        return str(explicit)
    settings, _ = phase_settings(phase)
    ref_settings = reference or {}
    keys = [key for key in settings if settings.get(key) != ref_settings.get(key)]
    if len(keys) == 1:
        return keys[0]
    label = phase.get("phase_id", phase.get("id", "condition"))
    return str(label).split("_")[0]


def write_csv(path: Path, rows: list[dict[str, Any]], fields: list[str]) -> None:
    with path.open("w", encoding="utf-8", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields, extrasaction="ignore")
        writer.writeheader()
        writer.writerows(rows)


def recover_characterization(source: Path, out: Path) -> int:
    payload = json.loads(source.read_text(encoding="utf-8"))
    outcomes = payload.get("control_characterization", payload).get("outcomes", [])
    rows = []
    for item in outcomes:
        command = item.get("command", {})
        readback = item.get("readback", {})
        requested = item.get("requested")
        actual = item.get("actual")
        rows.append(
            {
                "session_id": payload.get("session_id", "unknown"),
                "context": item.get("context"),
                "setting": item.get("setting"),
                "requested_value": requested,
                "previous_value": item.get("previous"),
                "actual_value": actual,
                "outcome": item.get("outcome"),
                "command_time_utc": command.get("command_time_utc"),
                "ack_time_utc": command.get("ack_time_utc"),
                "readback_time_utc": readback.get("timestamp_utc"),
                "transition_id": (readback.get("settings_readback") or {}).get("transition_id"),
                "readback_match": requested == actual if actual is not None else None,
            }
        )
    fields = [
        "session_id",
        "context",
        "setting",
        "requested_value",
        "previous_value",
        "actual_value",
        "outcome",
        "command_time_utc",
        "ack_time_utc",
        "readback_time_utc",
        "transition_id",
        "readback_match",
    ]
    write_csv(out, rows, fields)
    return len(rows)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--manifest", type=Path)
    parser.add_argument("--rotation-metrics", type=Path)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--recover-characterization", type=Path)
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)

    if args.recover_characterization:
        count = recover_characterization(
            args.recover_characterization, args.out / "control_characterization.csv"
        )
        print(f"recovered control characterization rows: {count}")

    if not args.manifest or not args.rotation_metrics:
        fields = [
            "campaign_id",
            "phase_index",
            "phase_id",
            "control_family",
            "condition",
            "role",
            "reference_phase_id",
            "reference_relation",
            "settings_hash",
            "reference_settings_hash",
            "settings_delta_keys",
            "rotation_count",
            "decoded_zero_fraction",
            "exported_code_support",
            "angular_support",
            "decoded_mean_code",
            "max_code_fraction",
            "source_rotation_metrics",
            "source_manifest",
        ]
        write_csv(args.out / "setting_response.csv", [], fields)
        return 0

    manifest = json.loads(args.manifest.read_text(encoding="utf-8"))
    phases = manifest.get("phases", []) if isinstance(manifest, dict) else []
    metrics = load_metrics(args.rotation_metrics)
    by_phase: dict[int, list[dict[str, Any]]] = defaultdict(list)
    by_key: dict[str, list[dict[str, Any]]] = defaultdict(list)
    for row in metrics:
        raw_phase = row.get("phase_index", row.get("phase_number"))
        phase_number = as_int(raw_phase)
        if phase_number is not None:
            by_phase[phase_number].append(row)
        for key in ("csv_path", "path", "file", "filename"):
            for path_key in path_keys(row.get(key)):
                by_key[path_key].append(row)

    phase_rows = []
    settings_by_index: dict[int, dict[str, Any]] = {
        index: phase_settings(phase)[0] for index, phase in enumerate(phases)
    }
    for index, phase in enumerate(phases):
        settings, source = phase_settings(phase)
        rows = list(by_phase.get(index, []))
        for key in phase_files(phase):
            rows.extend(by_key.get(key, []))
        seen: set[int] = set()
        unique_rows: list[dict[str, Any]] = []
        for row in rows:
            row_id = id(row)
            if row_id not in seen:
                seen.add(row_id)
                unique_rows.append(row)
        rows = unique_rows
        if not rows and phase.get("rotation_metrics"):
            for item in phase["rotation_metrics"]:
                rows.append(item if isinstance(item, dict) else {})
        if not rows:
            continue
        reference_index, relation = related_reference(index, phases, settings_by_index)
        reference_settings = (
            settings_by_index.get(reference_index, {}) if reference_index is not None else {}
        )
        delta_keys = sorted(
            key
            for key in set(settings) | set(reference_settings)
            if settings.get(key) != reference_settings.get(key)
        )
        reference_phase = phases[reference_index] if reference_index is not None else {}
        family = "reference" if not delta_keys else family_for(phase, reference_settings)
        phase_rows.append(
            {
                "campaign_id": manifest.get(
                    "session_id", manifest.get("experiment_id", "campaign")
                ),
                "phase_index": index,
                "phase_id": phase_id(phase, index),
                "control_family": family,
                "condition": phase_id(phase, index),
                "role": "reference" if not delta_keys else "variant",
                "reference_phase_id": phase_id(reference_phase, reference_index)
                if reference_index is not None
                else "",
                "reference_relation": relation,
                "settings_hash": phase.get("settings_hash") or settings_hash(settings) or "",
                "reference_settings_hash": settings_hash(reference_settings) or "",
                "settings_delta_keys": ";".join(delta_keys),
                "rotation_count": len(rows),
                "decoded_zero_fraction": mean(
                    metric_value(row, ("zero_fraction", "decoded_zero_fraction")) for row in rows
                ),
                "exported_code_support": mean(
                    metric_value(
                        row, ("observed_code_count", "exported_code_support", "code_support")
                    )
                    for row in rows
                ),
                "angular_support": mean(
                    metric_value(row, ("unique_angle_count", "angular_support")) for row in rows
                ),
                "decoded_mean_code": mean(
                    metric_value(row, ("mean_code", "decoded_mean_code")) for row in rows
                ),
                "max_code_fraction": mean(
                    metric_value(row, ("max_code_fraction", "decoded_max_code_fraction"))
                    for row in rows
                ),
                "source_rotation_metrics": str(args.rotation_metrics),
                "source_manifest": str(args.manifest),
            }
        )

    fields = (
        list(phase_rows[0])
        if phase_rows
        else [
            "campaign_id",
            "phase_index",
            "phase_id",
            "control_family",
            "condition",
            "role",
            "reference_phase_id",
            "reference_relation",
            "settings_hash",
            "reference_settings_hash",
            "settings_delta_keys",
            "rotation_count",
            "decoded_zero_fraction",
            "exported_code_support",
            "angular_support",
            "decoded_mean_code",
            "max_code_fraction",
            "source_rotation_metrics",
            "source_manifest",
        ]
    )
    write_csv(args.out / "setting_response.csv", phase_rows, fields)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

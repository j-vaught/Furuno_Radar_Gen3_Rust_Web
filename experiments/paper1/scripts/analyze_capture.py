#!/usr/bin/env python3
"""Analyze Paper 1 CSV rotations and raw UDP JSONL without producing plots."""

from __future__ import annotations

import argparse
import base64
import csv
import hashlib
import json
import math
import statistics
from collections.abc import Iterable
from pathlib import Path
from typing import Any


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as fp:
        for block in iter(lambda: fp.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def csv_metrics(path: Path, source_path: str | None = None) -> dict[str, Any]:
    rows = 0
    bins: list[int] = []
    angles: list[int] = []
    values: list[int] = []
    malformed = 0
    with path.open(encoding="utf-8", errors="replace", newline="") as fp:
        reader = csv.reader((line for line in fp if not line.startswith("#")), delimiter="\t")
        header = next(reader, [])
        for row in reader:
            if len(row) < 6:
                malformed += 1
                continue
            try:
                angle = int(row[4])
                echoes = [int(value) for value in row[5].split(",") if value != ""]
            except ValueError:
                malformed += 1
                continue
            rows += 1
            angles.append(angle)
            bins.append(len(echoes))
            values.extend(echoes)
    unique_angles = len(set(angles))
    zero_fraction = (values.count(0) / len(values)) if values else math.nan
    decreases = sum(
        1 for previous, current in zip(angles, angles[1:], strict=False) if current < previous
    )
    return {
        "csv_path": source_path or str(path),
        "analysis_csv_path": str(path),
        "csv_sha256": sha256_file(path),
        "header": header,
        "row_count": rows,
        "malformed_rows": malformed,
        "echo_bins_min": min(bins) if bins else None,
        "echo_bins_max": max(bins) if bins else None,
        "echo_bins_mean": statistics.fmean(bins) if bins else None,
        "unique_angle_count": unique_angles,
        "duplicate_angle_count": rows - unique_angles,
        "angle_decrease_count": decreases,
        "angle_min": min(angles) if angles else None,
        "angle_max": max(angles) if angles else None,
        "echo_count": len(values),
        "zero_fraction": zero_fraction,
        "code_min": min(values) if values else None,
        "code_max": max(values) if values else None,
        "observed_code_count": len(set(values)),
    }


def decode_sweep_count(
    data: bytes, encoding: int, sweep_len: int, previous: list[int]
) -> tuple[list[int], int]:
    values: list[int] = []
    used = 0
    strength = 0
    while len(values) < sweep_len and used < len(data):
        byte = data[used]
        if encoding == 0:
            count = min(len(data) - used, sweep_len - len(values))
            values.extend(data[used : used + count])
            used += count
            break
        if encoding == 1:
            if byte & 1 == 0:
                strength = byte
                values.append(strength)
            else:
                count = byte >> 1 or 128
                values.extend([strength] * min(count, sweep_len - len(values)))
        elif encoding == 2:
            if byte & 1 == 0:
                values.append(byte)
            else:
                count = byte >> 1 or 128
                start = len(values)
                values.extend(
                    (previous[i] if i < len(previous) else 0)
                    for i in range(start, start + min(count, sweep_len - len(values)))
                )
        elif encoding == 3:
            if byte & 3 == 0:
                strength = byte
                values.append(strength)
            else:
                count = byte >> 2 or 64
                if byte & 1:
                    values.extend([strength] * min(count, sweep_len - len(values)))
                else:
                    start = len(values)
                    for i in range(start, start + min(count, sweep_len - len(values))):
                        strength = previous[i] if i < len(previous) else 0
                        values.append(strength)
        else:
            return [], len(data)
        used += 1
    return values, (used + 3) & ~3


def replay_raw(path: Path) -> dict[str, Any]:
    packets = malformed = truncated = decoded = 0
    spoke_start_truncation = sweep_payload_truncation = 0
    encodings = {0: 0, 1: 0, 2: 0, 3: 0}
    angles: list[int] = []
    previous: list[int] = []
    timestamps: list[int] = []
    with path.open(encoding="utf-8", errors="replace") as fp:
        for line in fp:
            if not line.strip():
                continue
            try:
                record = json.loads(line)
                encoded = record.get("payload_b64") or record.get("payload")
                payload = base64.b64decode(encoded, validate=True)
                timestamp = int(
                    record.get(
                        "receive_unix_ns", record.get("received_unix_ns", record.get("unix_ns", 0))
                    )
                )
                timestamps.append(timestamp)
            except (ValueError, TypeError, json.JSONDecodeError):
                malformed += 1
                continue
            packets += 1
            if len(payload) < 20:
                malformed += 1
                continue
            sweep_count = payload[9] >> 1
            sweep_len = ((payload[11] & 7) << 8) | payload[10]
            encoding = (payload[11] & 0x18) >> 3
            if encoding not in encodings:
                malformed += 1
                continue
            encodings[encoding] += 1
            cursor = 16
            for sweep_index in range(sweep_count):
                if cursor + 4 > len(payload):
                    truncated += 1
                    spoke_start_truncation += 1
                    break
                angle = int.from_bytes(payload[cursor : cursor + 2], "little")
                angles.append(angle)
                cursor += 4
                # The Rust decoder treats the first sweep of encoding 2 as an
                # encoding-1 seed and copies later sweeps from the previous one.
                effective_encoding = 1 if encoding == 2 and sweep_index == 0 else encoding
                values, used = decode_sweep_count(
                    payload[cursor:], effective_encoding, sweep_len, previous
                )
                if len(values) != sweep_len:
                    truncated += 1
                    sweep_payload_truncation += 1
                if used > len(payload) - cursor:
                    truncated += 1
                    sweep_payload_truncation += 1
                    break
                cursor += used
                previous = values
                decoded += 1
    monotonic = all(a <= b for a, b in zip(timestamps, timestamps[1:], strict=False))
    return {
        "raw_path": str(path),
        "raw_sha256": sha256_file(path),
        "packet_count": packets,
        "malformed_packet_count": malformed,
        "encoding_0_packets": encodings[0],
        "encoding_1_packets": encodings[1],
        "encoding_2_packets": encodings[2],
        "encoding_3_packets": encodings[3],
        "decoded_spoke_count": decoded,
        "truncated_sweep_count": truncated,
        "spoke_start_truncation_count": spoke_start_truncation,
        "sweep_payload_truncation_count": sweep_payload_truncation,
        "angle_min": min(angles) if angles else None,
        "angle_max": max(angles) if angles else None,
        "timestamp_monotonic": monotonic,
        "timestamp_count": len(timestamps),
    }


def write_csv(
    path: Path, rows: Iterable[dict[str, Any]], fieldnames: list[str] | None = None
) -> None:
    rows = list(rows)
    if not rows:
        path.write_text(",".join(fieldnames or []) + "\n", encoding="utf-8")
        return
    keys = fieldnames or list(rows[0])
    with path.open("w", newline="", encoding="utf-8") as fp:
        writer = csv.DictWriter(fp, fieldnames=keys)
        writer.writeheader()
        writer.writerows(rows)


def parse_path_maps(values: list[str]) -> list[tuple[Path, Path]]:
    mappings: list[tuple[Path, Path]] = []
    for value in values:
        source, separator, destination = value.partition("=")
        if not separator or not source or not destination:
            raise SystemExit(f"--path-map must use FROM=TO, got {value!r}")
        mappings.append((Path(source).expanduser(), Path(destination).expanduser()))
    return sorted(mappings, key=lambda item: len(str(item[0])), reverse=True)


def map_path(path_text: str, mappings: list[tuple[Path, Path]]) -> Path:
    original = Path(path_text)
    for source, destination in mappings:
        try:
            relative = original.relative_to(source)
        except ValueError:
            continue
        return destination / relative
    return original


def manifest_rotation_paths(
    manifest: dict[str, Any], path_maps: list[tuple[Path, Path]]
) -> list[tuple[str, Path, str]]:
    """Return manifest-declared rotations as (source, local path, phase id)."""
    declared: list[tuple[str, Path, str]] = []
    missing_declarations: list[str] = []
    for phase in manifest.get("phases", []):
        phase_id = str(phase.get("phase_id", phase.get("id", "unknown")))
        for source in phase.get("rotation_files", []):
            source_text = str(source)
            local = map_path(source_text, path_maps)
            declared.append((source_text, local, phase_id))
            if not local.is_file():
                missing_declarations.append(f"{source_text} -> {local}")
    if missing_declarations:
        raise SystemExit(
            "manifest-declared rotation files are missing; provide --path-map FROM=TO "
            "for files mirrored to another host:\n" + "\n".join(missing_declarations)
        )
    if not declared:
        raise SystemExit("session manifest declares no rotation files; refusing zero-file analysis")
    return declared


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--session", type=Path, required=True)
    parser.add_argument("--raw", type=Path)
    parser.add_argument("--rotations", type=Path)
    parser.add_argument(
        "--path-map",
        action="append",
        default=[],
        metavar="FROM=TO",
        help="Map a recorded absolute path prefix to its local mirror; may be repeated",
    )
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument(
        "--geometry-measurements", type=Path, help="Optional measured target detections JSON"
    )
    args = parser.parse_args()
    manifest_path = args.session / "session_manifest.json"
    manifest = (
        json.loads(manifest_path.read_text(encoding="utf-8")) if manifest_path.exists() else {}
    )
    path_maps = parse_path_maps(args.path_map)
    args.out.mkdir(parents=True, exist_ok=True)
    raw_path = args.raw or args.session / "raw_udp_payloads.jsonl"
    if args.rotations:
        if args.rotations.is_file():
            rotation_specs = [(str(args.rotations), args.rotations, "unknown")]
        else:
            rotation_specs = [
                (str(path), path, "unknown") for path in sorted(args.rotations.glob("**/*.csv"))
            ]
            if not rotation_specs:
                raise SystemExit(f"--rotations contains no CSV files: {args.rotations}")
    else:
        rotation_specs = manifest_rotation_paths(manifest, path_maps)
    metrics = [csv_metrics(path, source) for source, path, _phase_id in rotation_specs]
    phase_by_file = {str(path): phase_id for _source, path, phase_id in rotation_specs}
    replay = (
        replay_raw(raw_path) if raw_path.exists() else {"raw_path": str(raw_path), "missing": True}
    )
    summary = {
        "schema_version": "paper1-analysis-1.0",
        "session": str(args.session),
        "manifest_status": manifest.get("status"),
        "rotation_count": len(metrics),
        "rotations": metrics,
        "raw_replay": replay,
    }
    (args.out / "rotation_metrics.json").write_text(
        json.dumps(summary, indent=2) + "\n", encoding="utf-8"
    )
    (args.out / "raw_replay_summary.json").write_text(
        json.dumps(replay, indent=2) + "\n", encoding="utf-8"
    )
    write_csv(args.out / "rotation_metrics.csv", metrics)
    write_csv(args.out / "raw_replay_summary.csv", [replay])
    figure_dir = args.out / "figure_data"
    figure_dir.mkdir(exist_ok=True)
    session_id = manifest.get("session_id", args.session.name)
    experiment_id = manifest.get("experiment_id", "unknown")
    figure_rows = []
    for index, item in enumerate(metrics):
        readback = {}
        for phase in manifest.get("phases", []):
            if phase.get("phase_id") == phase_by_file.get(item["analysis_csv_path"]):
                readback = phase.get("device_readback", {}) or {}
        figure_rows.append(
            {
                "session_id": session_id,
                "experiment_id": experiment_id,
                "phase_id": phase_by_file.get(item["analysis_csv_path"], "unknown"),
                "settings_hash": hashlib.sha256(
                    json.dumps(readback, sort_keys=True).encode()
                ).hexdigest(),
                "rotation_index": index,
                "csv_path": item["csv_path"],
                "analysis_csv_path": item["analysis_csv_path"],
                "csv_sha256": item["csv_sha256"],
                "raw_packet_count": replay.get("packet_count", 0),
                "spoke_count": item["row_count"],
                "unique_angle_count": item["unique_angle_count"],
                "duplicate_angle_count": item["duplicate_angle_count"],
                "echo_bins_min": item["echo_bins_min"],
                "echo_bins_max": item["echo_bins_max"],
                "zero_fraction": item["zero_fraction"],
                "code_min": item["code_min"],
                "code_max": item["code_max"],
                "replay_decoded_spokes": replay.get("decoded_spoke_count", 0),
            }
        )
    write_csv(figure_dir / "session_summary.csv", figure_rows)
    write_csv(figure_dir / "raw_replay_summary.csv", [replay])
    transition_fields = [
        "session_id",
        "phase_id",
        "setting",
        "requested_value",
        "ack_time_utc",
        "readback_time_utc",
        "readback_value",
        "settle_s",
        "first_stable_rotation",
        "readback_match",
    ]
    transition_rows: list[dict[str, Any]] = []
    for phase in manifest.get("phases", []):
        readback = phase.get("device_readback", {}) or {}
        values = readback.get("settings", readback) if isinstance(readback, dict) else {}
        matches = phase.get("readback_matches", {})
        for command in phase.get("command_events", []):
            setting = command.get("setting", "")
            transition_rows.append(
                {
                    "session_id": session_id,
                    "phase_id": phase.get("phase_id", "unknown"),
                    "setting": setting,
                    "requested_value": command.get("requested"),
                    "ack_time_utc": command.get("ack_time_utc"),
                    "readback_time_utc": readback.get("timestamp_utc"),
                    "readback_value": values.get(setting) if isinstance(values, dict) else None,
                    "settle_s": phase.get("settle_s"),
                    "first_stable_rotation": None,
                    "readback_match": matches.get(setting),
                }
            )
    write_csv(figure_dir / "setting_transition.csv", transition_rows, transition_fields)
    control_fields = [
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
    control_rows: list[dict[str, Any]] = []
    characterization = manifest.get("control_characterization", {})
    for item in characterization.get("outcomes", []):
        command = item.get("command", {})
        item_readback = item.get("readback", {})
        settings = item_readback.get("settings", {}) if isinstance(item_readback, dict) else {}
        requested = item.get("requested")
        actual = item.get("actual", settings.get(item.get("setting")))
        control_rows.append(
            {
                "session_id": session_id,
                "context": item.get("context"),
                "setting": item.get("setting"),
                "requested_value": requested,
                "previous_value": item.get("previous"),
                "actual_value": actual,
                "outcome": item.get("outcome"),
                "command_time_utc": command.get("command_time_utc"),
                "ack_time_utc": command.get("ack_time_utc"),
                "readback_time_utc": item_readback.get("timestamp_utc"),
                "transition_id": item_readback.get("settings_readback", {}).get("transition_id")
                if isinstance(item_readback.get("settings_readback"), dict)
                else None,
                "readback_match": (requested == actual if actual is not None else None),
            }
        )
    write_csv(figure_dir / "control_characterization.csv", control_rows, control_fields)
    (figure_dir / "control_characterization.json").write_text(
        json.dumps(
            {
                "schema_version": "paper1-control-characterization-1.0",
                "session_id": session_id,
                "experiment_id": experiment_id,
                "baseline": characterization.get("baseline"),
                "outcomes": characterization.get("outcomes", []),
                "restore_matches": characterization.get("restore_matches", {}),
                "restore_complete": characterization.get("restore_complete"),
            },
            indent=2,
        )
        + "\n",
        encoding="utf-8",
    )
    geometry_fields = [
        "session_id",
        "phase_id",
        "target_id",
        "expected_range_m",
        "measured_range_m",
        "range_error_m",
        "expected_bearing_deg",
        "measured_bearing_deg",
        "bearing_error_deg",
        "survey_uncertainty_m",
        "survey_uncertainty_bearing_deg",
        "measurement_method",
    ]
    geometry_rows: list[dict[str, Any]] = []
    if args.geometry_measurements and args.geometry_measurements.exists():
        measurements = json.loads(args.geometry_measurements.read_text(encoding="utf-8"))
        target_by_id = {
            target["target_id"]: target
            for target in manifest.get("target_manifest", {}).get("targets", [])
        }
        for item in measurements.get("measurements", []):
            target = target_by_id.get(item.get("target_id"))
            if not target:
                continue
            measured_range = item.get("measured_range_m")
            measured_bearing = item.get("measured_bearing_deg")
            geometry_rows.append(
                {
                    "session_id": session_id,
                    "phase_id": item.get("phase_id", "unknown"),
                    "target_id": item["target_id"],
                    "expected_range_m": target["range_m"],
                    "measured_range_m": measured_range,
                    "range_error_m": measured_range - target["range_m"]
                    if measured_range is not None
                    else None,
                    "expected_bearing_deg": target["bearing_deg"],
                    "measured_bearing_deg": measured_bearing,
                    "bearing_error_deg": measured_bearing - target["bearing_deg"]
                    if measured_bearing is not None
                    else None,
                    "survey_uncertainty_m": target["uncertainty_m"],
                    "survey_uncertainty_bearing_deg": target.get("uncertainty_bearing_deg"),
                    "measurement_method": target["measurement_method"],
                }
            )
    write_csv(figure_dir / "geometry_residuals.csv", geometry_rows, geometry_fields)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

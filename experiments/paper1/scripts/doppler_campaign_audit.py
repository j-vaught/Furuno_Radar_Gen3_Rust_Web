#!/usr/bin/env python3
"""Create a compact, phase-aware comparison of Doppler campaign captures.

The campaign runner stores one raw UDP JSONL stream for a session and records
phase boundaries in ``events.jsonl``.  This tool replays the complete stream
once per session, preserving encoding-3 previous-spoke state, then crops the
decoded spokes and packet metadata to each manifest phase by host receive
timestamp.  It never changes hardware state and does not claim that a Doppler
control mode provides velocity.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import importlib
import json
import sys
from collections import Counter
from collections.abc import Iterable
from pathlib import Path
from typing import Any

if __package__ in (None, ""):
    sys.path.insert(0, str(Path(__file__).resolve().parent))
_doppler_audit = importlib.import_module(
    "doppler_audit" if __package__ in (None, "") else "scripts.doppler_audit"
)
ReplayState = _doppler_audit.ReplayState
replay_checksum = _doppler_audit.replay_checksum
replay_raw = _doppler_audit.replay_raw
compare_csv = _doppler_audit.compare_csv

SCHEMA_VERSION = "paper1-doppler-campaign-audit-2"
REQUIRED_EF_MODES = (0, 1, 2)
# EF0 is the baseline for two separate matched contexts.  EF1 and EF2 are not
# required to share one fixed-settings digest with each other.
REQUIRED_PAIRS = ((0, 1), (0, 2))
# Retain the old name for callers that imported it from this script.
REQUIRED_MODES = REQUIRED_EF_MODES
REQUIRED_DURATION_S = 30.0

EF_MODE_LABELS = {0: "EF0", 1: "EF1", 2: "EF2"}
EF_MODE_PRESETS = {
    0: "restore prior processing flags",
    1: "automatic gain and sea processing; rain manual 0",
    2: "automatic gain, sea, and rain processing",
}


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as fp:
        for block in iter(lambda: fp.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def load_object(path: Path) -> dict[str, Any]:
    value = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(value, dict):
        raise ValueError(f"JSON object required: {path}")
    return value


def read_events(path: Path) -> list[dict[str, Any]]:
    events: list[dict[str, Any]] = []
    if not path.exists():
        return events
    with path.open(encoding="utf-8", errors="replace") as fp:
        for line in fp:
            try:
                value = json.loads(line)
            except json.JSONDecodeError:
                continue
            if isinstance(value, dict):
                events.append(value)
    return events


def phase_event_times(events: Iterable[dict[str, Any]]) -> dict[str, dict[str, int | None]]:
    windows: dict[str, dict[str, int | None]] = {}
    for event in events:
        phase_id = event.get("phase_id")
        name = event.get("event")
        timestamp = event.get("unix_ns")
        if not isinstance(phase_id, str) or not isinstance(timestamp, int):
            continue
        if name == "recording_window_start":
            windows.setdefault(phase_id, {})["start_unix_ns"] = timestamp
        elif name == "recording_window_end":
            windows.setdefault(phase_id, {})["end_unix_ns"] = timestamp
    return windows


def locate_session(path: Path) -> tuple[Path, Path, Path, dict[str, Any]]:
    session_dir = path if path.is_dir() else path.parent
    manifest_path = session_dir / "session_manifest.json"
    if not manifest_path.exists():
        raise FileNotFoundError(f"missing session_manifest.json in {session_dir}")
    raw_candidates = [
        session_dir / "raw_udp_payloads.jsonl",
        session_dir / "echo-udp-payloads.jsonl",
    ]
    raw_candidates.extend(
        candidate
        for candidate in sorted(session_dir.glob("*.jsonl"))
        if candidate.name != "events.jsonl"
    )
    raw_path = next((candidate for candidate in raw_candidates if candidate.exists()), None)
    if raw_path is None:
        raise FileNotFoundError(f"missing raw UDP JSONL in {session_dir}")
    return session_dir, manifest_path, raw_path, load_object(manifest_path)


def manifest_csv_paths(session_dir: Path, manifest: dict[str, Any]) -> list[Path]:
    """Resolve declared Mac recording CSVs onto the mirrored analysis host."""

    live_dir = session_dir.parents[1]
    paths: list[Path] = []
    phases = manifest.get("phases", [])
    if not isinstance(phases, list):
        return paths
    for phase in phases:
        if not isinstance(phase, dict):
            continue
        record_dir = phase.get("record_dir")
        if not isinstance(record_dir, str):
            continue
        recording_name = Path(record_dir).name
        output = phase.get("recording_output", {})
        declared = output.get("csv_files", []) if isinstance(output, dict) else []
        for item in declared:
            source = item.get("path") if isinstance(item, dict) else item
            if not isinstance(source, str):
                continue
            candidate = live_dir / "macos-recordings" / recording_name / "csv" / Path(source).name
            if candidate not in paths:
                paths.append(candidate)
    return paths


def settings_digest(settings: dict[str, Any]) -> str:
    fixed = {
        key: value
        for key, value in settings.items()
        if key not in {"doppler", "ef_processing_mode", "processing_mode"}
    }
    encoded = json.dumps(fixed, sort_keys=True, separators=(",", ":")).encode()
    return hashlib.sha256(encoded).hexdigest()[:16]


def counter_json(values: Counter[int]) -> dict[str, int]:
    return {str(key): count for key, count in sorted(values.items())}


def ef_processing_mode(settings: dict[str, Any]) -> int | None:
    for key in ("ef_processing_mode", "processing_mode", "doppler"):
        value = settings.get(key)
        if isinstance(value, int) and not isinstance(value, bool):
            return value
    return None


def phase_metrics(
    state: ReplayState,
    packet_rows: list[dict[str, Any]],
    phase: dict[str, Any],
    window: dict[str, int | None],
    session_id: str,
    raw_path: Path,
    manifest_path: Path,
) -> dict[str, Any]:
    requested = phase.get("requested_settings", phase.get("settings", {}))
    requested = requested if isinstance(requested, dict) else {}
    readback = phase.get("device_readback", {})
    readback = readback if isinstance(readback, dict) else {}
    actual = readback.get("settings", {})
    actual = actual if isinstance(actual, dict) else {}
    start = window.get("start_unix_ns")
    end = window.get("end_unix_ns")
    cropped = isinstance(start, int) and isinstance(end, int) and end > start
    if cropped:
        spokes = [spoke for spoke in state.spokes if start <= spoke.receive_unix_ns < end]
        packets = [
            row
            for row in packet_rows
            if isinstance(row.get("receive_unix_ns"), int) and start <= row["receive_unix_ns"] < end
        ]
    else:
        spokes = []
        packets = []
    values = [value for spoke in spokes for value in spoke.values]
    pre_shift = Counter(values)
    upper6 = Counter(value >> 2 for value in values)
    low2 = Counter(value & 3 for value in values)
    encodings = Counter(int(row["encoding"]) for row in packets if "encoding" in row)
    header_flags = Counter(int(row["flags_u16_be"]) for row in packets if "flags_u16_be" in row)
    header_configs = Counter(int(row["config_u16_be"]) for row in packets if "config_u16_be" in row)
    checksum = replay_checksum(spokes)
    raw_receive_times = [int(row["receive_unix_ns"]) for row in packets]
    raw_first_receive = min(raw_receive_times) if raw_receive_times else None
    raw_last_receive = max(raw_receive_times) if raw_receive_times else None
    raw_receive_span = (
        (raw_last_receive - raw_first_receive) / 1e9
        if raw_first_receive is not None and raw_last_receive is not None
        else None
    )
    requested_duration = phase.get("record_s", 0)
    try:
        requested_duration = float(requested_duration)
    except (TypeError, ValueError):
        requested_duration = 0.0
    measured_duration = (end - start) / 1e9 if cropped else None
    matches = phase.get("readback_matches", {})
    readback_verified = bool(
        readback.get("confirmed") is True
        and readback.get("fresh") is True
        and readback.get("verified") is True
        and isinstance(matches, dict)
        and all(value is True for value in matches.values())
    )
    actual_mode = ef_processing_mode(actual)
    requested_mode = ef_processing_mode(requested)
    mode = actual_mode if actual_mode is not None else requested_mode
    mode_label = EF_MODE_LABELS.get(mode)
    return {
        "session_id": session_id,
        "phase_id": phase.get("phase_id", phase.get("id", "unknown")),
        "raw_path": str(raw_path),
        "raw_sha256": sha256_file(raw_path),
        "manifest_path": str(manifest_path),
        "requested_doppler": requested_mode,
        "actual_doppler": actual_mode,
        "comparison_mode": mode,
        "requested_ef_processing_mode": requested_mode,
        "actual_ef_processing_mode": actual_mode,
        "ef_processing_mode": mode,
        "ef_processing_mode_label": mode_label,
        "ef_processing_preset": EF_MODE_PRESETS.get(mode),
        "requested_settings_json": json.dumps(requested, sort_keys=True, separators=(",", ":")),
        "actual_settings_json": json.dumps(actual, sort_keys=True, separators=(",", ":")),
        "fixed_settings_digest": settings_digest(actual or requested),
        "window_start_unix_ns": start,
        "window_end_unix_ns": end,
        "window_duration_s": measured_duration,
        "requested_record_s": requested_duration,
        "raw_first_receive_unix_ns": raw_first_receive,
        "raw_last_receive_unix_ns": raw_last_receive,
        "raw_receive_span_s": raw_receive_span,
        "raw_receive_coverage_adequate": bool(
            raw_receive_span is not None and raw_receive_span >= REQUIRED_DURATION_S
        ),
        "duration_adequate": bool(
            requested_duration >= REQUIRED_DURATION_S
            and measured_duration is not None
            and measured_duration >= REQUIRED_DURATION_S
        ),
        "raw_interval_cropped": cropped,
        "settings_readback_verified": readback_verified,
        "packet_count": len(packets),
        "declared_sweep_count": sum(int(row.get("sweep_count", 0)) for row in packets),
        "decoded_spoke_count": len(spokes),
        "complete_spoke_count": sum(spoke.complete for spoke in spokes),
        "encoding_packet_counts_json": json.dumps(counter_json(encodings), sort_keys=True),
        "header_flags_counts_json": json.dumps(counter_json(header_flags), sort_keys=True),
        "header_config_counts_json": json.dumps(counter_json(header_configs), sort_keys=True),
        "pre_shift_alphabet": ",".join(str(value) for value in sorted(pre_shift)),
        "upper6_alphabet": ",".join(str(value) for value in sorted(upper6)),
        "pre_shift_unique_count": len(pre_shift),
        "upper6_unique_count": len(upper6),
        "decoded_value_count": len(values),
        "low2_class_counts_json": json.dumps(counter_json(low2), sort_keys=True),
        "low2_nonzero_fraction": ((len(values) - low2[0]) / len(values) if values else None),
        "decoded_value_sum": checksum["decoded_value_sum"],
        "decoded_value_xor": checksum["decoded_value_xor"],
        "replay_checksum_fnv1a64": checksum["fnv1a64_angle_heading_raw_values"],
        "direct_velocity_field_found": False,
        "velocity_inference": "not_established_from_amplitude_spokes",
    }


def compare_modes(rows: list[dict[str, Any]]) -> list[dict[str, Any]]:
    groups: dict[str, dict[int, list[dict[str, Any]]]] = {}
    for row in rows:
        mode = row.get("comparison_mode")
        if not isinstance(mode, int):
            continue
        groups.setdefault(str(row["fixed_settings_digest"]), {}).setdefault(mode, []).append(row)
    comparisons: list[dict[str, Any]] = []
    for digest, modes in groups.items():
        for left, right in REQUIRED_PAIRS:
            if left not in modes or right not in modes:
                continue
            left_rows = modes[left]
            right_rows = modes[right]
            pair_ready = bool(
                all(row["duration_adequate"] for row in left_rows)
                and all(row["duration_adequate"] for row in right_rows)
                and all(row["settings_readback_verified"] for row in left_rows)
                and all(row["settings_readback_verified"] for row in right_rows)
            )
            comparisons.append(
                {
                    "fixed_settings_digest": digest,
                    "mode_left": left,
                    "mode_right": right,
                    "ef_processing_mode_left": left,
                    "ef_processing_mode_right": right,
                    "ef_processing_mode_label_left": EF_MODE_LABELS[left],
                    "ef_processing_mode_label_right": EF_MODE_LABELS[right],
                    "ef_processing_preset_left": EF_MODE_PRESETS[left],
                    "ef_processing_preset_right": EF_MODE_PRESETS[right],
                    "left_phase_count": len(left_rows),
                    "right_phase_count": len(right_rows),
                    "left_duration_adequate": all(row["duration_adequate"] for row in left_rows),
                    "right_duration_adequate": all(row["duration_adequate"] for row in right_rows),
                    "left_readback_verified": all(
                        row["settings_readback_verified"] for row in left_rows
                    ),
                    "right_readback_verified": all(
                        row["settings_readback_verified"] for row in right_rows
                    ),
                    "left_decoded_value_count": sum(
                        row["decoded_value_count"] for row in left_rows
                    ),
                    "right_decoded_value_count": sum(
                        row["decoded_value_count"] for row in right_rows
                    ),
                    "left_pre_shift_alphabets": sorted(
                        {row["pre_shift_alphabet"] for row in left_rows}
                    ),
                    "right_pre_shift_alphabets": sorted(
                        {row["pre_shift_alphabet"] for row in right_rows}
                    ),
                    "left_upper6_alphabets": sorted({row["upper6_alphabet"] for row in left_rows}),
                    "right_upper6_alphabets": sorted(
                        {row["upper6_alphabet"] for row in right_rows}
                    ),
                    "left_low2_nonzero_fractions": [
                        row["low2_nonzero_fraction"] for row in left_rows
                    ],
                    "right_low2_nonzero_fractions": [
                        row["low2_nonzero_fraction"] for row in right_rows
                    ],
                    "left_replay_checksums": [row["replay_checksum_fnv1a64"] for row in left_rows],
                    "right_replay_checksums": [
                        row["replay_checksum_fnv1a64"] for row in right_rows
                    ],
                    "pair_ready": pair_ready,
                    "comparison_status": "ready_descriptive" if pair_ready else "incomplete",
                    "velocity_conclusion": "not_established",
                }
            )
    return comparisons


def session_quality(
    state: ReplayState,
    spokes: list[Any],
    session_dir: Path,
    manifest: dict[str, Any],
    phase_rows: list[dict[str, Any]],
) -> dict[str, Any]:
    """Audit closed-session CSV completeness, windows, readbacks, and writer counters."""

    declared = manifest_csv_paths(session_dir, manifest)
    missing_paths = [str(path) for path in declared if not path.exists()]
    existing = [path for path in declared if path.exists()]
    alignment = compare_csv(existing, spokes)
    row_count = sum(int(row["row_count"]) for row in alignment)
    matched_row_count = sum(int(row["matched_row_count"]) for row in alignment)
    unmatched_row_count = sum(int(row["unmatched_row_count"]) for row in alignment)
    contiguous_count = sum(row["match_indices_contiguous"] is True for row in alignment)
    all_rows_match = (
        not missing_paths and len(alignment) == len(declared) and unmatched_row_count == 0
    )
    all_files_contiguous = all_rows_match and contiguous_count == len(declared) and bool(declared)
    raw_windows_adequate = all(row["raw_receive_coverage_adequate"] for row in phase_rows) and bool(
        phase_rows
    )
    readback_verified = all(row["settings_readback_verified"] for row in phase_rows) and bool(
        phase_rows
    )
    receiver = manifest.get("raw_receiver", {})
    receiver = receiver if isinstance(receiver, dict) else {}
    counter_fields = {
        key: receiver.get(key)
        for key in (
            "packets",
            "received_packets",
            "written_packets",
            "bytes",
            "received_bytes",
            "written_bytes",
            "pending",
            "queue_overflow",
            "write_errors",
            "peer_mismatch",
        )
        if key in receiver
    }
    extended_counters = any(
        key in receiver
        for key in (
            "received_packets",
            "written_packets",
            "received_bytes",
            "written_bytes",
            "pending",
            "queue_overflow",
            "write_errors",
        )
    )
    if extended_counters:
        counter_consistent: bool | None = bool(
            receiver.get("received_packets") == receiver.get("written_packets")
            and receiver.get("received_bytes") == receiver.get("written_bytes")
            and receiver.get("pending") == 0
            and receiver.get("queue_overflow") == 0
            and receiver.get("write_errors") == 0
            and receiver.get("peer_mismatch") == 0
            and receiver.get("received_packets") == state.packet_count
        )
    elif receiver and "packets" in receiver and "bytes" in receiver:
        counter_consistent = bool(
            receiver.get("packets") == state.packet_count
            and receiver.get("bytes") == state.payload_bytes
            and receiver.get("peer_mismatch", 0) == 0
        )
    else:
        counter_consistent = None
    if counter_consistent is True:
        counter_status = "consistent"
    elif counter_consistent is False:
        counter_status = "inconsistent"
    else:
        counter_status = "unavailable"
    failures: list[str] = []
    if missing_paths:
        failures.append("missing_declared_csv")
    if not all_rows_match:
        failures.append("csv_rows_unmatched")
    if not all_files_contiguous:
        failures.append("csv_files_not_fully_contiguous")
    if not raw_windows_adequate:
        failures.append("raw_window_under_30s")
    if not readback_verified:
        failures.append("settings_readback_unverified")
    if counter_consistent is False:
        failures.append("writer_counters_inconsistent")
    return {
        "declared_csv_file_count": len(declared),
        "available_csv_file_count": len(existing),
        "missing_declared_csv_paths": missing_paths,
        "csv_row_count": row_count,
        "csv_matched_row_count": matched_row_count,
        "csv_unmatched_row_count": unmatched_row_count,
        "csv_fully_contiguous_file_count": contiguous_count,
        "all_csv_rows_match": all_rows_match,
        "all_csv_files_fully_matched_contiguous": all_files_contiguous,
        "raw_windows_all_at_least_30s": raw_windows_adequate,
        "settings_readback_all_verified": readback_verified,
        "writer_counters": counter_fields,
        "writer_counters_consistent": counter_consistent,
        "writer_counters_status": counter_status,
        "quality_status": "pass" if not failures else "fail",
        "quality_failures": failures,
        "csv_alignment": alignment,
    }


def analyze_session(session_path: Path) -> tuple[list[dict[str, Any]], dict[str, Any]]:
    session_dir, manifest_path, raw_path, manifest = locate_session(session_path)
    state, packet_rows = replay_raw(raw_path)
    windows = phase_event_times(read_events(session_dir / "events.jsonl"))
    phases = manifest.get("phases", [])
    if not isinstance(phases, list):
        phases = []
    rows = [
        phase_metrics(
            state,
            packet_rows,
            phase,
            windows.get(str(phase.get("phase_id", phase.get("id", ""))), {}),
            str(manifest.get("session_id", session_dir.name)),
            raw_path,
            manifest_path,
        )
        for phase in phases
        if isinstance(phase, dict)
    ]
    quality = session_quality(state, state.spokes, session_dir, manifest, rows)
    return rows, {
        "session_id": manifest.get("session_id", session_dir.name),
        "manifest_path": str(manifest_path),
        "raw_path": str(raw_path),
        "raw_sha256": sha256_file(raw_path),
        "full_stream_packets": state.packet_count,
        "full_stream_decoded_spokes": state.decoded_spoke_count,
        "full_stream_missing_declared_sweeps": state.declared_sweep_count
        - state.decoded_spoke_count,
        "quality": quality,
    }


def csv_write(path: Path, rows: Iterable[dict[str, Any]]) -> None:
    materialized = list(rows)
    fields = list(materialized[0]) if materialized else ["status"]
    with path.open("w", newline="", encoding="utf-8") as fp:
        writer = csv.DictWriter(fp, fieldnames=fields, extrasaction="ignore")
        writer.writeheader()
        writer.writerows(materialized)


def write_campaign(paths: list[Path], out_dir: Path) -> dict[str, Any]:
    phase_rows: list[dict[str, Any]] = []
    sessions: list[dict[str, Any]] = []
    quality_records: list[dict[str, Any]] = []
    errors: list[dict[str, str]] = []
    for path in paths:
        try:
            rows, session_summary = analyze_session(path)
        except (OSError, ValueError, KeyError) as exc:
            errors.append({"path": str(path), "error": str(exc)})
            continue
        phase_rows.extend(rows)
        quality = session_summary.get("quality", {})
        if isinstance(quality, dict):
            quality_records.append(
                {
                    "session_id": session_summary["session_id"],
                    "raw_sha256": session_summary["raw_sha256"],
                    "quality": quality,
                }
            )
            session_summary["quality"] = {
                key: value for key, value in quality.items() if key != "csv_alignment"
            }
        sessions.append(session_summary)
    comparisons = compare_modes(phase_rows)
    observed_modes = sorted(
        {
            row["comparison_mode"]
            for row in phase_rows
            if isinstance(row.get("comparison_mode"), int)
        }
    )
    summary = {
        "schema_version": SCHEMA_VERSION,
        "sessions": sessions,
        "errors": errors,
        "quality_status": "pass"
        if quality_records
        and all(record["quality"]["quality_status"] == "pass" for record in quality_records)
        else "fail",
        "quality_failure_count": sum(
            bool(record["quality"].get("quality_failures")) for record in quality_records
        ),
        "phase_count": len(phase_rows),
        "observed_doppler_modes": observed_modes,
        "required_doppler_modes": list(REQUIRED_MODES),
        "missing_doppler_modes": [mode for mode in REQUIRED_MODES if mode not in observed_modes],
        "observed_ef_processing_modes": observed_modes,
        "required_ef_processing_modes": list(REQUIRED_EF_MODES),
        "missing_ef_processing_modes": [
            mode for mode in REQUIRED_EF_MODES if mode not in observed_modes
        ],
        "required_processing_pairs": [list(pair) for pair in REQUIRED_PAIRS],
        "mode_comparison_count": len(comparisons),
        "comparison_ready": any(row["pair_ready"] for row in comparisons),
        "required_duration_s": REQUIRED_DURATION_S,
        "doppler_inference": {
            "status": "not_established",
            "direct_velocity_field_found": False,
            "potential_mode_control_flag": True,
            "reason": (
                "EF0, EF1, and EF2 are application processing presets represented by the "
                "captured settings. Their labels do not establish manufacturer Doppler "
                "semantics. The audit does not infer velocity without a paired moving-target "
                "design and a calibrated pulse-to-pulse phase, I/Q, range-rate, or velocity "
                "field."
            ),
        },
        "provenance": {
            "raw_interval_policy": (
                "replay complete raw stream first, then crop decoded spokes and packets by "
                "recording_window_start/end receive-time bounds"
            ),
            "settings_policy": (
                "requested and device readback are retained separately; comparisons require "
                "verified readback and >=30 s windows"
            ),
            "no_hardware_actions": True,
        },
    }
    out_dir.mkdir(parents=True, exist_ok=True)
    (out_dir / "doppler_campaign_summary.json").write_text(
        json.dumps(summary, indent=2) + "\n", encoding="utf-8"
    )
    quality_statuses = [
        record["quality"]["quality_status"]
        for record in quality_records
        if isinstance(record.get("quality"), dict)
    ]
    quality_document = {
        "schema_version": "paper1-doppler-quality-diagnostic-1",
        "quality_status": "pass"
        if quality_statuses and all(status == "pass" for status in quality_statuses)
        else "fail",
        "sessions": quality_records,
        "conclusions": [
            (
                f"{record['session_id']}: {record['quality']['csv_matched_row_count']} of "
                f"{record['quality']['csv_row_count']} CSV rows matched, "
                f"{record['quality']['csv_fully_contiguous_file_count']} of "
                f"{record['quality']['declared_csv_file_count']} files fully contiguous, "
                "raw_windows_all_at_least_30s="
                f"{record['quality']['raw_windows_all_at_least_30s']}, "
                "settings_readback_all_verified="
                f"{record['quality']['settings_readback_all_verified']}, "
                f"writer_counters_consistent={record['quality']['writer_counters_consistent']}."
            )
            for record in quality_records
        ],
        "provenance": {
            "csv_policy": (
                "every declared CSV row must match the replay in order and every file "
                "must be fully contiguous"
            ),
            "raw_window_policy": (
                "every manifest recording window must contain at least 30 seconds "
                "between first and last received packet"
            ),
            "counter_policy": (
                "extended writer counters must agree with zero pending, overflow, "
                "write errors, and peer mismatches; legacy manifests require "
                "packet-count agreement and zero peer mismatches"
            ),
            "no_hardware_actions": True,
        },
    }
    (out_dir / "doppler_quality_diagnostic.json").write_text(
        json.dumps(quality_document, indent=2) + "\n", encoding="utf-8"
    )
    csv_write(
        out_dir / "doppler_quality_csv_alignment.csv",
        [
            alignment_row | {"session_id": record["session_id"], "raw_sha256": record["raw_sha256"]}
            for record in quality_records
            for alignment_row in record["quality"].get("csv_alignment", [])
        ],
    )
    csv_write(out_dir / "doppler_phase_metrics.csv", phase_rows)
    csv_write(out_dir / "doppler_mode_comparisons.csv", comparisons)
    return summary


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, help="Directory containing session subdirectories")
    parser.add_argument("--session", type=Path, action="append", default=[])
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    paths = list(args.session)
    if args.root:
        paths.extend(
            sorted(
                path
                for path in args.root.iterdir()
                if path.is_dir()
                and (path / "session_manifest.json").exists()
                and load_object(path / "session_manifest.json").get("status") == "complete"
            )
        )
    if not paths:
        parser.error("provide --root or at least one --session")
    summary = write_campaign(paths, args.out)
    print(json.dumps(summary, indent=2))
    return 0 if not summary["errors"] else 2


if __name__ == "__main__":
    raise SystemExit(main())

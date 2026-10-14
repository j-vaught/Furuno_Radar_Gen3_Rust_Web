#!/usr/bin/env python3
"""Replay Furuno UDP echo packets and audit signal-code evidence.

This module deliberately owns a small, independent decoder instead of importing
the Rust or ``analyze_capture.py`` implementation.  It preserves the decoded
wire value before the renderer's ``value >> 2`` conversion, records the two
discarded low bits separately, and carries the previous spoke across packet
boundaries for the delta encodings.

The audit describes what is present on the Ethernet wire.  It does not infer
ADC resolution or Doppler velocity from amplitude codes.  The command-line
entry point emits compact JSON and CSV files suitable for the Paper 1 figure
data pipeline.
"""

from __future__ import annotations

import argparse
import base64
import binascii
import csv
import hashlib
import json
from collections import Counter, defaultdict
from collections.abc import Iterable
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

SCHEMA_VERSION = "paper1-doppler-bit-audit-1"


@dataclass
class SweepDecode:
    values: list[int]
    consumed: int
    available: int
    complete: bool
    literal_count: int = 0
    repeat_count: int = 0
    previous_copy_count: int = 0
    malformed: bool = False


@dataclass
class SpokeRecord:
    packet_index: int
    sweep_index: int
    angle_ticks: int
    heading_ticks: int
    encoding: int
    sweep_len: int
    values: list[int]
    complete: bool
    receive_unix_ns: int
    packet_sequence: int | None


@dataclass
class ReplayState:
    packet_count: int = 0
    malformed_packet_count: int = 0
    unsupported_encoding_count: int = 0
    truncated_packet_count: int = 0
    spoke_start_truncation_count: int = 0
    sweep_payload_truncation_count: int = 0
    decoded_spoke_count: int = 0
    complete_spoke_count: int = 0
    declared_sweep_count: int = 0
    encodings: Counter[int] = field(default_factory=Counter)
    sweep_lengths: Counter[int] = field(default_factory=Counter)
    header_flags: Counter[int] = field(default_factory=Counter)
    header_configs: Counter[int] = field(default_factory=Counter)
    packet_sequences: list[int] = field(default_factory=list)
    timestamps: list[int] = field(default_factory=list)
    values: Counter[int] = field(default_factory=Counter)
    low2: Counter[int] = field(default_factory=Counter)
    upper6: Counter[int] = field(default_factory=Counter)
    wire_body_values: Counter[int] = field(default_factory=Counter)
    wire_body_low2: Counter[int] = field(default_factory=Counter)
    headings: Counter[int] = field(default_factory=Counter)
    angles: Counter[int] = field(default_factory=Counter)
    operation_counts: Counter[str] = field(default_factory=Counter)
    spokes: list[SpokeRecord] = field(default_factory=list)


def _aligned(value: int) -> int:
    return (value + 3) & ~3


def _decode_sweep(data: bytes, encoding: int, sweep_len: int, previous: list[int]) -> SweepDecode:
    """Decode one body using the wire decoder's four encodings.

    ``consumed`` includes the four-byte padding required by the packet format.
    ``complete`` is false when the advertised sweep length or padding is absent.
    The returned values always retain their pre-shift 8-bit wire values.
    """

    if encoding == 0:
        count = min(len(data), sweep_len)
        values = list(data[:count])
        values.extend([0] * (sweep_len - count))
        consumed = _aligned(count)
        return SweepDecode(
            values=values,
            consumed=consumed,
            available=len(data),
            complete=count == sweep_len and consumed <= len(data),
            literal_count=count,
        )

    values: list[int] = []
    used = 0
    strength = 0
    literal_count = 0
    repeat_count = 0
    previous_copy_count = 0

    while len(values) < sweep_len and used < len(data):
        byte = data[used]
        if encoding == 1:
            if byte & 1 == 0:
                strength = byte
                values.append(strength)
                literal_count += 1
            else:
                count = byte >> 1 or 128
                values.extend([strength] * min(count, sweep_len - len(values)))
                repeat_count += 1
        elif encoding == 2:
            if byte & 1 == 0:
                values.append(byte)
                literal_count += 1
            else:
                count = byte >> 1 or 128
                start = len(values)
                count = min(count, sweep_len - start)
                values.extend(
                    previous[i] if i < len(previous) else 0 for i in range(start, start + count)
                )
                previous_copy_count += 1
        elif encoding == 3:
            if byte & 3 == 0:
                strength = byte
                values.append(strength)
                literal_count += 1
            else:
                count = byte >> 2 or 64
                count = min(count, sweep_len - len(values))
                if byte & 1:
                    values.extend([strength] * count)
                    repeat_count += 1
                else:
                    start = len(values)
                    for i in range(start, start + count):
                        strength = previous[i] if i < len(previous) else 0
                        values.append(strength)
                    previous_copy_count += 1
        else:
            return SweepDecode(
                values=values,
                consumed=len(data),
                available=len(data),
                complete=False,
                malformed=True,
            )
        used += 1

    consumed = _aligned(used)
    complete = len(values) == sweep_len and consumed <= len(data)
    return SweepDecode(
        values=values,
        consumed=consumed,
        available=len(data),
        complete=complete,
        literal_count=literal_count,
        repeat_count=repeat_count,
        previous_copy_count=previous_copy_count,
    )


def _packet_metadata(payload: bytes) -> dict[str, Any] | None:
    if len(payload) < 20:
        return None
    encoding = (payload[11] & 0x18) >> 3
    return {
        "header_hex": payload[:16].hex(),
        "sequence_be": int.from_bytes(payload[0:2], "big"),
        "sequence_le": int.from_bytes(payload[0:2], "little"),
        "timestamp_u32_be": int.from_bytes(payload[8:12], "big"),
        "flags_u16_be": int.from_bytes(payload[12:14], "big"),
        "config_u16_be": int.from_bytes(payload[14:16], "big"),
        "sweep_count": payload[9] >> 1,
        "sweep_length": ((payload[11] & 0x07) << 8) | payload[10],
        "encoding": encoding,
        "header_byte_9": payload[9],
        "header_byte_10": payload[10],
        "header_byte_11": payload[11],
    }


def replay_raw(path: Path) -> tuple[ReplayState, list[dict[str, Any]]]:
    """Replay JSONL UDP records and return aggregate state plus packet rows."""

    state = ReplayState()
    packet_rows: list[dict[str, Any]] = []
    previous: list[int] = []

    with path.open(encoding="utf-8", errors="replace") as fp:
        for line_number, line in enumerate(fp, start=1):
            if not line.strip():
                continue
            try:
                record = json.loads(line)
                encoded = record.get("payload_b64") or record.get("payload")
                payload = base64.b64decode(encoded, validate=True)
                receive_ns = int(
                    record.get(
                        "received_unix_ns", record.get("receive_unix_ns", record.get("unix_ns", 0))
                    )
                )
            except (ValueError, TypeError, json.JSONDecodeError, binascii.Error):
                state.malformed_packet_count += 1
                packet_rows.append(
                    {"line_number": line_number, "status": "malformed_json_or_base64"}
                )
                continue

            state.packet_count += 1
            state.timestamps.append(receive_ns)
            metadata = _packet_metadata(payload)
            if metadata is None:
                state.malformed_packet_count += 1
                packet_rows.append(
                    {
                        "line_number": line_number,
                        "packet_index": state.packet_count - 1,
                        "status": "short_packet",
                        "receive_unix_ns": receive_ns,
                        "packet_bytes": len(payload),
                    }
                )
                continue

            encoding = int(metadata["encoding"])
            state.encodings[encoding] += 1
            state.declared_sweep_count += int(metadata["sweep_count"])
            for value in payload[16:]:
                state.wire_body_values[value] += 1
                state.wire_body_low2[value & 3] += 1
            state.sweep_lengths[int(metadata["sweep_length"])] += 1
            state.header_flags[int(metadata["flags_u16_be"])] += 1
            state.header_configs[int(metadata["config_u16_be"])] += 1
            state.packet_sequences.append(int(metadata["sequence_be"]))
            row: dict[str, Any] = {
                "line_number": line_number,
                "packet_index": state.packet_count - 1,
                "status": "ok",
                "receive_unix_ns": receive_ns,
                "packet_bytes": len(payload),
                **metadata,
                "decoded_spokes": 0,
                "complete_spokes": 0,
                "truncated": False,
            }

            if encoding not in (0, 1, 2, 3):
                state.unsupported_encoding_count += 1
                row["status"] = "unsupported_encoding"
                packet_rows.append(row)
                continue

            cursor = 16
            packet_truncated = False
            for sweep_index in range(int(metadata["sweep_count"])):
                if cursor + 4 > len(payload):
                    state.truncated_packet_count += 1
                    state.spoke_start_truncation_count += 1
                    packet_truncated = True
                    break
                angle = int.from_bytes(payload[cursor : cursor + 2], "little")
                heading = int.from_bytes(payload[cursor + 2 : cursor + 4], "little")
                cursor += 4
                effective = 1 if encoding == 2 and sweep_index == 0 else encoding
                decoded = _decode_sweep(
                    payload[cursor:], effective, int(metadata["sweep_length"]), previous
                )
                if decoded.malformed:
                    state.unsupported_encoding_count += 1
                    packet_truncated = True
                    break
                if decoded.consumed > decoded.available:
                    state.truncated_packet_count += 1
                    state.sweep_payload_truncation_count += 1
                    packet_truncated = True
                    break
                if len(decoded.values) != int(metadata["sweep_length"]):
                    state.truncated_packet_count += 1
                    state.sweep_payload_truncation_count += 1
                    packet_truncated = True
                cursor += decoded.consumed
                previous = decoded.values
                state.decoded_spoke_count += 1
                if decoded.complete:
                    state.complete_spoke_count += 1
                state.angles[angle] += 1
                state.headings[heading] += 1
                state.operation_counts["literal"] += decoded.literal_count
                state.operation_counts["repeat"] += decoded.repeat_count
                state.operation_counts["previous_copy"] += decoded.previous_copy_count
                for value in decoded.values:
                    state.values[value] += 1
                    state.low2[value & 3] += 1
                    state.upper6[value >> 2] += 1
                state.spokes.append(
                    SpokeRecord(
                        packet_index=state.packet_count - 1,
                        sweep_index=sweep_index,
                        angle_ticks=angle,
                        heading_ticks=heading,
                        encoding=encoding,
                        sweep_len=int(metadata["sweep_length"]),
                        values=decoded.values,
                        complete=decoded.complete,
                        receive_unix_ns=receive_ns,
                        packet_sequence=int(metadata["sequence_be"]),
                    )
                )
                row["decoded_spokes"] += 1
                row["complete_spokes"] += int(decoded.complete)
            if packet_truncated:
                row["truncated"] = True
            packet_rows.append(row)

    return state, packet_rows


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as fp:
        for block in iter(lambda: fp.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def _counter_rows(counter: Counter[int], field: str) -> list[dict[str, Any]]:
    total = sum(counter.values())
    return [
        {field: key, "count": value, "fraction": value / total if total else None}
        for key, value in sorted(counter.items())
    ]


def _csv_rows(path: Path) -> Iterable[tuple[int, list[int]]]:
    with path.open(encoding="utf-8", errors="replace", newline="") as fp:
        reader = csv.reader((line for line in fp if not line.startswith("#")), delimiter="\t")
        header = next(reader, [])
        if "Angle" not in header or "EchoValues" not in header:
            return
        angle_index = header.index("Angle")
        values_index = header.index("EchoValues")
        for row in reader:
            if len(row) <= max(angle_index, values_index):
                continue
            try:
                angle = int(row[angle_index])
                values = [int(value) for value in row[values_index].split(",") if value != ""]
            except ValueError:
                continue
            yield angle, values


def compare_csv(csv_paths: Iterable[Path], spokes: list[SpokeRecord]) -> list[dict[str, Any]]:
    """Compare rendered upper-six-bit sequences with replayed spoke sequences."""

    index: dict[tuple[int, str], list[int]] = defaultdict(list)
    for spoke_index, spoke in enumerate(spokes):
        displayed = bytes(value >> 2 for value in spoke.values)
        key = (spoke.angle_ticks, hashlib.sha256(displayed).hexdigest())
        index[key].append(spoke_index)

    results = []
    for path in sorted(csv_paths):
        rows = list(_csv_rows(path))
        matches: list[int] = []
        previous_match = -1
        for angle, values in rows:
            key = (angle, hashlib.sha256(bytes(values)).hexdigest())
            candidates = index.get(key, [])
            candidate = next((item for item in candidates if item > previous_match), -1)
            matches.append(candidate)
            if candidate >= 0:
                previous_match = candidate
        found = [match for match in matches if match >= 0]
        monotonic = all(a <= b for a, b in zip(found, found[1:], strict=False))
        contiguous = bool(found) and all(b == a + 1 for a, b in zip(found, found[1:], strict=False))
        results.append(
            {
                "csv_path": str(path),
                "csv_sha256": sha256_file(path),
                "row_count": len(rows),
                "matched_row_count": len(found),
                "match_fraction": len(found) / len(rows) if rows else None,
                "first_match_replay_index": found[0] if found else None,
                "last_match_replay_index": found[-1] if found else None,
                "match_indices_monotonic": monotonic,
                "match_indices_contiguous": contiguous,
                "unmatched_row_count": len(rows) - len(found),
                "interpretation": (
                    "exact rendered upper-six-bit sequence and angle match; no match "
                    "does not prove packet loss"
                ),
            }
        )
    return results


def capture_design() -> list[dict[str, Any]]:
    """Return the controlled 30-second matrix without issuing hardware commands."""

    common = {
        "duration_s": 30,
        "range_index": 0,
        "gain_auto": False,
        "gain_value": 128,
        "sea_clutter": 0,
        "rain_clutter": 0,
        "interference_rejection": False,
        "bird_mode": 0,
        "antenna_speed": 0,
        "weather_recording_required": True,
        "scene": "stationary indoor scene",
    }
    rows = []
    for mode, label in ((0, "off"), (1, "target_analyzer"), (2, "rain")):
        row = dict(common)
        row.update(
            {
                "condition_id": f"D{mode}",
                "doppler_mode": mode,
                "doppler_mode_label": label,
                "moving_target_actor": False,
                "purpose": "same-scene 30-second wire comparison",
            }
        )
        rows.append(row)
    moving = dict(common)
    moving.update(
        {
            "condition_id": "DM1",
            "doppler_mode": 1,
            "doppler_mode_label": "target_analyzer",
            "moving_target_actor": True,
            "purpose": "separate moving-target demonstration; required before any velocity claim",
        }
    )
    rows.append(moving)
    return rows


def _json_safe_counter(counter: Counter[Any]) -> dict[str, int]:
    return {str(key): value for key, value in sorted(counter.items())}


def replay_checksum(spokes: Iterable[SpokeRecord]) -> dict[str, Any]:
    hash_value = 14695981039346656037
    value_count = 0
    value_sum = 0
    value_xor = 0
    for spoke in spokes:
        fields = (
            spoke.angle_ticks & 0xFF,
            (spoke.angle_ticks >> 8) & 0xFF,
            spoke.heading_ticks & 0xFF,
            (spoke.heading_ticks >> 8) & 0xFF,
            *spoke.values,
        )
        for value in fields:
            hash_value ^= value
            hash_value = (hash_value * 1099511628211) & ((1 << 64) - 1)
        for value in spoke.values:
            value_count += 1
            value_sum += value
            value_xor ^= value
    return {
        "decoded_value_count": value_count,
        "decoded_value_sum": value_sum,
        "decoded_value_xor": value_xor,
        "fnv1a64_angle_heading_raw_values": f"{hash_value:016x}",
        "definition": (
            "FNV-1a over little-endian angle ticks, little-endian heading ticks, "
            "and each pre-shift decoded value in replay order"
        ),
    }


def build_summary(
    raw_path: Path,
    state: ReplayState,
    packet_rows: list[dict[str, Any]],
    alignment: list[dict[str, Any]],
) -> dict[str, Any]:
    timestamp_monotonic = all(
        a <= b for a, b in zip(state.timestamps, state.timestamps[1:], strict=False)
    )
    value_count = sum(state.values.values())
    low2_nonzero = value_count - state.low2[0]
    return {
        "schema_version": SCHEMA_VERSION,
        "raw_path": str(raw_path),
        "raw_sha256": sha256_file(raw_path),
        "packet_count": state.packet_count,
        "malformed_packet_count": state.malformed_packet_count,
        "unsupported_encoding_count": state.unsupported_encoding_count,
        "truncated_packet_count": state.truncated_packet_count,
        "spoke_start_truncation_count": state.spoke_start_truncation_count,
        "sweep_payload_truncation_count": state.sweep_payload_truncation_count,
        "decoded_spoke_count": state.decoded_spoke_count,
        "complete_spoke_count": state.complete_spoke_count,
        "declared_sweep_count": state.declared_sweep_count,
        "missing_declared_sweep_count": state.declared_sweep_count - state.decoded_spoke_count,
        "encoding_packet_counts": _json_safe_counter(state.encodings),
        "sweep_length_counts": _json_safe_counter(state.sweep_lengths),
        "packet_header_flags_counts": _json_safe_counter(state.header_flags),
        "packet_header_config_counts": _json_safe_counter(state.header_configs),
        "packet_sequence": {
            "min": min(state.packet_sequences) if state.packet_sequences else None,
            "max": max(state.packet_sequences) if state.packet_sequences else None,
            "unique": len(set(state.packet_sequences)),
        },
        "timestamp": {
            "count": len(state.timestamps),
            "monotonic": timestamp_monotonic,
            "first_unix_ns": state.timestamps[0] if state.timestamps else None,
            "last_unix_ns": state.timestamps[-1] if state.timestamps else None,
            "duration_s": (
                (state.timestamps[-1] - state.timestamps[0]) / 1e9
                if len(state.timestamps) > 1
                else None
            ),
        },
        "heading_ticks": {
            "count": sum(state.headings.values()),
            "unique": len(state.headings),
            "min": min(state.headings) if state.headings else None,
            "max": max(state.headings) if state.headings else None,
            "counts": _json_safe_counter(state.headings),
        },
        "angle_ticks": {
            "count": sum(state.angles.values()),
            "unique": len(state.angles),
            "min": min(state.angles) if state.angles else None,
            "max": max(state.angles) if state.angles else None,
        },
        "bitplanes": {
            "decoded_value_count": value_count,
            "pre_shift_unique_code_count": len(state.values),
            "pre_shift_codes": _json_safe_counter(state.values),
            "pre_shift_code_min": min(state.values) if state.values else None,
            "pre_shift_code_max": max(state.values) if state.values else None,
            "low2_class_counts": _json_safe_counter(state.low2),
            "low2_nonzero_fraction": low2_nonzero / value_count if value_count else None,
            "upper6_code_counts": _json_safe_counter(state.upper6),
            "upper6_unique_code_count": len(state.upper6),
            "display_conversion": (
                "upper6 = pre_shift_code >> 2; this is the renderer conversion, "
                "not a proven ADC bit depth"
            ),
            "encoded_body_byte_count": sum(state.wire_body_values.values()),
            "encoded_body_low2_class_counts": _json_safe_counter(state.wire_body_low2),
            "encoded_body_note": (
                "Encoded-body counts include sweep angle/heading words, compression "
                "markers, literals, and padding; they are not amplitude-code counts."
            ),
        },
        "decoder_operations": _json_safe_counter(state.operation_counts),
        "replay_checksum": replay_checksum(state.spokes),
        "csv_alignment": alignment,
        "doppler_evidence": {
            "status": "not_established",
            "observed_wire_fields": [
                "receive_unix_ns",
                "packet sequence bytes 0-1",
                "per-spoke angle ticks",
                "per-spoke heading ticks",
                "echo amplitude codes before and after renderer shift",
                "header flags and configuration words",
            ],
            "velocity_observable": False,
            "direct_velocity_field_found": False,
            "potential_mode_control_flag": True,
            "reason": (
                "The replayed packets expose amplitude-coded spokes and timing/angle metadata, "
                "but this audit found no calibrated pulse-to-pulse phase, I/Q, range-rate, or "
                "velocity field. A moving-target paired capture is required before treating "
                "Doppler mode differences as velocity evidence."
            ),
            "doppler_mode_interpretation": (
                "wire-level modes 0, 1, and 2 are experimental conditions; installed-unit "
                "semantics remain an observation to verify from readback and paired captures"
            ),
            "discarded_low_bits": (
                "retained and quantified; their presence or absence is not interpreted as "
                "Doppler evidence"
            ),
        },
        "capture_design": capture_design(),
        "provenance": {
            "decoder_basis": [
                "furuno-server/src/protocol.rs parse_echo_metadata",
                "furuno-server/src/protocol.rs decode_sweep_encoding_0..3",
                "furuno-server/src/render.rs amplitude conversion",
            ],
            "analysis_scope": (
                "independent Python replay of saved JSONL; no hardware commands issued"
            ),
        },
    }


def write_csv(path: Path, rows: Iterable[dict[str, Any]], fieldnames: list[str]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="", encoding="utf-8") as fp:
        writer = csv.DictWriter(fp, fieldnames=fieldnames, extrasaction="ignore")
        writer.writeheader()
        writer.writerows(rows)


def spoke_metric_rows(spokes: Iterable[SpokeRecord]) -> Iterable[dict[str, Any]]:
    for index, spoke in enumerate(spokes):
        values = spoke.values
        display = [value >> 2 for value in values]
        low2_nonzero = sum(value & 3 != 0 for value in values)
        yield {
            "spoke_index": index,
            "packet_index": spoke.packet_index,
            "sweep_index": spoke.sweep_index,
            "receive_unix_ns": spoke.receive_unix_ns,
            "packet_sequence": spoke.packet_sequence,
            "angle_ticks": spoke.angle_ticks,
            "heading_ticks": spoke.heading_ticks,
            "encoding": spoke.encoding,
            "sweep_length": spoke.sweep_len,
            "complete": spoke.complete,
            "raw_min": min(values) if values else None,
            "raw_max": max(values) if values else None,
            "raw_mean": sum(values) / len(values) if values else None,
            "display_min": min(display) if display else None,
            "display_max": max(display) if display else None,
            "display_mean": sum(display) / len(display) if display else None,
            "raw_unique_count": len(set(values)),
            "low2_nonzero_count": low2_nonzero,
            "low2_nonzero_fraction": low2_nonzero / len(values) if values else None,
        }


def write_outputs(raw_path: Path, out_dir: Path, csv_paths: list[Path]) -> dict[str, Any]:
    state, packet_rows = replay_raw(raw_path)
    alignment = compare_csv(csv_paths, state.spokes)
    summary = build_summary(raw_path, state, packet_rows, alignment)
    out_dir.mkdir(parents=True, exist_ok=True)
    (out_dir / "doppler_bit_audit.json").write_text(
        json.dumps(summary, indent=2) + "\n", encoding="utf-8"
    )
    write_csv(
        out_dir / "packet_metadata.csv",
        packet_rows,
        [
            "line_number",
            "packet_index",
            "status",
            "receive_unix_ns",
            "packet_bytes",
            "sequence_be",
            "sequence_le",
            "timestamp_u32_be",
            "flags_u16_be",
            "config_u16_be",
            "sweep_count",
            "sweep_length",
            "encoding",
            "header_byte_9",
            "header_byte_10",
            "header_byte_11",
            "decoded_spokes",
            "complete_spokes",
            "truncated",
        ],
    )
    write_csv(
        out_dir / "pre_shift_code_histogram.csv",
        _counter_rows(state.values, "pre_shift_code"),
        ["pre_shift_code", "count", "fraction"],
    )
    write_csv(
        out_dir / "low2_class_histogram.csv",
        _counter_rows(state.low2, "low2_class"),
        ["low2_class", "count", "fraction"],
    )
    write_csv(
        out_dir / "encoded_body_low2_histogram.csv",
        _counter_rows(state.wire_body_low2, "low2_class"),
        ["low2_class", "count", "fraction"],
    )
    write_csv(
        out_dir / "upper6_code_histogram.csv",
        _counter_rows(state.upper6, "upper6_code"),
        ["upper6_code", "count", "fraction"],
    )
    write_csv(
        out_dir / "spoke_metrics.csv",
        spoke_metric_rows(state.spokes),
        [
            "spoke_index",
            "packet_index",
            "sweep_index",
            "receive_unix_ns",
            "packet_sequence",
            "angle_ticks",
            "heading_ticks",
            "encoding",
            "sweep_length",
            "complete",
            "raw_min",
            "raw_max",
            "raw_mean",
            "display_min",
            "display_max",
            "display_mean",
            "raw_unique_count",
            "low2_nonzero_count",
            "low2_nonzero_fraction",
        ],
    )
    write_csv(
        out_dir / "heading_histogram.csv",
        _counter_rows(state.headings, "heading_ticks"),
        ["heading_ticks", "count", "fraction"],
    )
    write_csv(
        out_dir / "header_flags_histogram.csv",
        _counter_rows(state.header_flags, "flags_u16_be"),
        ["flags_u16_be", "count", "fraction"],
    )
    write_csv(
        out_dir / "csv_alignment.csv", alignment, list(alignment[0]) if alignment else ["csv_path"]
    )
    write_csv(out_dir / "capture_design.csv", capture_design(), list(capture_design()[0]))
    return summary


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--raw", type=Path, required=True, help="Saved echo UDP JSONL")
    parser.add_argument("--out", type=Path, required=True, help="External output directory")
    parser.add_argument(
        "--csv", type=Path, nargs="*", default=[], help="Rendered rotation CSV files"
    )
    args = parser.parse_args()
    summary = write_outputs(args.raw, args.out, args.csv)
    print(
        json.dumps(
            {
                key: summary[key]
                for key in (
                    "packet_count",
                    "declared_sweep_count",
                    "decoded_spoke_count",
                    "missing_declared_sweep_count",
                    "complete_spoke_count",
                    "encoding_packet_counts",
                    "doppler_evidence",
                )
            },
            indent=2,
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

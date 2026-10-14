"""Derive compact, inspectable figure inputs from the first live radar capture.

The figure sources consume JSON only. This script records the source paths and
summary counts so the plotted values can be audited without embedding the
large CSV/JSONL capture in the repository.
"""

from __future__ import annotations

import base64
import hashlib
import json
import math
from collections import Counter
from datetime import UTC, datetime
from pathlib import Path

ROOT = Path("/Volumes/MacApplications/Radar_papers")
RAW = ROOT / "macos-bringup/logs/tx-test-20261014T042023Z/echo-udp-payloads.jsonl"
CAPTURE = ROOT / "data/live/macos-recordings/20261014_042023"
REPORT = ROOT / "macos-bringup/logs/tx-test-20261014T042023Z/report.json"
OUT = Path(__file__).resolve().parents[1] / "data"


def digest(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for block in iter(lambda: f.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


def load_raw() -> tuple[list[int], list[int], list[int]]:
    times: list[int] = []
    lengths: list[int] = []
    codes: list[int] = []
    for line in RAW.read_text().splitlines():
        rec = json.loads(line)
        payload = base64.b64decode(rec["payload_b64"])
        times.append(int(rec["received_unix_ns"]))
        lengths.append(len(payload))
        codes.append(payload[1] if len(payload) > 1 else -1)
    return times, lengths, codes


def csv_stats(path: Path) -> dict[str, object]:
    hist = Counter()
    angles: list[int] = []
    nonzero_by_row: list[int] = []
    value_sum_by_row: list[float] = []
    bins_count = 0
    with path.open(newline="") as f:
        for row in f:
            if row.startswith("#") or row.startswith("Status"):
                continue
            fields = row.rstrip("\n").split("\t")
            if len(fields) != 6:
                continue
            try:
                angle = int(fields[4])
                values = [int(x) for x in fields[5].split(",") if x]
            except ValueError:
                continue
            angles.append(angle)
            bins_count = len(values)
            nonzero_by_row.append(sum(value > 0 for value in values))
            value_sum_by_row.append(float(sum(values)))
            hist.update(values)
    nonzero_codes = sorted((code, count) for code, count in hist.items() if count > 0)
    return {
        "name": path.name,
        "rows": len(angles),
        "bins": bins_count,
        "angle_min": min(angles) if angles else None,
        "angle_max": max(angles) if angles else None,
        "angle_support": sorted(set(angles)),
        "angle_bins": list(range(64)),
        "angle_bin_counts": [
            sum(1 for angle in angles if angle // 128 == bucket) for bucket in range(64)
        ],
        "code_values": list(range(64)),
        "code_counts": [hist.get(i, 0) for i in range(64)],
        # Keep zero-count bins in the audit data, but omit them from the
        # logarithmic plot so an artificial one-count floor is never shown.
        "code_values_nonzero": [code for code, _ in nonzero_codes],
        "code_counts_nonzero": [count for _, count in nonzero_codes],
        "angle_bin_deg": [bucket * 5.625 for bucket in range(64)],
        "nonzero_by_row": nonzero_by_row,
        "value_sum_by_row": value_sum_by_row,
        "sha256": digest(path),
    }


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    times, lengths, packet_codes = load_raw()
    intervals_ms = [(b - a) / 1e6 for a, b in zip(times, times[1:])]
    # A fixed-width decimation keeps the Typst source small while retaining
    # the complete capture in the JSONL artifact and its recorded digest.
    stride = max(1, math.ceil(len(intervals_ms) / 600))
    raw = {
        "source": str(RAW),
        "sha256": digest(RAW),
        "packets": len(times),
        "payload_bytes": sum(lengths),
        "start_utc": datetime.fromtimestamp(times[0] / 1e9, UTC).isoformat(),
        "duration_s": (times[-1] - times[0]) / 1e9,
        "interval_ms": intervals_ms[::stride],
        "rate_hz": [1000.0 / value for value in intervals_ms[::stride]],
        "packet_index": list(range(1, len(intervals_ms) + 1, stride)),
        "payload_length_values": sorted(set(lengths)),
        "packet_code_histogram": [packet_codes.count(i) for i in range(4)],
        "interval_mean_ms": sum(intervals_ms) / len(intervals_ms),
        "interval_min_ms": min(intervals_ms),
        "interval_max_ms": max(intervals_ms),
    }
    (OUT / "raw_interarrival.json").write_text(json.dumps(raw, indent=2) + "\n")

    csv_paths = sorted(CAPTURE.glob("csv/*.csv"))
    if len(csv_paths) != 3:
        raise RuntimeError(f"expected three CSV rotations, found {len(csv_paths)}")
    stats = [csv_stats(path) for path in csv_paths]
    codes = {
        "source": [str(path) for path in csv_paths],
        "records": stats,
        "settings": {
            "source": str(CAPTURE / "settings.json"),
            "sha256": digest(CAPTURE / "settings.json"),
        },
    }
    (OUT / "decoded_support.json").write_text(json.dumps(codes, indent=2) + "\n")

    report = json.loads(REPORT.read_text())
    contract = {
        "status": "ready",
        "source": str(REPORT),
        "tx_window_s": report["tx_request_to_off_request_seconds"],
        "raw_packet_count": report["raw_capture"]["packets"],
        "decoded_rotation_count": len(report["output_validation"]),
        "settings_readback_count": len(report["readbacks_before"]),
        "settings_metadata_status": report["before"]["settings"]["status"],
        "tx_on_ack": report["checks"]["tx_on_acknowledged"],
        "tx_off_ack": report["checks"]["tx_off_acknowledged"],
        "standby_readback": report["checks"]["standby_readback"],
        "metadata_persisted": True,
    }
    (OUT / "capture_contract.json").write_text(json.dumps(contract, indent=2) + "\n")


if __name__ == "__main__":
    main()

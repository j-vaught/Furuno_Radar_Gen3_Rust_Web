import base64
import csv
import json
import tempfile
import unittest
from pathlib import Path

from scripts.doppler_campaign_audit import write_campaign


def packet(angle: int, heading: int, values: bytes) -> bytes:
    header = bytearray(16)
    header[9] = 2
    header[10] = len(values)
    header[11] = 3 << 3
    body = bytearray()
    body.extend(angle.to_bytes(2, "little"))
    body.extend(heading.to_bytes(2, "little"))
    body.extend(values)
    body.extend(b"\x00" * ((-len(values)) % 4))
    return bytes(header + body)


def write_session(root: Path, mode: int, start: int) -> Path:
    session = root / f"session-{mode}"
    session.mkdir()
    manifest = {
        "session_id": f"session-{mode}",
        "phases": [
            {
                "phase_id": f"D{mode}",
                "record_s": 30,
                "requested_settings": {"doppler": mode, "range_index": 0},
                "device_readback": {
                    "confirmed": True,
                    "fresh": True,
                    "verified": True,
                    "settings": {"doppler": mode, "range_index": 0},
                },
                "readback_matches": {"doppler": True, "range_index": True},
            }
        ],
    }
    (session / "session_manifest.json").write_text(json.dumps(manifest) + "\n", encoding="utf-8")
    end = start + 30_000_000_000
    events = [
        {"event": "recording_window_start", "phase_id": f"D{mode}", "unix_ns": start},
        {"event": "recording_window_end", "phase_id": f"D{mode}", "unix_ns": end},
    ]
    (session / "events.jsonl").write_text(
        "".join(json.dumps(event) + "\n" for event in events), encoding="utf-8"
    )
    payload = packet(100 + mode, 7, bytes([4, 8, 12, 16]))
    raw = {
        "received_unix_ns": start + 1,
        "payload_b64": base64.b64encode(payload).decode(),
    }
    (session / "raw_udp_payloads.jsonl").write_text(json.dumps(raw) + "\n", encoding="utf-8")
    return session


class DopplerCampaignAuditTests(unittest.TestCase):
    def test_crops_windows_and_requires_all_three_modes(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "sessions"
            root.mkdir()
            sessions = [
                write_session(root, mode, 1_000_000_000_000 + mode * 100) for mode in range(3)
            ]
            out = Path(tmp) / "audit"
            summary = write_campaign(sessions, out)

            self.assertEqual(summary["errors"], [])
            self.assertEqual(summary["observed_doppler_modes"], [0, 1, 2])
            self.assertTrue(summary["comparison_ready"])
            self.assertEqual(summary["mode_comparison_count"], 3)
            metrics = (out / "doppler_phase_metrics.csv").read_text(encoding="utf-8")
            self.assertEqual(metrics.count("\n"), 4)
            self.assertIn("True", metrics)

    def test_missing_window_is_reported_without_using_full_stream(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "sessions"
            root.mkdir()
            session = write_session(root, 0, 1_000_000_000_000)
            events = session / "events.jsonl"
            events.write_text(
                json.dumps(
                    {
                        "event": "recording_window_start",
                        "phase_id": "D0",
                        "unix_ns": 2_000_000_000_000,
                    }
                )
                + "\n",
                encoding="utf-8",
            )
            out = Path(tmp) / "audit"
            summary = write_campaign([session], out)

            self.assertEqual(summary["phase_count"], 1)
            with (out / "doppler_phase_metrics.csv").open(newline="", encoding="utf-8") as fp:
                row = next(csv.DictReader(fp))
            self.assertEqual(row["duration_adequate"], "False")
            self.assertEqual(row["raw_interval_cropped"], "False")
            self.assertEqual(row["decoded_spoke_count"], "0")
            self.assertFalse(summary["comparison_ready"])


if __name__ == "__main__":
    unittest.main()

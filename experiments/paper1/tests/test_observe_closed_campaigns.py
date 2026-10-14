import json
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from scripts.observe_closed_campaigns import (
    OBSERVER_VERSION,
    campaign_children,
    mirror_ready,
    run_once,
    sha256_file,
)


def manifest(session_id: str, record_name: str) -> dict:
    return {
        "status": "complete",
        "session_id": session_id,
        "phases": [
            {
                "phase_id": "p0",
                "record_dir": (
                    "/Volumes/MacApplications/Radar_papers/data/live/macos-recordings/"
                    f"{record_name}"
                ),
                "recording_output": {
                    "csv_files": [
                        {
                            "path": (
                                "/Volumes/MacApplications/Radar_papers/data/live/macos-recordings/"
                                f"{record_name}/csv/rotation.csv"
                            )
                        }
                    ]
                },
            }
        ],
    }


class ClosedCampaignObserverTests(unittest.TestCase):
    def test_failed_campaign_is_excluded(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            campaign = root / "buffered-v2"
            campaign.mkdir()
            (campaign / "campaign_manifest.json").write_text(
                json.dumps(
                    {
                        "status": "failed",
                        "children": [{"status": "complete", "session_id": "child"}],
                    }
                ),
                encoding="utf-8",
            )
            (root / "child").mkdir()
            self.assertEqual(campaign_children(root, "buffered-v2"), [])

    def test_missing_csv_mirror_is_waiting(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            session = root / "session"
            session.mkdir()
            (session / "session_manifest.json").write_text(
                json.dumps(manifest("session", "recording")), encoding="utf-8"
            )
            (session / "raw_udp_payloads.jsonl").write_text('{"payload_b64":"AA=="}\n')
            ready, missing = mirror_ready(session)
            self.assertFalse(ready)
            self.assertTrue(missing)

    def test_completed_session_uses_manifest_cache(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "live" / "paper1-experiments"
            root.mkdir(parents=True)
            session = root / "legacy"
            session.mkdir()
            record = root.parent / "macos-recordings" / "recording" / "csv"
            record.mkdir(parents=True)
            (record / "rotation.csv").write_text("Angle\tEchoValues\n")
            (session / "session_manifest.json").write_text(
                json.dumps(manifest("legacy", "recording")), encoding="utf-8"
            )
            (session / "raw_udp_payloads.jsonl").write_text('{"payload_b64":"AA=="}\n')
            out = root / "out"
            result = {
                "session_id": "legacy",
                "manifest_sha256": sha256_file(session / "session_manifest.json"),
                "raw_sha256": sha256_file(session / "raw_udp_payloads.jsonl"),
                "source_sha256": "source-a",
                "raw_size": (session / "raw_udp_payloads.jsonl").stat().st_size,
                "raw_mtime_ns": (session / "raw_udp_payloads.jsonl").stat().st_mtime_ns,
                "status": "pass",
                "quality": {},
            }
            with (
                patch(
                    "scripts.observe_closed_campaigns.audit_session", return_value=result
                ) as audit,
                patch(
                    "scripts.observe_closed_campaigns.source_sha256",
                    side_effect=["source-a", "source-a", "source-b"],
                ),
            ):
                first = run_once(root, out, "future", ("legacy",))
                second = run_once(root, out, "future", ("legacy",))
                third = run_once(root, out, "future", ("legacy",))
            self.assertEqual(first["schema_version"], OBSERVER_VERSION)
            self.assertEqual(second["sessions"]["legacy"]["status"], "pass")
            self.assertEqual(third["sessions"]["legacy"]["status"], "pass")
            self.assertEqual(audit.call_count, 2)


if __name__ == "__main__":
    unittest.main()

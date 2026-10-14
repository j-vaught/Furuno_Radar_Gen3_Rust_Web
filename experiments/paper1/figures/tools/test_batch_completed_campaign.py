import json
import tempfile
import unittest
from pathlib import Path

from batch_completed_campaign import (
    analysis_cache_valid,
    complete_children,
    mapped_path,
    write_analysis_cache_marker,
)


class CompletedCampaignTests(unittest.TestCase):
    def test_only_validated_complete_children_are_selected(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            complete_dir = root / "complete"
            failed_dir = root / "failed"
            complete_dir.mkdir()
            failed_dir.mkdir()
            (complete_dir / "session_manifest.json").write_text(
                json.dumps({"status": "complete"}), encoding="utf-8"
            )
            (failed_dir / "session_manifest.json").write_text(
                json.dumps({"status": "failed"}), encoding="utf-8"
            )
            campaign = root / "campaign_manifest.json"
            campaign.write_text(
                json.dumps(
                    {
                        "families": ["gain_value", "doppler"],
                        "children": [
                            {
                                "family": "gain_value",
                                "session_id": "complete",
                                "manifest_path": str(complete_dir),
                                "status": "complete",
                                "validation": {"valid": True},
                            },
                            {
                                "family": "doppler",
                                "session_id": "failed",
                                "manifest_path": str(failed_dir),
                                "status": "failed",
                                "validation": {"valid": False},
                            },
                        ],
                    }
                ),
                encoding="utf-8",
            )
            complete, skipped = complete_children([campaign], root)
            self.assertEqual([item["family"] for item in complete], ["gain_value"])
            self.assertEqual([item["family"] for item in skipped], ["doppler"])

    def test_path_map_changes_only_the_declared_prefix(self) -> None:
        self.assertEqual(
            mapped_path(
                "/Volumes/MacApplications/data/a.csv", [("/Volumes/MacApplications", "/mnt/data")]
            ),
            "/mnt/data/data/a.csv",
        )
        self.assertEqual(
            mapped_path("/other/a.csv", [("/Volumes/MacApplications", "/mnt/data")]),
            "/other/a.csv",
        )

    def test_cache_requires_raw_replay_and_matching_hashes(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            session = root / "session"
            out = root / "analysis"
            session.mkdir()
            out.mkdir()
            (session / "raw_udp_payloads.jsonl").write_text('{"payload":"x"}\n', encoding="utf-8")
            (session / "events.jsonl").write_text('{"event":"record"}\n', encoding="utf-8")
            (out / "rotation_metrics.csv").write_text("csv_sha256\nabc\n", encoding="utf-8")
            (out / "raw_replay_summary.json").write_text('{"missing":false}\n', encoding="utf-8")
            write_analysis_cache_marker(out, session)
            self.assertTrue(analysis_cache_valid(out, session))
            (session / "raw_udp_payloads.jsonl").write_text(
                '{"payload":"changed"}\n', encoding="utf-8"
            )
            self.assertFalse(analysis_cache_valid(out, session))


if __name__ == "__main__":
    unittest.main()

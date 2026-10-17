import json
import tempfile
import unittest
from pathlib import Path

from scripts.run_indoor_campaign import (
    compare_readbacks,
    control_families,
    select_families,
    validate_child_manifest,
    write_json_atomic,
)


class IndoorCampaignTests(unittest.TestCase):
    @staticmethod
    def verified(settings: dict[str, int]) -> dict[str, object]:
        return {
            "confirmed": True,
            "fresh": True,
            "missing_fields": [],
            "settings": settings,
            "settings_readback": {
                "verified": True,
                "fields": {
                    name: {"verified": True, "value": value} for name, value in settings.items()
                },
            },
        }

    def test_control_families_are_unique_and_complete(self) -> None:
        plan = {
            "experiments": [
                {
                    "id": "E6_indoor_one_control_response",
                    "control_matrix": [
                        {"setting": "gain_value", "values": [0, 64]},
                        {"setting": "doppler", "values": [1, 2]},
                        {"setting": "gain_value", "values": [100]},
                    ],
                }
            ]
        }
        self.assertEqual(
            control_families(plan, "E6_indoor_one_control_response"),
            [
                "gain_value",
                "doppler",
            ],
        )
        self.assertEqual(
            select_families(
                ["range_index", "gain_value", "doppler"],
                ["doppler", "gain_value", "doppler"],
            ),
            ["doppler", "gain_value"],
        )

    def test_readback_validation_requires_all_fields_and_standby(self) -> None:
        initial = self.verified({"tx_state": 1, "range_index": 0, "gain_value": 64})
        final = self.verified({"tx_state": 1, "range_index": 0, "gain_value": 64})
        self.assertEqual(compare_readbacks(initial, final), (True, [], 1))
        mismatch = self.verified({"tx_state": 1, "range_index": 1, "gain_value": 64})
        self.assertEqual(compare_readbacks(initial, mismatch)[0], False)

    def test_cleanup_failure_cannot_be_reported_complete(self) -> None:
        manifest = {
            "status": "complete",
            "initial_device_readback": self.verified({"tx_state": 1, "gain_value": 64}),
            "final_baseline_restore": self.verified({"tx_state": 1, "gain_value": 64}),
            "cleanup_errors": ["recording stop failed"],
            "phases": [],
        }
        result = validate_child_manifest(manifest)
        self.assertFalse(result["valid"])
        self.assertIn("cleanup_errors", result["errors"])

    def test_stale_equal_readbacks_cannot_be_reported_complete(self) -> None:
        manifest = {
            "status": "complete",
            "initial_device_readback": {"settings": {"tx_state": 1, "gain_value": 64}},
            "final_baseline_restore": {"settings": {"tx_state": 1, "gain_value": 64}},
            "phases": [],
        }
        result = validate_child_manifest(manifest)
        self.assertFalse(result["valid"])
        self.assertIn("final_readback_not_confirmed_fresh_verified", result["errors"])

    def test_atomic_master_manifest_has_no_temporary_file(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "campaign_manifest.json"
            write_json_atomic(path, {"status": "running", "children": []})
            self.assertEqual(json.loads(path.read_text())["status"], "running")
            self.assertFalse(list(Path(tmp).glob("*.tmp")))


if __name__ == "__main__":
    unittest.main()

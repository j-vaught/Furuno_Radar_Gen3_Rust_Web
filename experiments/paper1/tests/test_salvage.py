import json
import tempfile
import unittest
from pathlib import Path

from scripts.salvage_characterization import salvage


class SalvageTests(unittest.TestCase):
    def test_salvage_preserves_failure_and_marks_old_ir_as_superseded(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            events_path = Path(tmp) / "events.jsonl"
            events = [
                {
                    "event": "setting_command",
                    "context": "near_stc:value:2",
                    "setting": "near_stc",
                    "requested": 2,
                    "http_status": 200,
                    "response": "ack",
                },
                {
                    "event": "settings_readback",
                    "response": {"settings": {"near_stc": 2}},
                },
                {
                    "event": "setting_command",
                    "context": "interference_rejection:value:True",
                    "setting": "interference_rejection",
                    "requested": True,
                    "http_status": 200,
                    "response": "ack",
                },
                {
                    "event": "settings_readback",
                    "response": {"settings": {"interference_rejection": False}},
                },
                {"event": "session_error", "error": "restoration mismatch"},
            ]
            events_path.write_text(
                "".join(json.dumps(event) + "\n" for event in events), encoding="utf-8"
            )
            report = salvage(events_path)
        self.assertEqual(report["status"], "failed_recovered")
        self.assertFalse(report["control_characterization"]["restore_complete"])
        outcomes = report["control_characterization"]["outcomes"]
        self.assertEqual(outcomes[0]["outcome"], "supported")
        self.assertEqual(outcomes[1]["outcome"], "superseded")
        self.assertEqual(report["error"], "restoration mismatch")


if __name__ == "__main__":
    unittest.main()

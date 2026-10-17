import hashlib
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from typing import Any
from unittest.mock import patch

from scripts import capture_runner


class ProvenanceTests(unittest.TestCase):
    def test_provenance_fails_closed_on_slow_git_status(self) -> None:
        calls: list[dict[str, object]] = []

        def fake_git(*args: Any, **kwargs: Any) -> subprocess.CompletedProcess[str]:
            calls.append(kwargs)
            if len(calls) == 1:
                return subprocess.CompletedProcess(args[0], 0, stdout="abc123\n", stderr="")
            raise subprocess.TimeoutExpired(args[0], kwargs["timeout"])

        with patch.object(capture_runner.subprocess, "run", side_effect=fake_git):
            result = capture_runner.provenance()

        self.assertEqual(len(calls), 2)
        self.assertEqual([call["timeout"] for call in calls], [60, 60])
        self.assertIsNone(result["git_commit"])
        self.assertIsNone(result["git_dirty"])
        self.assertEqual(result["files"], {})
        self.assertIn("TimeoutExpired", result["git_error"])

    def test_dry_run_records_effective_alternate_plan(self) -> None:
        source_root = Path(__file__).parents[1]
        canonical_plan = source_root / "plans/paper1_experiments.json"
        metadata = source_root / "metadata_template.json"

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            alternate_plan = root / "paper1-35s-plan.json"
            plan = json.loads(canonical_plan.read_text(encoding="utf-8"))
            for experiment in plan["experiments"]:
                for phase in experiment.get("phases", []):
                    if "record_s" in phase:
                        phase["record_s"] = 35
            alternate_plan.write_text(
                json.dumps(plan, indent=2) + "\n",
                encoding="utf-8",
            )
            output_root = root / "output"
            argv = [
                "capture_runner.py",
                "--plan",
                str(alternate_plan),
                "--experiment",
                "E8_readback_cadence_validation",
                "--metadata",
                str(metadata),
                "--output-root",
                str(output_root),
                "--session-id",
                "alternate-plan-provenance",
                "--dry-run",
            ]
            with patch.object(sys, "argv", argv):
                self.assertEqual(capture_runner.main(), 0)

            manifest = json.loads(
                (output_root / "alternate-plan-provenance" / "session_manifest.json").read_text(
                    encoding="utf-8"
                )
            )
            files = manifest["provenance"]["files"]
            planned_phases = manifest["phases"]
            self.assertEqual(len(planned_phases), 4)
            self.assertEqual(
                [phase["record_s"] for phase in planned_phases],
                [35, 35, 35, 35],
            )
            self.assertEqual(
                files["plan"]["sha256"],
                hashlib.sha256(alternate_plan.read_bytes()).hexdigest(),
            )
            self.assertEqual(files["plan"]["path"], str(alternate_plan.resolve()))
            self.assertEqual(files["canonical_plan"]["path"], str(canonical_plan))
            self.assertNotEqual(
                files["plan"]["sha256"],
                files["canonical_plan"]["sha256"],
            )


if __name__ == "__main__":
    unittest.main()

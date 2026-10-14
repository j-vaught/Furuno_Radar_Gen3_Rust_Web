import json
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from scripts import capture_runner


class _Thread:
    def join(self, timeout: float | None = None) -> None:
        del timeout


class RunnerSafetyTests(unittest.TestCase):
    def test_failure_after_initial_readback_restores_noncharacterization_session(self) -> None:
        calls: list[str] = []
        initial = {
            "confirmed": True,
            "fresh": True,
            "settings": {"tx_state": 1},
            "settings_readback": {"fields": {"tx_state": {"verified": True, "value": 1}}},
        }

        def fake_restore(*_args, **kwargs):
            calls.append(kwargs.get("context", _args[4] if len(_args) > 4 else ""))
            return initial

        with tempfile.TemporaryDirectory() as tmp:
            output = Path(tmp) / "session"
            argv = [
                "capture_runner.py",
                "--plan",
                str(Path(__file__).parents[1] / "plans/paper1_experiments.json"),
                "--experiment",
                "E0_reproducibility_readback",
                "--metadata",
                str(Path(__file__).parents[1] / "metadata_template.json"),
                "--output-root",
                str(output),
                "--session-id",
                "restore-after-failure",
            ]
            capture_runner.STOP.clear()
            with (
                patch.object(sys, "argv", argv),
                patch.object(capture_runner, "mounted_and_writable", return_value=(True, "test")),
                patch.object(capture_runner, "readback", return_value=initial),
                patch.object(
                    capture_runner,
                    "apply_phase",
                    side_effect=RuntimeError("phase failure after initial readback"),
                ),
                patch.object(capture_runner, "restore_settings_baseline", side_effect=fake_restore),
                patch.object(
                    capture_runner,
                    "http_request",
                    return_value=(200, "cleanup acknowledged"),
                ),
            ):
                with self.assertRaises(RuntimeError):
                    capture_runner.main()
        self.assertIn("campaign-final", calls)

    def test_exception_attempts_tx_and_recording_cleanup(self) -> None:
        calls: list[tuple[str, dict[str, str]]] = []

        def fake_request(_base: str, path: str, params=None, timeout: float = 8.0):
            del timeout
            calls.append((path, params or {}))
            if len(calls) == 1:
                raise RuntimeError("simulated controller failure")
            return 200, "cleanup acknowledged"

        with tempfile.TemporaryDirectory() as tmp:
            output = Path(tmp) / "session"
            argv = [
                "capture_runner.py",
                "--plan",
                str(Path(__file__).parents[1] / "plans/paper1_experiments.json"),
                "--experiment",
                "E0_reproducibility_readback",
                "--metadata",
                str(Path(__file__).parents[1] / "metadata_template.json"),
                "--output-root",
                str(output),
                "--session-id",
                "failure-check",
            ]
            capture_runner.STOP.clear()
            with (
                patch.object(sys, "argv", argv),
                patch.object(capture_runner, "mounted_and_writable", return_value=(True, "test")),
                patch.object(capture_runner, "receive_raw", return_value=(_Thread(), {})),
                patch.object(capture_runner, "http_request", side_effect=fake_request),
            ):
                with self.assertRaises(RuntimeError):
                    capture_runner.main()
            manifest_path = output / "failure-check" / "session_manifest.json"
            self.assertTrue(manifest_path.exists())
            manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
            self.assertEqual(manifest["status"], "failed")
        self.assertIn(("/toggle_echo", {"mode": "off"}), calls)
        self.assertIn(("/toggle_recording", {"mode": "stop"}), calls)


if __name__ == "__main__":
    unittest.main()

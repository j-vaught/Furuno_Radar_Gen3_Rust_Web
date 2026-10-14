import base64
import json
import tempfile
import unittest
from pathlib import Path
from typing import Any
from unittest.mock import patch

from scripts import capture_runner
from scripts.analyze_capture import decode_sweep_count, replay_raw
from scripts.capture_runner import (
    REQUIRED_READBACK_FIELDS,
    _valid_until_is_fresh,
    apply_phase,
    characterize_controls,
    expand_control_matrix,
    filter_by_characterization_report,
    merge_characterization_reports,
    parse_settings_response,
    readback,
    require_confirmed_readback,
    restore_verified_baseline,
)


class ToolkitTests(unittest.TestCase):
    def test_analyzer_encoding_three_copy_updates_repeat_strength(self) -> None:
        values, consumed = decode_sweep_count(bytes([18, 13]), 3, 7, [4, 8, 12, 16])
        self.assertEqual(values, [4, 8, 12, 16, 16, 16, 16])
        self.assertEqual(consumed, 4)

    def test_register_readback_preserves_device_fields(self) -> None:
        parsed = parse_settings_response(
            {
                "TX": "TX State: $N69,1,0,0,0,0,0",
                "RANGE": "Range: $N62,2,0,0",
                "PULSE": "Pulse: $N68,0,2,0,0,0",
                "GAIN": "Gain: $N63,0,128,0,80,0",
                "CLUTTER": "SEA: $N64,0,10,0,0,0 | RAIN: $N65,0,20,0,0,0,0",
                "ANTENNA": "Antenna: $N89,0,0",
                "STC": 'JSON:{"near":3,"middle":0,"far":1}',
                "MAINBANG": "Main Bang: $N83,4,0",
                "IR": "IR: $N67,0,0,2,0",
                "NOISE": "Noise: $N67,0,2,1,0",
                "HEIGHT": "Height: $N84,0,5,0",
                "BIRDMODE": "Bird Mode: $NED,1,0",
                "DOPPLER": "Doppler: $NEF,2,0,0",
            }
        )
        self.assertTrue(parsed["confirmed"])
        self.assertEqual(parsed["range_index"], 2)
        self.assertEqual(parsed["gain_value"], 128)
        self.assertEqual(parsed["near_stc"], 3)
        self.assertTrue(parsed["interference_rejection"])
        self.assertEqual(parsed["interference_rejection_level"], 2)
        self.assertTrue(parsed["noise_reduction"])
        self.assertEqual(parsed["antenna_height_raw"], 5)

    def test_gain_mode_toggle_preserves_current_hidden_values(self) -> None:
        path, false_params = capture_runner.setting_requests(
            "gain_auto",
            False,
            {"gain_manual_value": 12, "gain_auto_value": 80, "gain_value": 80},
        )
        self.assertEqual(path, "/set_gain")
        self.assertEqual(false_params["manualValue"], 12)
        self.assertEqual(false_params["autoValue"], 80)
        _, true_params = capture_runner.setting_requests(
            "gain_auto",
            True,
            {"gain_manual_value": 12, "gain_auto_value": 80, "gain_value": 80},
        )
        self.assertEqual(true_params["manualValue"], 12)
        self.assertEqual(true_params["autoValue"], 80)

    def test_noise_and_height_use_standby_setting_routes(self) -> None:
        path, params = capture_runner.setting_requests("noise_reduction", True, {})
        self.assertEqual((path, params), ("/set_noise_reduction", {"enabled": 1}))
        path, params = capture_runner.setting_requests("antenna_height_raw", 5, {})
        self.assertEqual((path, params), ("/set_antenna_height", {"value": 5}))

    def test_raw_replay_detects_complete_packet(self) -> None:
        # One encoding-0 packet. Header declares one 4-byte sweep.
        payload = bytes([2, 0, 0, 0, 0, 0, 0, 0, 0, 2, 4, 0, 0, 0, 0, 0, 7, 0, 0, 0, 1, 2, 3, 4])
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "raw.jsonl"
            path.write_text(
                json.dumps(
                    {"received_unix_ns": 1, "payload_b64": base64.b64encode(payload).decode()}
                )
                + "\n",
                encoding="utf-8",
            )
            summary = replay_raw(path)
        self.assertEqual(summary["packet_count"], 1)
        self.assertEqual(summary["decoded_spoke_count"], 1)
        self.assertEqual(summary["truncated_sweep_count"], 0)
        self.assertTrue(summary["timestamp_monotonic"])

    def test_raw_replay_separates_spoke_start_truncation(self) -> None:
        payload = bytes([2, 0, 0, 0, 0, 0, 0, 0, 0, 4, 4, 0, 0, 0, 0, 0, 7, 0, 0, 0, 1, 2, 3, 4])
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "raw.jsonl"
            path.write_text(
                json.dumps(
                    {"received_unix_ns": 1, "payload_b64": base64.b64encode(payload).decode()}
                )
                + "\n",
                encoding="utf-8",
            )
            summary = replay_raw(path)
        self.assertEqual(summary["decoded_spoke_count"], 1)
        self.assertEqual(summary["spoke_start_truncation_count"], 1)
        self.assertEqual(summary["sweep_payload_truncation_count"], 0)

    def test_partial_readback_is_refused(self) -> None:
        with self.assertRaises(RuntimeError):
            require_confirmed_readback(
                {"confirmed": False, "missing_fields": ["gain_auto_value"]},
                "mock partial snapshot",
            )

    def test_readback_ttl_is_checked(self) -> None:
        import time

        now_ms = int(time.time() * 1000)
        self.assertTrue(_valid_until_is_fresh({"valid_until": f"unix_ms:{now_ms + 5000}"}))
        self.assertFalse(_valid_until_is_fresh({"valid_until": f"unix_ms:{now_ms - 1}"}))

    def test_readback_forces_fresh_aggregate_query(self) -> None:
        import time

        calls = []
        values = {field: 0 for field in REQUIRED_READBACK_FIELDS}
        values["tx_state"] = 1

        def fake_request(_base, path, params=None, timeout=8.0):
            del timeout
            calls.append((path, params))
            fields = {
                name: {
                    "verified": True,
                    "value": value,
                    "register": "$N00",
                    "raw": "raw",
                    "read_at": "unix_ms:1",
                    "error": None,
                }
                for name, value in values.items()
            }
            body = json.dumps(
                {
                    "connected": True,
                    "settings_readback": {
                        "verified": True,
                        "transition_id": 9,
                        "captured_at": "unix_ms:1",
                        "valid_until": f"unix_ms:{int(time.time() * 1000) + 5000}",
                        "source": "api_fresh",
                        "fields": fields,
                        "raw_register_responses": [],
                    },
                }
            )
            return 200, body

        with (
            tempfile.TemporaryDirectory() as tmp,
            patch("scripts.capture_runner.http_request", fake_request),
        ):
            with (Path(tmp) / "events.jsonl").open("w", encoding="utf-8") as fp:
                result = readback("mock://controller", fp, 1.0, fresh_required=True)
        self.assertEqual(calls[0][0], "/get_current")
        self.assertEqual(calls[0][1]["command"], "ALL")
        self.assertEqual(calls[0][1]["fresh"], 1)
        self.assertTrue(result["confirmed"])

    def test_response_matrix_filters_to_characterized_controls(self) -> None:
        experiment = {
            "phases": [{"id": "reference", "settings": {}}],
            "control_matrix": [
                {"setting": "near_stc", "values": [0, 2]},
                {"setting": "bird_mode", "values": [1]},
            ],
        }
        expanded = expand_control_matrix(experiment)
        filtered, skipped = filter_by_characterization_report(
            expanded,
            {
                "control_characterization": {
                    "outcomes": [{"setting": "near_stc", "requested": 2, "outcome": "supported"}]
                }
            },
        )
        self.assertEqual(
            [phase["id"] for phase in filtered["phases"]],
            ["reference", "near_stc_2", "reference"],
        )
        self.assertEqual(len(skipped), 2)

    def test_response_matrix_skips_clamped_and_marks_deferred(self) -> None:
        experiment = {
            "phases": [{"id": "reference", "settings": {}}],
            "control_matrix": [
                {"setting": "near_stc", "values": [2, 3]},
                {"setting": "bird_mode", "values": [1]},
            ],
        }
        expanded = expand_control_matrix(experiment)
        filtered, skipped = filter_by_characterization_report(
            expanded,
            {
                "control_characterization": {
                    "outcomes": [
                        {
                            "setting": "near_stc",
                            "requested": 2,
                            "actual": 1,
                            "outcome": "clamped",
                        },
                        {
                            "setting": "near_stc",
                            "requested": 3,
                            "actual": 2,
                            "outcome": "deferred",
                        },
                    ]
                }
            },
        )
        self.assertEqual(
            [phase["id"] for phase in filtered["phases"]],
            ["reference", "near_stc_3", "reference"],
        )
        self.assertEqual(filtered["phases"][1]["characterization_deferred_fields"], ["near_stc"])
        self.assertEqual(len(skipped), 2)

    def test_inactive_auto_register_writes_preserve_mode(self) -> None:
        _path, gain_params = capture_runner.setting_requests(
            "gain_auto_value", 40, {"gain_auto": False, "gain_manual_value": 90}
        )
        _path, sea_params = capture_runner.setting_requests(
            "sea_clutter_auto_value", 40, {"sea_clutter_auto": False}
        )
        _path, rain_params = capture_runner.setting_requests(
            "rain_clutter_auto_value", 40, {"rain_clutter_auto": False}
        )
        self.assertEqual(gain_params["status"], 0)
        self.assertEqual(sea_params["status"], 0)
        self.assertEqual(rain_params["status"], 0)

    def test_apply_phase_groups_register_payloads_and_skips_equal_groups(self) -> None:
        state = {field: 0 for field in REQUIRED_READBACK_FIELDS}
        state.update(
            {
                "tx_state": 1,
                "gain_auto": False,
                "gain_value": 20,
                "gain_manual_value": 20,
                "gain_auto_value": 80,
                "sea_clutter_auto": False,
                "sea_clutter": 0,
                "sea_clutter_auto_value": 0,
                "sea_clutter_mode": 0,
                "rain_clutter_auto": False,
                "rain_clutter": 0,
                "rain_clutter_auto_value": 0,
                "rain_clutter_ftc_mode": 0,
                "rain_clutter_mode": 0,
                "antenna_speed": 0,
                "antenna_hi_rpm": False,
            }
        )
        requests: list[tuple[str, dict[str, Any]]] = []

        def fake_readback(_base, _fp, _timeout, fresh_required=False):
            self.assertTrue(fresh_required)
            return {"confirmed": True, "fresh": True, "settings": dict(state)}

        def fake_http(_base, path, params, _timeout):
            requests.append((path, dict(params)))
            return 200, "ACK"

        phase = {
            "id": "grouped",
            "settings": {
                "gain_auto": False,
                "gain_value": 64,
                "sea_clutter_auto": False,
                "sea_clutter": 50,
                "rain_clutter_auto": False,
                "rain_clutter": 50,
                "antenna_speed": 1,
                "antenna_hi_rpm": True,
            },
        }
        with (
            tempfile.TemporaryDirectory() as tmp,
            patch("scripts.capture_runner.readback", fake_readback),
            patch("scripts.capture_runner.http_request", fake_http),
        ):
            with (Path(tmp) / "events.jsonl").open("w", encoding="utf-8") as fp:
                events = apply_phase("mock://controller", phase, fp, 1.0)
        self.assertEqual(
            [path for path, _params in requests],
            ["/set_gain", "/set_acsea", "/set_acrain", "/set_antenna_revolution"],
        )
        self.assertEqual(requests[0][1]["manualValue"], 64)
        self.assertEqual(requests[1][1]["manual"], 50)
        self.assertEqual(requests[2][1]["manual"], 50)
        self.assertEqual(requests[3][1], {"rollSpeed": 1, "hiRpm": 1})
        self.assertEqual(len(events), 4)

        state.update(
            {
                "gain_value": 64,
                "gain_manual_value": 64,
                "sea_clutter": 50,
                "rain_clutter": 50,
                "antenna_speed": 1,
                "antenna_hi_rpm": True,
            }
        )
        requests.clear()
        with (
            tempfile.TemporaryDirectory() as tmp,
            patch("scripts.capture_runner.readback", fake_readback),
            patch("scripts.capture_runner.http_request", fake_http),
        ):
            with (Path(tmp) / "events.jsonl").open("w", encoding="utf-8") as fp:
                events = apply_phase("mock://controller", phase, fp, 1.0)
        self.assertEqual(requests, [])
        self.assertTrue(all(event["skipped"] for event in events))

    def test_response_conditions_include_reference_settings(self) -> None:
        experiment = {
            "phases": [
                {
                    "id": "reference",
                    "settings": {"gain_auto": True, "range_index": 2},
                }
            ],
            "control_matrix": [
                {
                    "setting": "near_stc",
                    "values": [3],
                    "fixed_settings": {"middle_stc": 1},
                }
            ],
        }
        expanded = expand_control_matrix(experiment)
        condition = expanded["phases"][1]
        self.assertEqual(
            condition["settings"],
            {
                "gain_auto": True,
                "range_index": 2,
                "middle_stc": 1,
                "near_stc": 3,
            },
        )

    def test_characterization_merge_uses_latest_corrected_outcome(self) -> None:
        merged = merge_characterization_reports(
            [
                (
                    "original.json",
                    {
                        "session_id": "original",
                        "control_characterization": {
                            "outcomes": [
                                {
                                    "setting": "interference_rejection",
                                    "requested": True,
                                    "outcome": "deferred",
                                }
                            ]
                        },
                    },
                ),
                (
                    "corrected.json",
                    {
                        "session_id": "corrected",
                        "provenance": {"git_commit": "corrected-build"},
                        "control_characterization": {
                            "outcomes": [
                                {
                                    "setting": "interference_rejection",
                                    "requested": True,
                                    "outcome": "supported",
                                }
                            ]
                        },
                    },
                ),
            ]
        )
        outcomes = merged["control_characterization"]["outcomes"]
        self.assertEqual(len(outcomes), 1)
        self.assertEqual(outcomes[0]["outcome"], "supported")
        self.assertEqual(len(merged["control_characterization"]["outcomes_all_sources"]), 2)
        self.assertEqual(merged["sources"][1]["provenance"]["git_commit"], "corrected-build")

    def test_characterization_restores_baseline(self) -> None:
        baseline_values = {field: 0 for field in REQUIRED_READBACK_FIELDS}
        baseline_values.update(
            {
                "tx_state": 1,
                "range_index": 0,
                "gain_auto": False,
                "sea_clutter_auto": False,
                "rain_clutter_auto": False,
                "antenna_hi_rpm": False,
                "interference_rejection": False,
            }
        )
        state = dict(baseline_values)

        def fake_readback(_base, _fp, _timeout, fresh_required=False):
            del fresh_required
            return {
                "confirmed": True,
                "fresh": True,
                "timestamp_utc": "2026-10-14T00:00:00Z",
                "settings": dict(state),
                "settings_readback": {"transition_id": 7},
            }

        def fake_issue(_base, setting, value, _settings, _fp, _timeout, context):
            state[setting] = value
            return {
                "setting": setting,
                "requested": value,
                "http_status": 200,
                "response": "ok",
                "context": context,
                "command_time_utc": "2026-10-14T00:00:00Z",
                "ack_time_utc": "2026-10-14T00:00:01Z",
            }

        spec = {
            "controls": [
                {"id": "near", "setting": "near_stc", "values": [2]},
            ]
        }
        with (
            tempfile.TemporaryDirectory() as tmp,
            patch("scripts.capture_runner.readback", fake_readback),
            patch("scripts.capture_runner.issue_setting", fake_issue),
        ):
            capture_runner.STOP.clear()
            with (Path(tmp) / "events.jsonl").open("w", encoding="utf-8") as fp:
                result = characterize_controls("mock://controller", spec, fp, 1.0)
        self.assertTrue(result["restore_complete"])
        self.assertEqual(state, baseline_values)

    def test_mid_probe_failure_still_restores_mutated_state(self) -> None:
        baseline_values = {field: 0 for field in REQUIRED_READBACK_FIELDS}
        baseline_values.update(
            {
                "tx_state": 1,
                "range_index": 0,
                "gain_auto": False,
                "sea_clutter_auto": False,
                "rain_clutter_auto": False,
                "antenna_hi_rpm": False,
                "interference_rejection": False,
            }
        )
        state = dict(baseline_values)
        calls = 0

        def fake_readback(_base, _fp, _timeout, fresh_required=False):
            del fresh_required
            return {
                "confirmed": True,
                "fresh": True,
                "timestamp_utc": "2026-10-14T00:00:00Z",
                "settings": dict(state),
                "settings_readback": {"transition_id": 8},
            }

        def failing_issue(_base, setting, value, _settings, _fp, _timeout, context):
            nonlocal calls
            calls += 1
            state[setting] = value
            if calls == 2:
                raise RuntimeError("injected probe failure after device mutation")
            return {
                "setting": setting,
                "requested": value,
                "http_status": 200,
                "response": "ok",
                "context": context,
                "command_time_utc": "2026-10-14T00:00:00Z",
                "ack_time_utc": "2026-10-14T00:00:01Z",
            }

        spec = {"controls": [{"id": "near", "setting": "near_stc", "values": [2, 3]}]}
        with (
            tempfile.TemporaryDirectory() as tmp,
            patch("scripts.capture_runner.readback", fake_readback),
            patch("scripts.capture_runner.issue_setting", failing_issue),
        ):
            capture_runner.STOP.clear()
            with (Path(tmp) / "events.jsonl").open("w", encoding="utf-8") as fp:
                result = characterize_controls("mock://controller", spec, fp, 1.0)
        self.assertIn("injected probe failure", result["error"])
        self.assertTrue(result["restore_complete"])
        self.assertEqual(state, baseline_values)

    def test_baseline_restore_reads_freshly_before_each_write(self) -> None:
        baseline_values = {field: 0 for field in REQUIRED_READBACK_FIELDS}
        baseline_values.update(
            {
                "tx_state": 1,
                "range_index": 0,
                "gain_auto": True,
                "gain_manual_value": 7,
                "gain_auto_value": 80,
                "gain_value": 80,
                "sea_clutter_auto": False,
                "rain_clutter_auto": False,
                "antenna_hi_rpm": True,
                "interference_rejection": False,
            }
        )
        state = dict(baseline_values)
        readback_count = 0
        commands: list[tuple[str, Any, dict[str, Any], dict[str, Any]]] = []

        def fake_readback(_base, _fp, _timeout, fresh_required=False):
            nonlocal readback_count
            self.assertTrue(fresh_required)
            readback_count += 1
            fields = {name: {"verified": True, "value": value} for name, value in state.items()}
            return {
                "confirmed": True,
                "fresh": True,
                "settings": dict(state),
                "settings_readback": {"fields": fields},
            }

        def fake_issue(_base, setting, value, settings, _fp, _timeout, context):
            del context
            _path, params = capture_runner.setting_requests(setting, value, settings)
            commands.append((setting, value, dict(settings), params))
            if setting == "gain_auto":
                state["gain_auto"] = bool(value)
                state["gain_value"] = (
                    state["gain_auto_value"] if bool(value) else state["gain_manual_value"]
                )
            elif setting in {"gain_value", "gain_auto_value"}:
                state[setting] = value
                if setting == "gain_value":
                    state["gain_manual_value"] = value
            elif setting == "antenna_hi_rpm":
                state["antenna_hi_rpm"] = bool(value)
            else:
                state[setting] = value
            return {"http_status": 200, "response": "ok", "setting": setting}

        with (
            tempfile.TemporaryDirectory() as tmp,
            patch("scripts.capture_runner.readback", fake_readback),
            patch("scripts.capture_runner.issue_setting", fake_issue),
        ):
            with (Path(tmp) / "events.jsonl").open("w", encoding="utf-8") as fp:
                report = restore_verified_baseline(
                    "mock://controller", {"settings": baseline_values}, fp, 1.0, "fresh-test"
                )
        self.assertTrue(report["restore_complete"])
        self.assertGreaterEqual(readback_count, len(commands) + 1)
        # Payload values come from the baseline, even after the fake device has
        # moved through the opposite gain mode during staging.
        gain_payloads = [
            payload for setting, _value, payload, _params in commands if setting == "gain_auto"
        ]
        self.assertTrue(gain_payloads)
        self.assertTrue(all(payload["gain_value"] == 80 for payload in gain_payloads))
        gain_manual_commands = [
            params for setting, _value, _payload, params in commands if setting == "gain_value"
        ]
        self.assertEqual(gain_manual_commands[0]["status"], 0)
        sea_manual_commands = [
            params for setting, _value, _payload, params in commands if setting == "sea_clutter"
        ]
        rain_manual_commands = [
            params for setting, _value, _payload, params in commands if setting == "rain_clutter"
        ]
        self.assertEqual(sea_manual_commands[0]["status"], 0)
        self.assertEqual(rain_manual_commands[0]["status"], 0)

    def test_baseline_restore_skips_gain_when_mode_field_is_unverified(self) -> None:
        baseline_values = {field: 0 for field in REQUIRED_READBACK_FIELDS}
        baseline_values.update(
            {
                "tx_state": 1,
                "range_index": 0,
                "gain_auto": True,
                "gain_manual_value": 7,
                "gain_auto_value": 80,
                "gain_value": 80,
                "sea_clutter_auto": False,
                "rain_clutter_auto": False,
                "antenna_hi_rpm": False,
                "interference_rejection": False,
            }
        )
        commands: list[str] = []

        def fake_readback(_base, _fp, _timeout, fresh_required=False):
            del fresh_required
            fields = {
                name: {"verified": name != "gain_auto", "value": value}
                for name, value in baseline_values.items()
            }
            return {
                "confirmed": False,
                "fresh": True,
                "settings": dict(baseline_values),
                "settings_readback": {"fields": fields},
            }

        def fake_issue(_base, setting, value, _settings, _fp, _timeout, _context):
            del value
            commands.append(setting)
            return {"http_status": 200, "response": "ok", "setting": setting}

        with (
            tempfile.TemporaryDirectory() as tmp,
            patch("scripts.capture_runner.readback", fake_readback),
            patch("scripts.capture_runner.issue_setting", fake_issue),
        ):
            with (Path(tmp) / "events.jsonl").open("w", encoding="utf-8") as fp:
                report = restore_verified_baseline(
                    "mock://controller", {"settings": baseline_values}, fp, 1.0, "partial-test"
                )
        self.assertFalse(report["restore_complete"])
        self.assertNotIn("gain_auto", commands)
        self.assertTrue(
            any(item.get("setting") == "gain_auto" for item in report["restore_outcomes"])
        )


if __name__ == "__main__":
    unittest.main()

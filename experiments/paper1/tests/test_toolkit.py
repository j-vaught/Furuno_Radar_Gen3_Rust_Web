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
    expand_phase_repeats,
    filter_by_characterization_report,
    inspect_recording_output,
    merge_characterization_reports,
    parse_settings_response,
    parse_system_info,
    readback,
    readback_with_retry,
    require_confirmed_readback,
    require_system_info_settings,
    require_transmitting_readback,
    restore_verified_baseline,
    select_control_family,
    write_manifest_atomic,
)


class ToolkitTests(unittest.TestCase):
    def test_atomic_manifest_checkpoint_replaces_previous_file(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "session_manifest.json"
            write_manifest_atomic(path, {"status": "running", "phase_index": 2})
            self.assertEqual(json.loads(path.read_text())["phase_index"], 2)
            self.assertFalse(list(Path(tmp).glob("*.tmp")))

    def test_control_family_selection_keeps_reference_matrix_context(self) -> None:
        experiment = {
            "phases": [{"id": "reference", "settings": {"range_index": 0}}],
            "control_matrix": [
                {"setting": "gain_value", "values": [0, 64]},
                {"setting": "doppler", "values": [1, 2]},
            ],
        }
        selected = select_control_family(experiment, "doppler")
        expanded = expand_control_matrix(selected)
        self.assertEqual(selected["selected_control_family"], "doppler")
        self.assertEqual(
            [phase["id"] for phase in expanded["phases"]],
            ["reference", "doppler_1", "reference", "doppler_2", "reference"],
        )
        self.assertTrue(all(phase["settings"]["range_index"] == 0 for phase in expanded["phases"]))

    def test_system_info_json_error_null_is_not_a_failure(self) -> None:
        info = parse_system_info(
            200,
            json.dumps({"recording": True, "connected": True, "error": None}),
            "recording status",
        )
        self.assertTrue(info["recording"])
        self.assertIsNone(info["error"])

    def test_system_info_rejects_http_failure_without_dumping_payload(self) -> None:
        with self.assertRaisesRegex(RuntimeError, "HTTP 500"):
            parse_system_info(500, '{"error":null}', "recording status")

    def test_restore_readback_retry_recovers_transient_failure(self) -> None:
        responses = iter(
            [
                {"confirmed": False, "fresh": False, "missing_fields": ["near_stc"]},
                {"confirmed": True, "fresh": True, "settings": {"near_stc": 3}},
            ]
        )
        with (
            tempfile.TemporaryDirectory() as tmp,
            patch(
                "scripts.capture_runner.readback",
                side_effect=lambda *args, **kwargs: next(responses),
            ),
            (Path(tmp) / "events.jsonl").open("w", encoding="utf-8") as events,
        ):
            result, failures = readback_with_retry(
                "mock://controller", events, 1.0, "retry-test", max_attempts=2, delay_s=0
            )
        self.assertTrue(result["confirmed"])
        self.assertEqual(len(failures), 1)

    def test_system_info_rejects_unverified_or_drifted_settings(self) -> None:
        requested = {"gain_auto": True, "gain_value": 80}
        with self.assertRaisesRegex(RuntimeError, "settings_verified"):
            require_system_info_settings(
                {"settings_verified": False, "settings": requested}, requested, "poll"
            )
        with self.assertRaisesRegex(RuntimeError, "drifted"):
            require_system_info_settings(
                {
                    "settings_verified": True,
                    "settings": {"gain_auto": True, "gain_value": 64},
                },
                requested,
                "poll",
            )
        self.assertEqual(
            require_system_info_settings(
                {"settings_verified": True, "settings": requested}, requested, "poll"
            ),
            requested,
        )

    def test_recording_output_requires_verified_furuno_metadata(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            record_dir = Path(tmp) / "recording"
            (record_dir / "csv").mkdir(parents=True)
            (record_dir / "settings.json").write_text(
                json.dumps({"schema_version": "furuno-web-2"}), encoding="utf-8"
            )
            csv_header = (
                "# schema_version,furuno-web-2\n"
                "# settings_verified,true\n"
                "# recording_started_at,unix_ms:1\n"
                "# recording_stopped_at,unix_ms:2\n"
                "angle_deg,echoes\n"
                "0,0\n"
            )
            for index in range(2):
                (record_dir / "csv" / f"rotation_{index}.csv").write_text(
                    csv_header, encoding="utf-8"
                )
            result = inspect_recording_output(record_dir, 2)
            self.assertEqual(result["csv_count"], 2)
            self.assertTrue(all(item["settings_verified"] for item in result["csv_files"]))

    def test_transmitting_readback_rejects_standby_state(self) -> None:
        standby = {
            "confirmed": True,
            "fresh": True,
            "settings": {"tx_state": 1},
            "settings_readback": {"fields": {"tx_state": {"verified": True, "value": 1}}},
        }
        with self.assertRaises(RuntimeError):
            require_transmitting_readback(standby, "test TX gate")

    def test_phase_repeats_expand_with_unique_provenance_ids(self) -> None:
        expanded = expand_phase_repeats(
            {
                "phases": [
                    {"id": "reference", "settings": {}, "repeat": 2},
                    {"id": "reference", "settings": {}},
                ]
            }
        )
        self.assertEqual(
            [phase["id"] for phase in expanded["phases"]],
            ["reference__r01", "reference__r02", "reference__p003"],
        )
        self.assertEqual(expanded["phases"][1]["repeat_index"], 1)
        self.assertEqual(expanded["phases"][2]["source_phase_id"], "reference")
        self.assertEqual([phase["phase_index"] for phase in expanded["phases"]], [0, 1, 2])

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

    def test_paired_doppler_references_survive_characterization_filter(self) -> None:
        experiment = select_control_family(
            {
                "phases": [{"id": "reference", "settings": {}}],
                "control_matrix": [
                    {
                        "setting": "doppler",
                        "context_id": "rain_manual_modes_0_1",
                        "values": [0, 1],
                        "reference_settings": {"rain_clutter_auto": False},
                    },
                    {
                        "setting": "doppler",
                        "context_id": "rain_auto_modes_0_2",
                        "values": [0, 2],
                        "reference_settings": {"rain_clutter_auto": True},
                    },
                ],
            },
            "doppler",
        )
        expanded = expand_control_matrix(experiment)
        filtered, skipped = filter_by_characterization_report(
            expanded,
            {
                "control_characterization": {
                    "outcomes": [
                        {"setting": "doppler", "requested": 0, "outcome": "supported"},
                        {"setting": "doppler", "requested": 1, "outcome": "supported"},
                        {"setting": "doppler", "requested": 2, "outcome": "supported"},
                    ]
                }
            },
        )
        self.assertEqual(len(filtered["phases"]), 10)
        self.assertEqual(len(skipped), 0)
        references = [
            phase for phase in filtered["phases"] if phase.get("phase_role") == "reference"
        ]
        self.assertEqual(len(references), 6)
        self.assertEqual(
            {phase["reference_context"] for phase in references},
            {"rain_manual_modes_0_1", "rain_auto_modes_0_2"},
        )

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

    def test_doppler_is_applied_before_mode_sensitive_registers(self) -> None:
        state = {field: 0 for field in REQUIRED_READBACK_FIELDS}
        state.update(
            {
                "tx_state": 1,
                "doppler": 1,
                "gain_auto": True,
                "gain_value": 80,
                "gain_manual_value": 0,
                "gain_auto_value": 80,
                "sea_clutter_auto": True,
                "sea_clutter": 0,
                "sea_clutter_auto_value": 10,
                "rain_clutter_auto": True,
                "rain_clutter": 0,
                "rain_clutter_auto_value": 0,
                "rain_clutter_ftc_mode": 0,
                "rain_clutter_mode": 0,
                "sea_clutter_mode": 0,
            }
        )
        requests: list[str] = []

        def fake_readback(_base, _fp, _timeout, fresh_required=False):
            self.assertTrue(fresh_required)
            return {"confirmed": True, "fresh": True, "settings": dict(state)}

        def fake_http(_base, path, params, _timeout):
            requests.append(path)
            if path == "/set_doppler":
                state["doppler"] = params["mode"]
                state["gain_auto"] = False
                state["gain_value"] = 64
                state["gain_manual_value"] = 0
                state["sea_clutter_auto"] = False
                state["sea_clutter"] = 4
                if params["mode"] == 2:
                    state["rain_clutter_auto"] = False
                    state["rain_clutter"] = 3
            elif path == "/set_gain":
                state["gain_auto"] = bool(params["status"])
                state["gain_manual_value"] = params["manualValue"]
                state["gain_auto_value"] = params["autoValue"]
                state["gain_value"] = (
                    state["gain_auto_value"] if state["gain_auto"] else state["gain_manual_value"]
                )
            elif path == "/set_acsea":
                state["sea_clutter_auto"] = bool(params["status"])
                state["sea_clutter"] = params["manual"]
                state["sea_clutter_auto_value"] = params["auto"]
                state["sea_clutter_mode"] = params["mode"]
            return 200, "ACK"

        phase = {
            "id": "doppler-mode",
            "settings": {
                "doppler": 2,
                "gain_auto": True,
                "gain_value": 80,
                "gain_manual_value": 64,
                "gain_auto_value": 80,
                "sea_clutter_auto": True,
                "sea_clutter": 0,
                "sea_clutter_auto_value": 10,
                "rain_clutter_auto": True,
                "rain_clutter": 0,
                "rain_clutter_auto_value": 0,
            },
        }
        with (
            tempfile.TemporaryDirectory() as tmp,
            patch("scripts.capture_runner.readback", fake_readback),
            patch("scripts.capture_runner.http_request", fake_http),
        ):
            with (Path(tmp) / "events.jsonl").open("w", encoding="utf-8") as fp:
                apply_phase("mock://controller", phase, fp, 1.0)
        self.assertEqual(
            requests,
            ["/set_doppler", "/set_gain", "/set_gain", "/set_acsea", "/set_acrain"],
        )
        self.assertEqual(state["gain_manual_value"], 64)
        self.assertEqual(state["gain_auto_value"], 80)
        self.assertTrue(state["gain_auto"])

    def test_doppler_restore_neutralizes_before_gain_and_sea(self) -> None:
        target = {field: 0 for field in REQUIRED_READBACK_FIELDS}
        target.update(
            {
                "tx_state": 1,
                "doppler": 0,
                "gain_auto": True,
                "gain_value": 80,
                "gain_manual_value": 0,
                "gain_auto_value": 80,
                "sea_clutter_auto": True,
                "sea_clutter": 0,
                "sea_clutter_auto_value": 10,
            }
        )
        state = dict(target)
        state.update(
            {
                "doppler": 1,
                "gain_auto": False,
                "gain_value": 64,
                "gain_manual_value": 64,
                "sea_clutter_auto": False,
                "sea_clutter": 4,
                "rain_clutter_auto": False,
                "rain_clutter": 3,
            }
        )
        commands: list[str] = []

        def fake_readback(_base, _fp, _timeout, fresh_required=False):
            return {"confirmed": True, "fresh": True, "settings": dict(state)}

        def fake_issue(_base, setting, value, settings, _fp, _timeout, _context):
            commands.append(setting)
            if setting == "doppler":
                state["doppler"] = value
                state["gain_auto"] = False
                state["gain_value"] = 64
                state["gain_manual_value"] = 64
                state["sea_clutter_auto"] = False
                state["sea_clutter"] = 4
            elif setting == "gain_auto":
                _path, params = capture_runner.setting_requests(setting, value, settings)
                state["gain_auto"] = bool(value)
                state["gain_manual_value"] = params["manualValue"]
                state["gain_auto_value"] = params["autoValue"]
                state["gain_value"] = params["autoValue"] if value else params["manualValue"]
            elif setting == "sea_clutter_auto":
                _path, params = capture_runner.setting_requests(setting, value, settings)
                state["sea_clutter_auto"] = bool(value)
                state["sea_clutter"] = params["manual"]
                state["sea_clutter_auto_value"] = params["auto"]
                state["sea_clutter_mode"] = params["mode"]
            elif setting == "rain_clutter_auto":
                _path, params = capture_runner.setting_requests(setting, value, settings)
                state["rain_clutter_auto"] = bool(value)
                state["rain_clutter"] = params["manual"]
                state["rain_clutter_auto_value"] = params["auto"]
                state["rain_clutter_ftc_mode"] = params["ftcMode"]
                state["rain_clutter_mode"] = params["rainMode"]
            return {"http_status": 200, "response": "ACK"}

        with (
            tempfile.TemporaryDirectory() as tmp,
            patch("scripts.capture_runner.readback", fake_readback),
            patch("scripts.capture_runner.issue_setting", fake_issue),
        ):
            with (Path(tmp) / "events.jsonl").open("w", encoding="utf-8") as fp:
                report = restore_verified_baseline(
                    "mock://controller", {"settings": target}, fp, 1.0, "doppler-restore"
                )
        self.assertTrue(report["restore_complete"])
        self.assertEqual(commands[:3], ["doppler", "gain_auto", "sea_clutter_auto"])

    def test_doppler_family_uses_common_mode_context(self) -> None:
        selected = select_control_family(
            {
                "phases": [{"id": "reference", "settings": {"gain_auto": False}}],
                "control_matrix": [{"setting": "doppler", "values": [0, 1, 2]}],
            },
            "doppler",
        )
        context = selected["phases"][0]["settings"]
        self.assertEqual(
            {
                key: context[key]
                for key in (
                    "gain_auto",
                    "gain_value",
                    "gain_auto_value",
                    "sea_clutter_auto",
                    "sea_clutter_auto_value",
                    "sea_clutter",
                    "rain_clutter_auto",
                    "rain_clutter_auto_value",
                    "rain_clutter",
                )
            },
            {
                "gain_auto": True,
                "gain_value": 80,
                "gain_auto_value": 80,
                "sea_clutter_auto": True,
                "sea_clutter_auto_value": 10,
                "sea_clutter": 0,
                "rain_clutter_auto": True,
                "rain_clutter_auto_value": 0,
                "rain_clutter": 0,
            },
        )

    def test_doppler_family_keeps_paired_rain_contexts(self) -> None:
        selected = select_control_family(
            {
                "phases": [{"id": "reference", "settings": {}}],
                "control_matrix": [
                    {
                        "setting": "doppler",
                        "context_id": "rain_manual_modes_0_1",
                        "values": [0, 1],
                        "reference_settings": {
                            "rain_clutter_auto": False,
                            "rain_clutter": 0,
                            "rain_clutter_auto_value": 0,
                        },
                    },
                    {
                        "setting": "doppler",
                        "context_id": "rain_auto_modes_0_2",
                        "values": [0, 2],
                        "reference_settings": {
                            "rain_clutter_auto": True,
                            "rain_clutter": 0,
                            "rain_clutter_auto_value": 0,
                        },
                    },
                ],
            },
            "doppler",
        )
        expanded = expand_control_matrix(selected)
        contexts = {
            phase["id"]: phase["settings"]["rain_clutter_auto"]
            for phase in expanded["phases"]
            if phase["id"].startswith("reference_")
        }
        self.assertEqual(
            contexts,
            {
                "reference_rain_manual_modes_0_1": False,
                "reference_rain_auto_modes_0_2": True,
            },
        )
        self.assertEqual(
            [phase["id"] for phase in expanded["phases"] if phase["id"].startswith("doppler_")],
            [
                "doppler_0_rain_manual_modes_0_1",
                "doppler_1_rain_manual_modes_0_1",
                "doppler_0_rain_auto_modes_0_2",
                "doppler_2_rain_auto_modes_0_2",
            ],
        )

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

    def test_gain_matrix_keeps_active_value_consistent_with_mode(self) -> None:
        expanded = expand_control_matrix(
            {
                "phases": [{"id": "reference", "settings": {}}],
                "control_matrix": [
                    {
                        "setting": "gain_auto",
                        "values": [False, True],
                        "reference_settings": {
                            "gain_auto": False,
                            "gain_value": 64,
                            "gain_manual_value": 64,
                            "gain_auto_value": 80,
                        },
                        "value_settings": {
                            "false": {
                                "gain_value": 64,
                                "gain_manual_value": 64,
                                "gain_auto_value": 80,
                            },
                            "true": {
                                "gain_value": 80,
                                "gain_manual_value": 64,
                                "gain_auto_value": 80,
                            },
                        },
                    }
                ],
            }
        )
        conditions = [phase for phase in expanded["phases"] if phase["id"].startswith("gain_auto_")]
        self.assertEqual(
            [
                (phase["settings"]["gain_auto"], phase["settings"]["gain_value"])
                for phase in conditions
            ],
            [(False, 64), (True, 80)],
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
        state.update(
            {
                "gain_manual_value": 0,
                "gain_value": 0,
                "antenna_hi_rpm": False,
            }
        )

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
            _path, params = capture_runner.setting_requests(setting, value, state)
            if setting == "sea_clutter_auto":
                state["sea_clutter_auto"] = bool(value)
                state["sea_clutter"] = params["manual"]
                state["sea_clutter_auto_value"] = params["auto"]
                state["sea_clutter_mode"] = params["mode"]
            elif setting == "rain_clutter_auto":
                state["rain_clutter_auto"] = bool(value)
                state["rain_clutter"] = params["manual"]
                state["rain_clutter_auto_value"] = params["auto"]
                state["rain_clutter_ftc_mode"] = params["ftcMode"]
                state["rain_clutter_mode"] = params["rainMode"]
            elif setting == "gain_auto":
                state["gain_auto"] = bool(value)
                state["gain_manual_value"] = params["manualValue"]
                state["gain_auto_value"] = params["autoValue"]
                state["gain_value"] = (
                    state["gain_auto_value"] if bool(value) else state["gain_manual_value"]
                )
            elif setting == "antenna_speed":
                state["antenna_speed"] = params["rollSpeed"]
                state["antenna_hi_rpm"] = bool(params["hiRpm"])
            else:
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
        state.update(
            {
                "gain_auto": False,
                "gain_manual_value": 1,
                "gain_value": 1,
                "sea_clutter": 4,
                "rain_clutter": 4,
                "antenna_speed": 1,
                "antenna_hi_rpm": False,
            }
        )
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
                state["gain_manual_value"] = params["manualValue"]
                state["gain_auto_value"] = params["autoValue"]
                state["gain_value"] = (
                    state["gain_auto_value"] if bool(value) else state["gain_manual_value"]
                )
            elif setting == "sea_clutter_auto":
                state["sea_clutter_auto"] = bool(value)
                state["sea_clutter"] = params["manual"]
                state["sea_clutter_auto_value"] = params["auto"]
                state["sea_clutter_mode"] = params["mode"]
            elif setting == "rain_clutter_auto":
                state["rain_clutter_auto"] = bool(value)
                state["rain_clutter"] = params["manual"]
                state["rain_clutter_auto_value"] = params["auto"]
                state["rain_clutter_ftc_mode"] = params["ftcMode"]
                state["rain_clutter_mode"] = params["rainMode"]
            elif setting == "antenna_speed":
                state["antenna_speed"] = params["rollSpeed"]
                state["antenna_hi_rpm"] = bool(params["hiRpm"])
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
        self.assertEqual(
            [setting for setting, _value, _payload, _params in commands],
            ["gain_auto", "sea_clutter_auto", "rain_clutter_auto", "antenna_speed"],
        )
        gain_params = commands[0][3]
        self.assertEqual(gain_params["status"], 1)
        self.assertEqual(gain_params["manualValue"], 7)
        self.assertEqual(gain_params["autoValue"], 80)
        commands.clear()
        with tempfile.TemporaryDirectory() as second_tmp:
            with (
                patch("scripts.capture_runner.readback", fake_readback),
                patch("scripts.capture_runner.issue_setting", fake_issue),
            ):
                with (Path(second_tmp) / "events-second.jsonl").open("w", encoding="utf-8") as fp:
                    unchanged = restore_verified_baseline(
                        "mock://controller",
                        {"settings": baseline_values},
                        fp,
                        1.0,
                        "unchanged-test",
                    )
        self.assertTrue(unchanged["restore_complete"])
        self.assertFalse(commands)

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
        self.assertFalse(commands)
        self.assertTrue(report["restore_errors"])


if __name__ == "__main__":
    unittest.main()

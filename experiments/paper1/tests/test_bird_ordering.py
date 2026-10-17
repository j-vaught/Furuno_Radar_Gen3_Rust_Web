from __future__ import annotations

import tempfile
import unittest
from pathlib import Path
from typing import Any
from unittest.mock import patch

from scripts.capture_runner import (
    REQUIRED_READBACK_FIELDS,
    apply_phase,
    restore_verified_baseline,
)


def coupled_state(*, bird: int, gain: int, sea: int) -> dict[str, Any]:
    state = {field: 0 for field in REQUIRED_READBACK_FIELDS}
    state.update(
        {
            "tx_state": 1,
            "range_index": 0,
            "pulse_width": 0,
            "pulse_range_index": 0,
            "gain_auto": False,
            "gain_value": gain,
            "gain_manual_value": gain,
            "gain_auto_value": 80,
            "sea_clutter": sea,
            "sea_clutter_auto": False,
            "sea_clutter_auto_value": 10,
            "sea_clutter_mode": 0,
            "rain_clutter": 0,
            "rain_clutter_auto": False,
            "rain_clutter_auto_value": 0,
            "rain_clutter_ftc_mode": 0,
            "rain_clutter_mode": 0,
            "antenna_speed": 0,
            "antenna_hi_rpm": False,
            "bird_mode": bird,
            "doppler": 0,
        }
    )
    return state


def readback_result(state: dict[str, Any]) -> dict[str, Any]:
    return {"confirmed": True, "fresh": True, "settings": dict(state)}


def coupled_http(
    state: dict[str, Any],
    requests: list[tuple[str, dict[str, Any]]],
    *,
    bird_zero_resets_dependents: bool = False,
):
    def request(_base: str, path: str, params: dict[str, Any], _timeout: float) -> tuple[int, str]:
        requests.append((path, dict(params)))
        if path == "/set_bird_mode":
            level = int(params["level"])
            state["bird_mode"] = level
            if level == 0 and bird_zero_resets_dependents:
                state["gain_value"] = 93
                state["gain_manual_value"] = 93
                state["sea_clutter"] = 30
            elif level == 1:
                state["gain_value"] = 93
                state["gain_manual_value"] = 93
            elif level == 2:
                state["gain_value"] = 93
                state["gain_manual_value"] = 93
                state["sea_clutter"] = 30
            elif level == 3:
                state["gain_value"] = 93
                state["gain_manual_value"] = 93
                state["sea_clutter"] = 40
        elif path == "/set_gain":
            state["gain_auto"] = bool(params["status"])
            state["gain_manual_value"] = int(params["manualValue"])
            state["gain_value"] = state["gain_manual_value"]
            state["gain_auto_value"] = int(params["autoValue"])
        elif path == "/set_acsea":
            state["sea_clutter_auto"] = bool(params["status"])
            state["sea_clutter"] = int(params["manual"])
            state["sea_clutter_auto_value"] = int(params["auto"])
            state["sea_clutter_mode"] = int(params["mode"])
        elif path == "/set_doppler":
            state["doppler"] = int(params["mode"])
        return 200, "ACK"

    return request


class BirdOrderingTests(unittest.TestCase):
    def test_apply_phase_applies_bird_before_dependent_registers(self) -> None:
        for level, expected_paths in (
            (1, ["/set_bird_mode", "/set_gain"]),
            (2, ["/set_bird_mode", "/set_gain", "/set_acsea"]),
            (3, ["/set_bird_mode", "/set_gain", "/set_acsea"]),
        ):
            with self.subTest(level=level):
                state = coupled_state(bird=0, gain=64, sea=0)
                requests: list[tuple[str, dict[str, Any]]] = []
                phase = {
                    "id": f"bird-{level}",
                    "settings": {
                        "bird_mode": level,
                        "gain_auto": False,
                        "gain_value": 64,
                        "gain_manual_value": 64,
                        "gain_auto_value": 80,
                        "sea_clutter_auto": False,
                        "sea_clutter": 0,
                        "sea_clutter_auto_value": 10,
                        "sea_clutter_mode": 0,
                    },
                }

                with (
                    tempfile.TemporaryDirectory() as tmp,
                    patch(
                        "scripts.capture_runner.readback",
                        side_effect=lambda *_args, _state=state, **_kwargs: readback_result(_state),
                    ),
                    patch(
                        "scripts.capture_runner.http_request",
                        side_effect=coupled_http(state, requests),
                    ),
                ):
                    with (Path(tmp) / "events.jsonl").open("w", encoding="utf-8") as events:
                        apply_phase("mock://controller", phase, events, 1.0)

                self.assertEqual(
                    [path for path, _params in requests],
                    expected_paths,
                )
                self.assertEqual(state["bird_mode"], level)
                self.assertEqual(state["gain_value"], 64)
                self.assertEqual(state["sea_clutter"], 0)

    def test_restore_applies_bird_before_dependent_baseline_registers(self) -> None:
        state = coupled_state(bird=2, gain=93, sea=30)
        requests: list[tuple[str, dict[str, Any]]] = []
        baseline = {"settings": coupled_state(bird=0, gain=64, sea=0)}

        with (
            tempfile.TemporaryDirectory() as tmp,
            patch(
                "scripts.capture_runner.readback",
                side_effect=lambda *_args, **_kwargs: readback_result(state),
            ),
            patch(
                "scripts.capture_runner.http_request",
                side_effect=coupled_http(
                    state,
                    requests,
                    bird_zero_resets_dependents=True,
                ),
            ),
        ):
            with (Path(tmp) / "events.jsonl").open("w", encoding="utf-8") as events:
                result = restore_verified_baseline(
                    "mock://controller",
                    baseline,
                    events,
                    1.0,
                    "bird-restore",
                )

        self.assertEqual(
            [path for path, _params in requests[:3]],
            [
                "/set_bird_mode",
                "/set_gain",
                "/set_acsea",
            ],
        )
        self.assertTrue(result["restore_complete"])
        self.assertEqual(state["bird_mode"], 0)
        self.assertEqual(state["gain_value"], 64)
        self.assertEqual(state["sea_clutter"], 0)

    def test_pulse_state_restore_orders_doppler_bird_then_dependents(self) -> None:
        state = coupled_state(bird=2, gain=93, sea=30)
        state["doppler"] = 1
        requests: list[tuple[str, dict[str, Any]]] = []
        baseline = {"settings": coupled_state(bird=0, gain=64, sea=0)}

        with (
            tempfile.TemporaryDirectory() as tmp,
            patch(
                "scripts.capture_runner.readback",
                side_effect=lambda *_args, **_kwargs: readback_result(state),
            ),
            patch(
                "scripts.capture_runner.http_request",
                side_effect=coupled_http(
                    state,
                    requests,
                    bird_zero_resets_dependents=True,
                ),
            ),
        ):
            with (Path(tmp) / "events.jsonl").open("w", encoding="utf-8") as events:
                result = restore_verified_baseline(
                    "mock://controller",
                    baseline,
                    events,
                    1.0,
                    "bird-restore-pulse-state",
                    pulse_states={"0": {"pulse_width": 0}},
                )

        paths = [path for path, _params in requests]
        self.assertLess(paths.index("/set_doppler"), paths.index("/set_bird_mode"))
        self.assertLess(paths.index("/set_bird_mode"), paths.index("/set_gain"))
        self.assertLess(paths.index("/set_gain"), paths.index("/set_acsea"))
        self.assertTrue(result["restore_complete"])
        self.assertEqual(state["doppler"], 0)
        self.assertEqual(state["bird_mode"], 0)
        self.assertEqual(state["gain_value"], 64)
        self.assertEqual(state["sea_clutter"], 0)


if __name__ == "__main__":
    unittest.main()

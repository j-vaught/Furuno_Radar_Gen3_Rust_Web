#!/usr/bin/env python3
"""Recover an evidence-scoped characterization report from an event log.

This utility reconstructs only command/readback pairs that are present in the
event log. It marks interference-rejection outcomes as superseded when the
source log contains the known selector-2 readback, and never labels a missing
readback as supported.
"""

from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
from pathlib import Path
from typing import Any


def sha256_path(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as fp:
        for block in iter(lambda: fp.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def _bool_equal(actual: Any, requested: Any) -> bool:
    if isinstance(requested, bool):
        return bool(actual) == requested
    return actual == requested


def salvage(events_path: Path) -> dict[str, Any]:
    events: list[dict[str, Any]] = []
    with events_path.open(encoding="utf-8") as fp:
        for line in fp:
            try:
                value = json.loads(line)
            except json.JSONDecodeError:
                continue
            if isinstance(value, dict):
                events.append(value)
    outcomes: list[dict[str, Any]] = []
    setting_commands = 0
    readback_count = 0
    for index, event in enumerate(events):
        if event.get("event") == "settings_readback":
            readback_count += 1
        if event.get("event") != "setting_command":
            continue
        context = str(event.get("context", ""))
        if context.startswith("characterization-baseline") or context.startswith("restore"):
            continue
        setting_commands += 1
        readback: dict[str, Any] | None = None
        for candidate in events[index + 1 :]:
            if candidate.get("event") == "settings_readback":
                response = candidate.get("response")
                if isinstance(response, dict):
                    readback = candidate
                break
        response_body = str(event.get("response", ""))
        actual = None
        if readback is not None:
            response = readback.get("response", {})
            settings = response.get("settings", {}) if isinstance(response, dict) else {}
            if isinstance(settings, dict):
                actual = settings.get(event.get("setting"))
        lower = response_body.lower()
        rejected = event.get("http_status", 500) not in range(200, 300) or any(
            token in lower
            for token in ("error", "failed", "rejected", "unsupported", "timeout", "no ack")
        )
        requested = event.get("requested")
        if event.get("setting") == "interference_rejection":
            outcome = "superseded"
            reason = (
                "source event log used the known selector-2 IR readback; "
                "rerun the corrected supplement"
            )
        elif rejected:
            outcome = "rejected"
            reason = "command response contains an error or rejection"
        elif readback is None or actual is None:
            outcome = "verification_failed"
            reason = "no usable subsequent settings readback was recorded"
        elif _bool_equal(actual, requested):
            outcome = "supported"
            reason = "command and subsequent event readback agree"
        else:
            outcome = "clamped"
            reason = "subsequent event readback differs from requested value"
        outcomes.append(
            {
                "setting": event.get("setting"),
                "requested": requested,
                "actual": actual,
                "previous": None,
                "outcome": outcome,
                "reason": reason,
                "context": context,
                "command": event,
                "readback": readback,
                "evidence": "reconstructed_from_event_order",
            }
        )
    timestamps = [event.get("timestamp_utc") for event in events if event.get("timestamp_utc")]
    return {
        "schema_version": "paper1-characterization-salvage-1.0",
        "status": "failed_recovered",
        "session_id": (
            (events[0].get("session_id") or events_path.parent.name)
            if events
            else events_path.parent.name
        ),
        "error": next(
            (event.get("error") for event in events if event.get("event") == "session_error"),
            "source session ended without a durable manifest",
        ),
        "provenance": {
            "source_events": str(events_path),
            "source_events_sha256": sha256_path(events_path),
            "event_count": len(events),
            "setting_command_count": setting_commands,
            "settings_readback_count": readback_count,
            "first_event_utc": timestamps[0] if timestamps else None,
            "last_event_utc": timestamps[-1] if timestamps else None,
            "recovered_at_utc": dt.datetime.now(dt.UTC).isoformat(timespec="milliseconds"),
            "git_provenance": None,
        },
        "control_characterization": {
            "outcomes": outcomes,
            "restore_complete": False,
            "restore_status": "not_claimed_from_event_log",
            "note": (
                "The source process did not write a final manifest; no restoration "
                "success is inferred."
            ),
        },
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--events", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    report = salvage(args.events)
    args.out.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

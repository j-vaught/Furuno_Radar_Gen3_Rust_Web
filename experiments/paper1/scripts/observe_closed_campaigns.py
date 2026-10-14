#!/usr/bin/env python3
"""Watch selected mirrored Paper 1 sessions and audit only closed complete data."""

from __future__ import annotations

import argparse
import hashlib
import json
import time
from pathlib import Path
from typing import Any

try:
    from scripts.doppler_campaign_audit import locate_session, manifest_csv_paths, write_campaign
except ModuleNotFoundError:
    from importlib import import_module

    _campaign_audit = import_module("doppler_campaign_audit")
    locate_session = _campaign_audit.locate_session
    manifest_csv_paths = _campaign_audit.manifest_csv_paths
    write_campaign = _campaign_audit.write_campaign

DEFAULT_LEGACY = (
    "indoor-all-controls-EF-paired-30s-20261014-gain_value",
    "indoor-all-controls-resume-30s-20261014-gain_auto",
)
OBSERVER_VERSION = "paper1-closed-campaign-observer-5"
AUDIT_SOURCE_NAMES = (
    "doppler_audit.py",
    "doppler_campaign_audit.py",
    "observe_closed_campaigns.py",
)


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as fp:
        for block in iter(lambda: fp.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def source_sha256() -> str:
    """Hash the replay, campaign, and observer sources used by the cache."""

    digest = hashlib.sha256()
    source_dir = Path(__file__).resolve().parent
    for name in AUDIT_SOURCE_NAMES:
        path = source_dir / name
        digest.update(name.encode("utf-8"))
        digest.update(b"\0")
        with path.open("rb") as fp:
            for block in iter(lambda: fp.read(1024 * 1024), b""):
                digest.update(block)
    return digest.hexdigest()


def load_json(path: Path) -> dict[str, Any]:
    value = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(value, dict):
        raise ValueError(f"JSON object required: {path}")
    return value


def campaign_children(root: Path, prefix: str) -> list[Path]:
    """Return complete child session directories for one selected campaign prefix."""

    campaign_dir = root / prefix
    manifest_path = campaign_dir / "campaign_manifest.json"
    if not manifest_path.exists():
        return []
    manifest = load_json(manifest_path)
    if manifest.get("status") == "failed":
        return []
    children = manifest.get("children", [])
    if not isinstance(children, list):
        return []
    sessions: list[Path] = []
    for child in children:
        if not isinstance(child, dict) or child.get("status") != "complete":
            continue
        session_id = child.get("session_id")
        if not isinstance(session_id, str):
            continue
        session_dir = root / session_id
        if (session_dir / "session_manifest.json").exists():
            sessions.append(session_dir)
    return sessions


def selected_sessions(root: Path, prefix: str, legacy: tuple[str, ...]) -> list[Path]:
    sessions = campaign_children(root, prefix)
    for session_id in legacy:
        session_dir = root / session_id
        manifest_path = session_dir / "session_manifest.json"
        if not manifest_path.exists():
            continue
        manifest = load_json(manifest_path)
        if manifest.get("status") == "complete":
            sessions.append(session_dir)
    return sorted(set(sessions))


def mirror_ready(session_dir: Path) -> tuple[bool, list[str]]:
    try:
        _, _, raw_path, manifest = locate_session(session_dir)
    except (OSError, ValueError, FileNotFoundError) as exc:
        return False, [str(exc)]
    missing = []
    if not raw_path.exists() or raw_path.stat().st_size == 0:
        missing.append(str(raw_path))
    missing.extend(
        str(path) for path in manifest_csv_paths(session_dir, manifest) if not path.exists()
    )
    return not missing, missing


def audit_session(session_dir: Path, output_root: Path) -> dict[str, Any]:
    """Audit one complete session and return its compact quality result."""

    ready, missing = mirror_ready(session_dir)
    if not ready:
        return {"session_id": session_dir.name, "status": "waiting_for_mirror", "missing": missing}
    _, manifest_path, raw_path, _ = locate_session(session_dir)
    manifest_sha = sha256_file(manifest_path)
    raw_sha = sha256_file(raw_path)
    out = output_root / session_dir.name
    summary = write_campaign([session_dir], out)
    session = summary["sessions"][0]
    quality = session.get("quality", {})
    return {
        "session_id": session_dir.name,
        "manifest_sha256": manifest_sha,
        "raw_sha256": raw_sha,
        "source_sha256": source_sha256(),
        "raw_size": raw_path.stat().st_size,
        "raw_mtime_ns": raw_path.stat().st_mtime_ns,
        "status": quality.get("quality_status", "fail"),
        "quality": quality,
        "output": str(out),
    }


def run_once(root: Path, output_root: Path, prefix: str, legacy: tuple[str, ...]) -> dict[str, Any]:
    output_root.mkdir(parents=True, exist_ok=True)
    state_path = output_root / "quality_state.json"
    previous = load_json(state_path) if state_path.exists() else {"sessions": {}}
    previous_sessions = previous.get("sessions", {})
    if not isinstance(previous_sessions, dict):
        previous_sessions = {}
    records: dict[str, Any] = {}
    current_source_sha = source_sha256()
    for session_dir in selected_sessions(root, prefix, legacy):
        manifest_path = session_dir / "session_manifest.json"
        manifest_sha = sha256_file(manifest_path)
        prior = previous_sessions.get(session_dir.name, {})
        ready, _ = mirror_ready(session_dir)
        if ready and isinstance(prior, dict) and previous.get("schema_version") == OBSERVER_VERSION:
            _, _, raw_path, _ = locate_session(session_dir)
            raw_size = raw_path.stat().st_size
            raw_mtime_ns = raw_path.stat().st_mtime_ns
            same_manifest = prior.get("manifest_sha256") == manifest_sha
            same_source = prior.get("source_sha256") == current_source_sha
            same_stat = (
                prior.get("raw_size") == raw_size and prior.get("raw_mtime_ns") == raw_mtime_ns
            )
            if same_manifest and same_source and same_stat:
                records[session_dir.name] = prior
                continue
            if same_manifest and same_source and prior.get("raw_sha256") == sha256_file(raw_path):
                records[session_dir.name] = {
                    **prior,
                    "raw_size": raw_size,
                    "raw_mtime_ns": raw_mtime_ns,
                }
                continue
        record = audit_session(session_dir, output_root)
        record.setdefault("source_sha256", current_source_sha)
        records[session_dir.name] = record
    quality_failures = {
        session_id: record
        for session_id, record in records.items()
        if record.get("status") == "fail"
    }
    state = {
        "schema_version": OBSERVER_VERSION,
        "observer_status": "watching",
        "updated_unix_s": time.time(),
        "watched_campaign_prefix": prefix,
        "watched_legacy_sessions": list(legacy),
        "sessions": records,
        "quality_status": "fail" if quality_failures else "pass",
        "quality_failures": sorted(quality_failures),
    }
    state_path.write_text(json.dumps(state, indent=2) + "\n", encoding="utf-8")
    marker = output_root / "QUALITY_FAIL.json"
    if quality_failures:
        marker.write_text(json.dumps({"quality_failures": quality_failures}, indent=2) + "\n")
    elif marker.exists():
        marker.unlink()
    return state


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--output-root", type=Path, required=True)
    parser.add_argument("--campaign-prefix", default="indoor-all-controls-buffered-v2-30s-20261014")
    parser.add_argument("--legacy-session", action="append", default=list(DEFAULT_LEGACY))
    parser.add_argument("--interval-s", type=float, default=45.0)
    parser.add_argument("--once", action="store_true")
    args = parser.parse_args()
    legacy = tuple(dict.fromkeys(args.legacy_session))
    args.output_root.mkdir(parents=True, exist_ok=True)
    state_path = args.output_root / "quality_state.json"
    state: dict[str, Any] = load_json(state_path) if state_path.exists() else {"sessions": {}}
    if "schema_version" not in state:
        state["schema_version"] = OBSERVER_VERSION
    state.update(
        {
            "observer_status": "running",
            "quality_status": "pending",
            "quality_failures": [],
        }
    )
    state_path.write_text(json.dumps(state, indent=2) + "\n", encoding="utf-8")
    while True:
        state = run_once(args.root, args.output_root, args.campaign_prefix, legacy)
        print(
            json.dumps(
                {"quality_status": state["quality_status"], "sessions": sorted(state["sessions"])}
            ),
            flush=True,
        )
        if args.once:
            return 1 if state["quality_status"] == "fail" else 0
        time.sleep(args.interval_s)


if __name__ == "__main__":
    raise SystemExit(main())

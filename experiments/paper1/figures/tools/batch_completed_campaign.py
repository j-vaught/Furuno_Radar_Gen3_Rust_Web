"""Analyze only validated complete campaign children on a compute host.

The runner records every child, including failed and interrupted attempts. This
tool selects a child only when both the campaign record and its session
manifest report completion. It then runs ``analyze_capture.py`` against the
mirrored CSVs, merges the resulting phase metrics, and emits the existing
campaign figure contract. Missing or failed children are reported in the
manifest and are never represented as zero-valued measurements.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import subprocess
import sys
from copy import deepcopy
from pathlib import Path
from typing import Any


def read_json(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def rows(path: Path) -> list[dict[str, str]]:
    if not path.exists():
        return []
    with path.open(encoding="utf-8", newline="") as stream:
        return list(csv.DictReader(stream))


def write_rows(path: Path, items: list[dict[str, Any]]) -> None:
    if not items:
        path.write_text("\n", encoding="utf-8")
        return
    fields = list(items[0])
    with path.open("w", encoding="utf-8", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields, extrasaction="ignore")
        writer.writeheader()
        writer.writerows(items)


def file_sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def analysis_cache_valid(out: Path, session: Path) -> bool:
    """Require raw replay inputs and their recorded hashes before reuse."""
    marker = out / "analysis_inputs.json"
    replay = out / "raw_replay_summary.json"
    raw = session / "raw_udp_payloads.jsonl"
    events = session / "events.jsonl"
    if not marker.exists() or not replay.exists() or not raw.exists():
        return False
    try:
        replay_data = read_json(replay)
        marker_data = read_json(marker)
    except (OSError, json.JSONDecodeError):
        return False
    if replay_data.get("missing") is True:
        return False
    expected = {"raw_udp_payloads.jsonl": raw}
    if events.exists():
        expected["events.jsonl"] = events
    recorded = marker_data.get("files", {})
    return all(
        recorded.get(name, {}).get("sha256") == file_sha256(path)
        for name, path in expected.items()
    )


def write_analysis_cache_marker(out: Path, session: Path) -> None:
    files = {}
    for name in ("raw_udp_payloads.jsonl", "events.jsonl"):
        path = session / name
        if path.exists():
            files[name] = {"path": str(path), "sha256": file_sha256(path)}
    (out / "analysis_inputs.json").write_text(
        json.dumps({"schema_version": "paper1-analysis-inputs-1.0", "files": files}, indent=2)
        + "\n",
        encoding="utf-8",
    )


def mapped_path(value: str, path_maps: list[tuple[str, str]]) -> str:
    for source, target in path_maps:
        if value == source or value.startswith(source + "/"):
            return target + value[len(source) :]
    return value


def parse_maps(values: list[str]) -> list[tuple[str, str]]:
    result = []
    for value in values:
        source, separator, target = value.partition("=")
        if not separator or not source or not target:
            raise ValueError(f"invalid --path-map {value!r}; expected FROM=TO")
        result.append((source.rstrip("/"), target.rstrip("/")))
    return result


def complete_children(
    campaign_paths: list[Path], session_root: Path
) -> tuple[list[dict[str, Any]], list[dict[str, Any]]]:
    complete = []
    skipped = []
    for campaign_path in campaign_paths:
        campaign = read_json(campaign_path)
        for child in campaign.get("children", []):
            session_id = str(child.get("session_id", ""))
            session_dir = session_root / session_id
            manifest_path = Path(str(child.get("manifest_path", "")))
            if not (manifest_path / "session_manifest.json").exists():
                manifest_path = session_dir
            session_manifest_path = manifest_path / "session_manifest.json"
            session_status = "missing"
            if session_manifest_path.exists():
                session_status = read_json(session_manifest_path).get("status", "missing")
            valid = (
                child.get("status") == "complete"
                and child.get("validation", {}).get("valid") is True
            )
            valid = valid and session_status == "complete"
            record = {
                "family": child.get("family"),
                "session_id": session_id,
                "campaign": str(campaign_path),
                "session_dir": str(manifest_path),
                "child_status": child.get("status"),
                "session_status": session_status,
            }
            if valid:
                complete.append(record)
            else:
                record["reason"] = "child or session is not validated complete"
                skipped.append(record)
    return complete, skipped


def analyze_child(
    child: dict[str, Any],
    analyzer: Path,
    analysis_root: Path,
    path_maps: list[tuple[str, str]],
    reuse: bool,
) -> Path:
    session = Path(child["session_dir"])
    out = analysis_root / child["session_id"]
    metrics = out / "rotation_metrics.csv"
    cache_ok = reuse and metrics.exists() and analysis_cache_valid(out, session)
    if not cache_ok:
        command = [sys.executable, str(analyzer), "--session", str(session), "--out", str(out)]
        for source, target in path_maps:
            command.extend(("--path-map", f"{source}={target}"))
        subprocess.run(command, check=True)
        write_analysis_cache_marker(out, session)
    if not metrics.exists():
        raise RuntimeError(f"analyzer did not produce {metrics}")
    return out


def merge_analysis(
    children: list[dict[str, Any]],
    analysis_dirs: list[Path],
    output_root: Path,
    path_maps: list[tuple[str, str]],
) -> tuple[Path, Path]:
    output_root.mkdir(parents=True, exist_ok=True)
    figure_dir = output_root / "figure_data"
    figure_dir.mkdir(parents=True, exist_ok=True)
    phases: list[dict[str, Any]] = []
    metrics: list[dict[str, Any]] = []
    session_rows: list[dict[str, Any]] = []
    transitions: list[dict[str, Any]] = []
    phase_lookup: dict[tuple[str, str], int] = {}
    metric_by_sha: dict[str, dict[str, str]] = {}

    for child, analysis_dir in zip(children, analysis_dirs, strict=True):
        session_manifest = read_json(Path(child["session_dir"]) / "session_manifest.json")
        family = str(
            child.get("family") or session_manifest.get("selected_control_family") or "unknown"
        )
        local_phases = session_manifest.get("phases", [])
        summary_rows = rows(analysis_dir / "figure_data" / "session_summary.csv")
        for phase_index, original in enumerate(local_phases):
            phase = deepcopy(original)
            original_id = str(phase.get("phase_id", f"phase_{phase_index}"))
            unique_id = f"{family}::{child['session_id']}::{original_id}"
            phase["phase_id"] = unique_id
            phase["control_family"] = family
            phase["rotation_files"] = [
                mapped_path(str(value), path_maps) for value in phase.get("rotation_files", [])
            ]
            phase_index_merged = len(phases)
            phases.append(phase)
            phase_lookup[(child["session_id"], original_id)] = phase_index_merged
        for row in summary_rows:
            original_id = row.get("phase_id", "unknown")
            phase_index_merged = phase_lookup.get((child["session_id"], original_id))
            if phase_index_merged is None:
                continue
            item = dict(row)
            item["phase_index"] = str(phase_index_merged)
            item["phase_id"] = phases[phase_index_merged]["phase_id"]
            item["csv_path"] = mapped_path(item.get("csv_path", ""), path_maps)
            item["analysis_csv_path"] = mapped_path(item.get("analysis_csv_path", ""), path_maps)
            session_rows.append(item)
        for item in rows(analysis_dir / "rotation_metrics.csv"):
            metric_by_sha[item.get("csv_sha256", "")] = dict(item)
        transition_rows = rows(analysis_dir / "figure_data" / "setting_transition.csv")
        for item in transition_rows:
            transition = dict(item)
            original_id = transition.get("phase_id", "unknown")
            transition["phase_id"] = phases[phase_lookup[(child["session_id"], original_id)]][
                "phase_id"
            ]
            transitions.append(transition)

    for summary in session_rows:
        metric = metric_by_sha.get(summary.get("csv_sha256", ""))
        if metric is None:
            raise RuntimeError(f"missing rotation metric for {summary.get('csv_sha256')}")
        item = dict(metric)
        item["phase_index"] = summary["phase_index"]
        item["phase_id"] = summary["phase_id"]
        item["csv_path"] = summary["csv_path"]
        item["analysis_csv_path"] = summary["analysis_csv_path"]
        metrics.append(item)

    merged_manifest = {
        "schema_version": "paper1-complete-campaign-1.0",
        "session_id": "complete-campaign-aggregate",
        "experiment_id": "E6_indoor_one_control_response",
        "status": "complete-data-subset",
        "phases": phases,
        "complete_children": children,
    }
    manifest_path = output_root / "complete_campaign_manifest.json"
    metrics_path = output_root / "rotation_metrics.csv"
    manifest_path.write_text(json.dumps(merged_manifest, indent=2) + "\n", encoding="utf-8")
    write_rows(metrics_path, metrics)
    write_rows(figure_dir / "session_summary.csv", session_rows)
    write_rows(figure_dir / "setting_transition.csv", transitions)
    return manifest_path, metrics_path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--campaign-manifest", action="append", type=Path)
    parser.add_argument("--session-root", type=Path, required=True)
    parser.add_argument("--analyzer", type=Path, required=True)
    parser.add_argument("--analysis-root", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--path-map", action="append", default=[])
    parser.add_argument("--reuse-existing", action="store_true")
    args = parser.parse_args()
    campaigns = args.campaign_manifest or sorted(args.session_root.glob("*/campaign_manifest.json"))
    if not campaigns:
        raise SystemExit("no campaign manifests found")
    path_maps = parse_maps(args.path_map)
    complete, skipped = complete_children(campaigns, args.session_root)
    if not complete:
        raise SystemExit("no validated complete children found")
    analysis_dirs = [
        analyze_child(child, args.analyzer, args.analysis_root, path_maps, args.reuse_existing)
        for child in complete
    ]
    manifest_path, metrics_path = merge_analysis(complete, analysis_dirs, args.out, path_maps)
    complete_families = {str(item["family"]) for item in complete}
    superseded_failed = [
        item
        for item in skipped
        if item.get("child_status") in {"failed", "interrupted"}
        and str(item.get("family")) in complete_families
    ]
    excluded_failed = [item for item in skipped if item not in superseded_failed]
    (args.out / "selection_report.json").write_text(
        json.dumps(
            {
                "complete": complete,
                "skipped": skipped,
                "superseded_failed": superseded_failed,
                "excluded_failed": excluded_failed,
            },
            indent=2,
        )
        + "\n",
        encoding="utf-8",
    )
    planned = sorted(
        {
            str(family)
            for campaign_path in campaigns
            for family in read_json(campaign_path).get("families", [])
        }
    )
    failed_families = {
        str(item["family"])
        for item in skipped
        if item.get("child_status") in {"failed", "interrupted"}
    } - complete_families
    family_status = []
    for family in planned:
        status = (
            "complete"
            if family in complete_families
            else "failed"
            if family in failed_families
            else "pending"
        )
        family_status.append({"family": family, "status": status})
    (args.out / "family_coverage.json").write_text(
        json.dumps(
            {
                "status": "ready",
                "planned_family_count": len(planned),
                "family_status": family_status,
                "status_labels": ["complete", "failed", "pending"],
                "status_counts": [
                    sum(item["status"] == "complete" for item in family_status),
                    sum(item["status"] == "failed" for item in family_status),
                    sum(item["status"] == "pending" for item in family_status),
                ],
            },
            indent=2,
        )
        + "\n",
        encoding="utf-8",
    )
    print(
        json.dumps(
            {
                "complete_children": len(complete),
                "skipped_children": len(skipped),
                "manifest": str(manifest_path),
                "metrics": str(metrics_path),
            }
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

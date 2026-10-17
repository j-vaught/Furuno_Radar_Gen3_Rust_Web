#!/usr/bin/env python3
"""Run all Paper 1 indoor response families as independently checkpointed children."""

from __future__ import annotations

import argparse
import datetime as dt
import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import Any


def utc_now() -> str:
    return dt.datetime.now(dt.UTC).isoformat(timespec="milliseconds")


def load_json(path: Path) -> dict[str, Any]:
    with path.open(encoding="utf-8") as fp:
        value = json.load(fp)
    if not isinstance(value, dict):
        raise ValueError(f"expected JSON object: {path}")
    return value


def write_json_atomic(path: Path, value: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary: Path | None = None
    try:
        with tempfile.NamedTemporaryFile(
            mode="w",
            encoding="utf-8",
            dir=path.parent,
            prefix=f".{path.name}.",
            suffix=".tmp",
            delete=False,
        ) as fp:
            temporary = Path(fp.name)
            json.dump(value, fp, indent=2)
            fp.write("\n")
            fp.flush()
            os.fsync(fp.fileno())
        os.replace(temporary, path)
        temporary = None
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)


def control_families(plan: dict[str, Any], experiment_id: str) -> list[str]:
    experiment = next(
        (item for item in plan.get("experiments", []) if item.get("id") == experiment_id),
        None,
    )
    if not isinstance(experiment, dict):
        raise ValueError(f"unknown experiment: {experiment_id}")
    matrix = experiment.get("control_matrix")
    if not isinstance(matrix, list):
        raise ValueError(f"experiment has no control matrix: {experiment_id}")
    families: list[str] = []
    for entry in matrix:
        setting = entry.get("setting")
        if isinstance(setting, str) and setting not in families:
            families.append(setting)
    if not families:
        raise ValueError(f"control matrix is empty: {experiment_id}")
    return families


def select_families(available: list[str], requested: list[str]) -> list[str]:
    if not requested:
        return available
    unique_requested = list(dict.fromkeys(requested))
    unknown = sorted(set(unique_requested) - set(available))
    if unknown:
        raise ValueError(f"unknown control families: {unknown}; available={available}")
    return unique_requested


def settings_from_readback(readback: Any) -> dict[str, Any] | None:
    if not isinstance(readback, dict):
        return None
    settings = readback.get("settings")
    if not isinstance(settings, dict):
        return None
    return settings


def compare_readbacks(
    initial_readback: Any, final_readback: Any
) -> tuple[bool, list[str], int | None]:
    initial = settings_from_readback(initial_readback)
    final = settings_from_readback(final_readback)
    if initial is None or final is None:
        return False, ["missing_settings_object"], None
    mismatches = sorted(
        key
        for key in set(initial) | set(final)
        if key not in initial or key not in final or initial[key] != final[key]
    )
    final_tx = final.get("tx_state")
    return not mismatches and final_tx == 1, mismatches, final_tx


def verified_fresh_readback(readback: Any) -> bool:
    if not isinstance(readback, dict):
        return False
    if readback.get("confirmed") is not True or readback.get("fresh") is not True:
        return False
    snapshot = readback.get("settings_readback")
    if not isinstance(snapshot, dict) or snapshot.get("verified") is not True:
        return False
    if readback.get("missing_fields"):
        return False
    fields = snapshot.get("fields")
    return (
        isinstance(fields, dict)
        and bool(fields)
        and all(
            isinstance(field, dict) and field.get("verified") is True for field in fields.values()
        )
    )


def validate_child_manifest(manifest: dict[str, Any], dry_run: bool = False) -> dict[str, Any]:
    status = manifest.get("status")
    accepted_status = {"dry_run_validated"} if dry_run else {"complete"}
    errors: list[str] = []
    if status not in accepted_status:
        errors.append(f"status={status!r}")
    for field in ("final_baseline_restore_error", "cleanup_errors"):
        if manifest.get(field):
            errors.append(field)
    initial = manifest.get("initial_device_readback")
    final = manifest.get("final_baseline_restore")
    matches, mismatches, final_tx = compare_readbacks(initial, final)
    if not dry_run:
        if not verified_fresh_readback(initial):
            errors.append("initial_readback_not_confirmed_fresh_verified")
        if not verified_fresh_readback(final):
            errors.append("final_readback_not_confirmed_fresh_verified")
        if not matches:
            errors.append(f"initial_final_readback_mismatch={mismatches}")
        if final_tx != 1:
            errors.append(f"final_tx_state={final_tx!r}")
    return {
        "valid": not errors,
        "status": status,
        "errors": errors,
        "readback_matches": matches,
        "readback_mismatches": mismatches,
        "final_tx_state": final_tx,
        "completed_phase_count": len(manifest.get("phases", [])),
        "rotation_files": [
            path for phase in manifest.get("phases", []) for path in phase.get("rotation_files", [])
        ],
    }


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--plan", type=Path, required=True)
    parser.add_argument("--experiment", default="E6_indoor_one_control_response")
    parser.add_argument("--metadata", type=Path, required=True)
    parser.add_argument("--characterization-report", type=Path, required=True)
    parser.add_argument(
        "--characterization-report-supplement", type=Path, action="append", default=[]
    )
    parser.add_argument("--output-root", type=Path, required=True)
    parser.add_argument("--campaign-id", required=True)
    parser.add_argument("--base-url", default="http://127.0.0.1:8080")
    parser.add_argument("--family", dest="families", action="append", default=[])
    parser.add_argument("--transmit", action="store_true")
    parser.add_argument("--physical-ready", action="store_true")
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--receive-bind", default="0.0.0.0:10024")
    parser.add_argument("--receive-interface")
    parser.add_argument("--multicast", default="239.255.0.2")
    parser.add_argument("--peer")
    parser.add_argument("--timeout-per-phase", type=float, default=90.0)
    parser.add_argument("--campaign-timeout", type=float, default=7200.0)
    parser.add_argument("--http-timeout", type=float, default=8.0)
    return parser.parse_args()


def run_child_process(command: list[str], stdout: Any, stderr: Any) -> tuple[int, bool]:
    """Run one child and wait for its cleanup when interrupted."""
    process = subprocess.Popen(command, stdout=stdout, stderr=stderr)
    try:
        return process.wait(), False
    except KeyboardInterrupt:
        process.terminate()
        try:
            return process.wait(timeout=60), True
        except subprocess.TimeoutExpired:
            process.kill()
            return process.wait(), True


def main() -> int:
    args = parse_args()
    if args.experiment != "E6_indoor_one_control_response":
        raise SystemExit("the batched driver is restricted to the Paper 1 E6 indoor response plan")
    if not args.dry_run and not (args.transmit and args.physical_ready):
        raise SystemExit("live batching requires both --transmit and --physical-ready")
    plan = load_json(args.plan)
    families = control_families(plan, args.experiment)
    try:
        families = select_families(families, args.families)
    except ValueError as exc:
        raise SystemExit(str(exc)) from exc
    if not families:
        raise SystemExit("no control families selected")
    campaign_dir = args.output_root / args.campaign_id
    manifest_path = campaign_dir / "campaign_manifest.json"
    if campaign_dir.exists():
        raise SystemExit(
            f"refusing to reuse existing campaign directory {campaign_dir}; "
            "choose a new --campaign-id"
        )
    manifest: dict[str, Any] = {
        "schema_version": "paper1-indoor-campaign-1.0",
        "campaign_id": args.campaign_id,
        "experiment_id": args.experiment,
        "status": "running",
        "started_at_utc": utc_now(),
        "plan_path": str(args.plan),
        "metadata_path": str(args.metadata),
        "characterization_report": str(args.characterization_report),
        "families": families,
        "children": [],
        "dry_run": args.dry_run,
    }
    write_json_atomic(manifest_path, manifest)
    runner = Path(__file__).with_name("capture_runner.py")
    for index, family in enumerate(families):
        session_id = f"{args.campaign_id}-{family}"
        child_dir = args.output_root / session_id
        child_manifest_path = child_dir / "session_manifest.json"
        stdout_path = campaign_dir / "families" / f"{index:02d}-{family}.stdout.log"
        stderr_path = campaign_dir / "families" / f"{index:02d}-{family}.stderr.log"
        command = [
            sys.executable,
            str(runner),
            "--plan",
            str(args.plan),
            "--experiment",
            args.experiment,
            "--control-family",
            family,
            "--metadata",
            str(args.metadata),
            "--characterization-report",
            str(args.characterization_report),
            "--output-root",
            str(args.output_root),
            "--session-id",
            session_id,
            "--base-url",
            args.base_url,
            "--receive-bind",
            args.receive_bind,
            "--multicast",
            args.multicast,
            "--timeout-per-phase",
            str(args.timeout_per_phase),
            "--campaign-timeout",
            str(args.campaign_timeout),
            "--http-timeout",
            str(args.http_timeout),
        ]
        if args.receive_interface:
            command.extend(["--receive-interface", args.receive_interface])
        if args.peer:
            command.extend(["--peer", args.peer])
        for supplement in args.characterization_report_supplement:
            command.extend(["--characterization-report-supplement", str(supplement)])
        if args.transmit:
            command.append("--transmit")
        if args.physical_ready:
            command.append("--physical-ready")
        if args.dry_run:
            command.append("--dry-run")
        child_record: dict[str, Any] = {
            "family": family,
            "session_id": session_id,
            "index": index,
            "status": "running",
            "started_at_utc": utc_now(),
            "command": command,
            "manifest_path": str(child_manifest_path),
            "stdout_path": str(stdout_path),
            "stderr_path": str(stderr_path),
        }
        manifest["current_family"] = family
        manifest["current_index"] = index
        manifest["children"].append(child_record)
        write_json_atomic(manifest_path, manifest)
        stdout_path.parent.mkdir(parents=True, exist_ok=True)
        with (
            stdout_path.open("w", encoding="utf-8") as stdout,
            stderr_path.open("w", encoding="utf-8") as stderr,
        ):
            return_code, interrupted = run_child_process(command, stdout, stderr)
        child_record["return_code"] = return_code
        child_record["finished_at_utc"] = utc_now()
        if interrupted:
            child_record["status"] = "interrupted"
            child_record["validation"] = {
                "valid": False,
                "errors": ["driver_keyboard_interrupt_child_cleanup_waited"],
            }
            manifest["status"] = "interrupted"
            manifest["failure_family"] = family
            manifest["finished_at_utc"] = utc_now()
            write_json_atomic(manifest_path, manifest)
            return 130
        if not child_manifest_path.is_file():
            child_record["status"] = "failed"
            child_record["validation"] = {"valid": False, "errors": ["child_manifest_missing"]}
        else:
            child_manifest = load_json(child_manifest_path)
            validation = validate_child_manifest(child_manifest, dry_run=args.dry_run)
            child_record["status"] = "complete" if validation["valid"] else "failed"
            child_record["validation"] = validation
        if return_code != 0:
            child_record["status"] = "failed"
            child_record.setdefault("validation", {}).setdefault("errors", []).append(
                f"child_return_code={return_code}"
            )
        write_json_atomic(manifest_path, manifest)
        if child_record["status"] != "complete":
            manifest["status"] = "failed"
            manifest["failure_family"] = family
            manifest["finished_at_utc"] = utc_now()
            write_json_atomic(manifest_path, manifest)
            return 2
    manifest.pop("current_family", None)
    manifest.pop("current_index", None)
    manifest["status"] = "complete"
    manifest["finished_at_utc"] = utc_now()
    write_json_atomic(manifest_path, manifest)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

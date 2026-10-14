"""Incrementally mirror and render validated campaign children.

This is an offline watcher for a running radar campaign.  It reads campaign
metadata, copies only newly completed session manifests and CSVs to the
compute host, and reuses existing analyzer outputs.  Pending, failed, and
invalid children never enter the measured figure data.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import shlex
import subprocess
import time
from pathlib import Path
from typing import Any


def read_json(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(command: list[str], *, quiet: bool = False) -> None:
    subprocess.run(command, check=True, stdout=subprocess.DEVNULL if quiet else None)


def ssh(host: str, command: str) -> None:
    run(["ssh", host, command])


def remote_copy(host: str, local: Path, remote: str) -> None:
    ssh(host, f"mkdir -p {shlex.quote(str(Path(remote).parent))}")
    run(["scp", str(local), f"{host}:{remote}"])


def map_path(value: str, source: Path, target: Path) -> Path | None:
    try:
        relative = Path(value).relative_to(source)
    except ValueError:
        return None
    return target / relative


def complete_children(campaign_paths: list[Path]) -> list[dict[str, Any]]:
    complete: list[dict[str, Any]] = []
    for campaign_path in campaign_paths:
        if not campaign_path.exists():
            continue
        campaign = read_json(campaign_path)
        for child in campaign.get("children", []):
            manifest = Path(str(child.get("manifest_path", "")))
            if manifest.is_dir():
                manifest = manifest / "session_manifest.json"
            if (
                child.get("status") != "complete"
                or child.get("validation", {}).get("valid") is not True
                or not manifest.exists()
            ):
                continue
            session = read_json(manifest)
            if session.get("status") != "complete":
                continue
            complete.append(
                {
                    "family": child.get("family"),
                    "session_id": str(child.get("session_id", "")),
                    "manifest": manifest,
                    "campaign": campaign_path,
                    "digest": digest(manifest),
                }
            )
    return complete


def sync_session(
    item: dict[str, Any],
    *,
    host: str,
    local_data: Path,
    remote_data: Path,
) -> None:
    manifest = Path(item["manifest"])
    remote_manifest = map_path(str(manifest), local_data, remote_data)
    if remote_manifest is None:
        raise ValueError(f"session manifest is outside data root: {manifest}")
    remote_copy(host, manifest, str(remote_manifest))
    remote_session_dir = remote_manifest.parent
    ssh(host, f"mkdir -p {shlex.quote(str(remote_session_dir))}")
    # Keep the closed child's raw receiver stream and event log alongside its
    # manifest.  These files are required for the analyzer's raw replay and
    # are small enough to mirror only when a child first becomes complete.
    run(
        [
            "rsync",
            "-a",
            "--prune-empty-dirs",
            "--include=*/",
            "--include=*.json",
            "--include=*.jsonl",
            "--exclude=*",
            f"{manifest.parent}/",
            f"{host}:{remote_session_dir}/",
        ],
        quiet=True,
    )
    session = read_json(manifest)
    seen: set[Path] = set()
    for phase in session.get("phases", []):
        for value in phase.get("rotation_files", []):
            csv_path = Path(str(value))
            parent = csv_path.parent
            if parent in seen:
                continue
            seen.add(parent)
            remote_parent = map_path(str(parent), local_data, remote_data)
            if remote_parent is None:
                continue
            ssh(host, f"mkdir -p {shlex.quote(str(remote_parent))}")
            # Raw UDP recordings are intentionally excluded.  The analyzer
            # can derive all figure metrics from the recorded CSVs.
            run(
                [
                    "rsync",
                    "-a",
                    "--prune-empty-dirs",
                    "--include=*/",
                    "--include=*.csv",
                    "--exclude=*",
                    f"{parent}/",
                    f"{host}:{remote_parent}/",
                ],
                quiet=True,
            )


def sync_campaigns(
    campaign_paths: list[Path], *, host: str, local_data: Path, remote_data: Path
) -> None:
    for campaign in campaign_paths:
        remote_campaign = map_path(str(campaign), local_data, remote_data)
        if remote_campaign is not None and campaign.exists():
            remote_copy(host, campaign, str(remote_campaign))


def sync_figure_sources(host: str, local_figures: Path, remote_figures: Path) -> None:
    ssh(host, f"mkdir -p {shlex.quote(str(remote_figures / 'tools'))}")
    for name in [
        "batch_completed_campaign.py",
        "aggregate_campaign.py",
        "adapt_toolkit.py",
        "list_setting_families.py",
        "build-figures.sh",
    ]:
        relative = Path("tools") / name if name.endswith(".py") else Path(name)
        local = local_figures / relative
        remote = remote_figures / relative
        remote_copy(host, local, str(remote))
    for local in local_figures.glob("*.typ"):
        remote_copy(host, local, str(remote_figures / local.name))


def render_remote(
    *,
    host: str,
    remote_figures: Path,
    remote_data: Path,
    remote_analyzer: Path,
    remote_analysis: Path,
    campaign_paths: list[Path],
    local_data: Path,
    local_figures: Path,
    path_map: str,
) -> None:
    remote_out = remote_analysis / "aggregate"
    remote_session_root = remote_data / "paper1-experiments"
    manifest_args = " ".join(
        f"--campaign-manifest {shlex.quote(str(map_path(str(p), local_data, remote_data)))}"
        for p in campaign_paths
        if map_path(str(p), local_data, remote_data) is not None
    )
    batch = (
        f"cd {shlex.quote(str(remote_figures))} && "
        f"python3 tools/batch_completed_campaign.py {manifest_args} "
        f"--session-root {shlex.quote(str(remote_session_root))} "
        f"--analyzer {shlex.quote(str(remote_analyzer))} "
        f"--analysis-root {shlex.quote(str(remote_analysis / 'sessions'))} "
        f"--out {shlex.quote(str(remote_out))} "
        f"--path-map {shlex.quote(path_map)} --reuse-existing"
    )
    ssh(host, batch)
    aggregate_manifest = remote_out / "complete_campaign_manifest.json"
    aggregate_metrics = remote_out / "rotation_metrics.csv"
    family_coverage = remote_out / "family_coverage.json"
    build = (
        f"cd {shlex.quote(str(remote_figures))} && "
        f"./build-figures.sh {shlex.quote(str(remote_out / 'figure_data'))} '' "
        f"{shlex.quote(str(aggregate_manifest))} {shlex.quote(str(aggregate_metrics))} '' '' '' "
        f"{shlex.quote(str(family_coverage))}"
    )
    ssh(host, build)
    # Coverage outcomes come from the settings readback characterization. A
    # campaign aggregate has no authority to replace that measured contract.
    local_coverage = local_figures / "data/settings_coverage.json"
    if local_coverage.exists():
        remote_copy(host, local_coverage, str(remote_figures / "data/settings_coverage.json"))
        ssh(
            host,
            f"cd {shlex.quote(str(remote_figures))} && "
            "typst compile fig_settings_coverage.typ output/fig_settings_coverage.pdf && "
            "pdftoppm -png -r 180 -singlefile output/fig_settings_coverage.pdf "
            "output/fig_settings_coverage",
        )


def pull_results(host: str, remote_figures: Path, local_figures: Path) -> None:
    run(["rsync", "-a", f"{host}:{remote_figures / 'output'}/", f"{local_figures / 'output'}/"], quiet=True)
    for name in [
        "setting_response.json",
        "setting_transition.json",
        "campaign_family_coverage.json",
    ]:
        run(["scp", f"{host}:{remote_figures / 'data' / name}", str(local_figures / "data" / name)], quiet=True)


def cycle(args: argparse.Namespace, state: dict[str, str]) -> tuple[dict[str, str], list[dict[str, Any]]]:
    campaigns = [Path(value) for value in args.campaign_manifest]
    sync_campaigns(campaigns, host=args.host, local_data=args.local_data, remote_data=args.remote_data)
    complete = complete_children(campaigns)
    new = [item for item in complete if state.get(item["session_id"]) != item["digest"]]
    if not new:
        return state, []
    sync_figure_sources(args.host, args.local_figures, args.remote_figures)
    for item in new:
        sync_session(item, host=args.host, local_data=args.local_data, remote_data=args.remote_data)
    render_remote(
        host=args.host,
        remote_figures=args.remote_figures,
        remote_data=args.remote_data,
        remote_analyzer=args.remote_analyzer,
        remote_analysis=args.remote_analysis,
        campaign_paths=campaigns,
        local_data=args.local_data,
        local_figures=args.local_figures,
        path_map=f"{args.local_data}={args.remote_data}",
    )
    pull_results(args.host, args.remote_figures, args.local_figures)
    state.update({item["session_id"]: item["digest"] for item in new})
    return state, new


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--campaign-manifest", action="append", required=True)
    parser.add_argument("--local-data", type=Path, required=True)
    parser.add_argument("--local-figures", type=Path, required=True)
    parser.add_argument("--host", default="comech-2080")
    parser.add_argument("--remote-data", type=Path, required=True)
    parser.add_argument("--remote-figures", type=Path, required=True)
    parser.add_argument("--remote-analyzer", type=Path, required=True)
    parser.add_argument("--remote-analysis", type=Path, required=True)
    parser.add_argument("--state", type=Path, default=Path("/tmp/paper1_campaign_watch_state.json"))
    parser.add_argument("--interval-s", type=float, default=45.0)
    parser.add_argument("--once", action="store_true")
    args = parser.parse_args()
    state = read_json(args.state) if args.state.exists() else {}
    while True:
        state, new = cycle(args, state)
        args.state.write_text(json.dumps(state, indent=2) + "\n", encoding="utf-8")
        if new:
            print("processed completed families: " + ", ".join(str(item["family"]) for item in new), flush=True)
        if args.once:
            return 0
        time.sleep(args.interval_s)


if __name__ == "__main__":
    raise SystemExit(main())

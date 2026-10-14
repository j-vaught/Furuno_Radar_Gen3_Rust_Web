#!/usr/bin/env python3
"""Run a Paper 1 phase plan through the HTTP controller.

The runner is deliberately conservative. It will not transmit unless both
``--transmit`` and ``--physical-ready`` are present. Every successful or
exceptional exit attempts TX OFF and recording OFF, and every device response
is written to an event log before it is interpreted.
"""

from __future__ import annotations

import argparse
import base64
import datetime as dt
import json
import os
import platform
import re
import signal
import socket
import subprocess
import tempfile
import threading
import time
import urllib.error
import urllib.parse
import urllib.request
from hashlib import sha256
from pathlib import Path
from typing import Any

STOP = threading.Event()
RANGE_LABELS = {
    21: "1/16 NM",
    0: "1/8 NM",
    1: "1/4 NM",
    2: "1/2 NM",
    3: "3/4 NM",
    4: "1 NM",
    5: "1.5 NM",
    6: "2 NM",
    7: "3 NM",
    8: "4 NM",
    9: "6 NM",
    10: "8 NM",
    11: "12 NM",
    12: "16 NM",
    13: "24 NM",
    14: "32 NM",
    19: "36 NM",
    15: "48 NM",
}
REQUIRED_READBACK_FIELDS = [
    "tx_state",
    "range_index",
    "gain_auto",
    "gain_value",
    "gain_manual_value",
    "gain_auto_value",
    "sea_clutter",
    "sea_clutter_auto",
    "sea_clutter_auto_value",
    "sea_clutter_mode",
    "rain_clutter",
    "rain_clutter_auto",
    "rain_clutter_auto_value",
    "rain_clutter_ftc_mode",
    "rain_clutter_mode",
    "pulse_width",
    "pulse_range_index",
    "main_bang",
    "near_stc",
    "middle_stc",
    "far_stc",
    "antenna_speed",
    "antenna_hi_rpm",
    "bird_mode",
    "rezboost",
    "doppler",
    "interference_rejection",
    "interference_rejection_level",
    "noise_reduction",
    "noise_reduction_level",
    "antenna_height_raw",
]
# These are the fields that a phase may request the runner to write.  The
# controller also returns diagnostic and derived fields such as
# ``*_level``, ``pulse_range_index``, and ``tx_state``; those remain part of
# complete readback verification but are not writable phase settings.
PHASE_SETTING_FIELDS = frozenset(
    {
        "range_index",
        "pulse_width",
        "gain_auto",
        "gain_value",
        "gain_manual_value",
        "gain_auto_value",
        "rezboost",
        "antenna_speed",
        "antenna_hi_rpm",
        "sea_clutter",
        "sea_clutter_auto",
        "sea_clutter_auto_value",
        "sea_clutter_mode",
        "rain_clutter",
        "rain_clutter_auto",
        "rain_clutter_auto_value",
        "rain_clutter_ftc_mode",
        "rain_clutter_mode",
        "near_stc",
        "middle_stc",
        "far_stc",
        "interference_rejection",
        "noise_reduction",
        "antenna_height_raw",
        "bird_mode",
        "doppler",
        "main_bang",
    }
)
UNCHANGED_SCALAR_FIELDS = frozenset(
    {
        "range_index",
        "pulse_width",
        "near_stc",
        "middle_stc",
        "far_stc",
        "interference_rejection",
        "noise_reduction",
        "antenna_height_raw",
        "bird_mode",
        "main_bang",
        "rezboost",
    }
)


def utc_now() -> str:
    return dt.datetime.now(dt.UTC).isoformat(timespec="milliseconds")


def canonical_hash(value: Any) -> str:
    return sha256(json.dumps(value, sort_keys=True, separators=(",", ":")).encode()).hexdigest()


def sha256_path(path: Path) -> str:
    digest = sha256()
    with path.open("rb") as fp:
        for block in iter(lambda: fp.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def provenance() -> dict[str, Any]:
    source_root = Path(__file__).resolve().parents[3]
    try:
        commit = subprocess.run(
            ["git", "-C", str(source_root), "rev-parse", "HEAD"],
            check=True,
            capture_output=True,
            text=True,
            timeout=3,
        ).stdout.strip()
        dirty = bool(
            subprocess.run(
                ["git", "-C", str(source_root), "status", "--porcelain"],
                check=True,
                capture_output=True,
                text=True,
                timeout=3,
            ).stdout.strip()
        )
    except (OSError, subprocess.SubprocessError) as exc:
        return {"git_error": repr(exc), "git_commit": None, "git_dirty": None, "files": {}}
    binary_candidates: list[Path] = []
    if os.environ.get("FURUNO_SERVER_BINARY"):
        binary_candidates.append(Path(os.environ["FURUNO_SERVER_BINARY"]))
    binary_candidates.extend(
        [
            source_root / "furuno-server/target/release/furuno-server",
            source_root / "runtime/macos/furuno-server",
            source_root / "macos/furuno-server",
            source_root.parent / "runtime/macos/furuno-server",
            source_root.parent / "macos/furuno-server",
        ]
    )
    binary_path = next(
        (path for path in binary_candidates if path.is_file()),
        source_root / "furuno-server/target/release/furuno-server",
    )
    file_paths = {
        "runner": Path(__file__).resolve(),
        "analyzer": Path(__file__).with_name("analyze_capture.py").resolve(),
        "plan": source_root / "experiments/paper1/plans/paper1_experiments.json",
        "settings_schema": source_root / "experiments/paper1/schemas/settings_readback.schema.json",
        "figure_schema": source_root / "experiments/paper1/schemas/figure_data_contract.json",
        "binary": binary_path,
    }
    hashes: dict[str, Any] = {}
    for name, path in file_paths.items():
        hashes[name] = {"path": str(path), "sha256": sha256_path(path) if path.is_file() else None}
    return {"git_commit": commit, "git_dirty": dirty, "files": hashes}


def log_event(fp: Any, event: str, **fields: Any) -> dict[str, Any]:
    item = {"timestamp_utc": utc_now(), "unix_ns": time.time_ns(), "event": event, **fields}
    fp.write(json.dumps(item, sort_keys=True) + "\n")
    fp.flush()
    return item


def write_manifest_atomic(path: Path, manifest: dict[str, Any]) -> None:
    """Persist a checkpoint with an atomic same-directory replacement."""
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary_path: Path | None = None
    try:
        with tempfile.NamedTemporaryFile(
            mode="w",
            encoding="utf-8",
            dir=path.parent,
            prefix=f".{path.name}.",
            suffix=".tmp",
            delete=False,
        ) as temporary:
            temporary_path = Path(temporary.name)
            json.dump(manifest, temporary, indent=2)
            temporary.write("\n")
            temporary.flush()
            os.fsync(temporary.fileno())
        os.replace(temporary_path, path)
        temporary_path = None
    finally:
        if temporary_path is not None:
            temporary_path.unlink(missing_ok=True)


def http_request(
    base: str, path: str, params: dict[str, Any] | None = None, timeout: float = 8.0
) -> tuple[int, str]:
    query = urllib.parse.urlencode(
        {k: str(v).lower() if isinstance(v, bool) else str(v) for k, v in (params or {}).items()}
    )
    url = base.rstrip("/") + path + (("?" + query) if query else "")
    request = urllib.request.Request(url, headers={"Cache-Control": "no-cache"})
    try:
        with urllib.request.urlopen(request, timeout=timeout) as response:
            return response.status, response.read().decode("utf-8", errors="replace")
    except urllib.error.HTTPError as exc:
        return exc.code, exc.read().decode("utf-8", errors="replace")


def parse_wire_line(body: str, register: str) -> list[str] | None:
    marker = "$N" + register.upper()
    for line in body.splitlines():
        if marker in line.upper():
            raw = line[line.upper().find(marker) :].strip()
            raw = raw.split("|")[0].strip()
            return [part.strip() for part in raw.split(",")]
    return None


def integer(fields: list[str] | None, index: int) -> int | None:
    if fields is None or index >= len(fields) or not fields[index]:
        return None
    try:
        return int(fields[index])
    except ValueError:
        return None


def parse_settings_response(raw: dict[str, str]) -> dict[str, Any]:
    """Parse the current backend response strings and retain every raw line."""
    parsed: dict[str, Any] = {"raw": dict(raw)}
    tx = parse_wire_line(raw.get("TX", ""), "69")
    r62 = parse_wire_line(raw.get("RANGE", ""), "62")
    r68 = parse_wire_line(raw.get("PULSE", ""), "68")
    r63 = parse_wire_line(raw.get("GAIN", ""), "63")
    r64 = parse_wire_line(raw.get("CLUTTER", ""), "64")
    r65 = parse_wire_line(raw.get("CLUTTER", ""), "65")
    r89 = parse_wire_line(raw.get("ANTENNA", ""), "89")
    r83 = parse_wire_line(raw.get("MAINBANG", ""), "83")
    red = parse_wire_line(raw.get("BIRDMODE", ""), "ED")
    ref = parse_wire_line(raw.get("DOPPLER", ""), "EF")
    r67 = parse_wire_line(raw.get("IR", ""), "67")
    r67_noise = parse_wire_line(raw.get("NOISE", ""), "67")
    r84 = parse_wire_line(raw.get("HEIGHT", ""), "84")
    parsed.update(
        {
            "tx_state": integer(tx, 1),
            "range_index": integer(r62, 1),
            "range_label": RANGE_LABELS.get(integer(r62, 1), "unknown"),
            "pulse_width": integer(r68, 1),
            "pulse_range_index": integer(r68, 2),
            "gain_auto": integer(r63, 1),
            "gain_value": integer(r63, 2),
            "sea_status": integer(r64, 1),
            "sea_clutter": integer(r64, 2),
            "rain_status": integer(r65, 1),
            "rain_clutter": integer(r65, 2),
            "antenna_speed": integer(r89, 1),
            "antenna_hi_rpm": integer(r89, 2),
            "main_bang": integer(r83, 1),
            "bird_mode": integer(red, 1),
            "doppler": integer(ref, 1),
            "interference_rejection_level": integer(r67, 3),
            "interference_rejection": (integer(r67, 3) or 0) != 0,
            "noise_reduction_level": integer(r67_noise, 3),
            "noise_reduction": (integer(r67_noise, 3) or 0) != 0,
            "antenna_height_raw": integer(r84, 2),
        }
    )
    stc = raw.get("STC", "")
    if stc.startswith("JSON:"):
        try:
            stc_values = json.loads(stc[5:])
            parsed.update(stc_values)
            parsed["near_stc"] = stc_values.get("near")
            parsed["middle_stc"] = stc_values.get("middle")
            parsed["far_stc"] = stc_values.get("far")
        except json.JSONDecodeError:
            pass
    else:
        for name, marker in (("near_stc", "near="), ("middle_stc", "middle="), ("far_stc", "far=")):
            if marker in stc:
                try:
                    parsed[name] = int(stc.split(marker, 1)[1].split(",", 1)[0].split(")", 1)[0])
                except ValueError:
                    parsed[name] = None
    required = (
        "tx_state",
        "range_index",
        "pulse_width",
        "gain_value",
        "near_stc",
        "middle_stc",
        "far_stc",
        "interference_rejection_level",
        "noise_reduction",
        "noise_reduction_level",
        "antenna_height_raw",
    )
    parsed["confirmed"] = all(parsed.get(key) is not None for key in required)
    return parsed


def _valid_until_is_fresh(snapshot: dict[str, Any]) -> bool:
    value = snapshot.get("valid_until")
    if not isinstance(value, str) or not value:
        return False
    try:
        if value.startswith("unix_ms:"):
            return int(value.split(":", 1)[1]) > int(time.time() * 1000)
        parsed = dt.datetime.fromisoformat(value.replace("Z", "+00:00"))
        return parsed.timestamp() > time.time()
    except (TypeError, ValueError, OverflowError):
        return False


def _snapshot_result(
    info: dict[str, Any], source: str, status: int, fp: Any
) -> dict[str, Any] | None:
    snapshot = info.get("settings_readback")
    if not isinstance(snapshot, dict):
        return None
    fields = snapshot.get("fields", {})
    values = {
        name: field.get("value") if isinstance(field, dict) else None
        for name, field in fields.items()
    }
    missing = [
        name
        for name in REQUIRED_READBACK_FIELDS
        if not isinstance(fields.get(name), dict) or fields[name].get("verified") is not True
    ]
    fresh = _valid_until_is_fresh(snapshot)
    result = {
        "timestamp_utc": snapshot.get("captured_at") or utc_now(),
        "source": source,
        "confirmed": snapshot.get("verified") is True and fresh and not missing,
        "verified": snapshot.get("verified") is True,
        "fresh": fresh,
        "expired": not fresh,
        "missing_fields": missing,
        "settings": values,
        "settings_readback": snapshot,
        "raw_register_responses": snapshot.get("raw_register_responses", []),
        "controller_info": {
            key: info.get(key)
            for key in ("connected", "tx_active", "firmware", "radar_ip", "host_ip")
        },
    }
    log_event(fp, "settings_readback", http_status=status, response=result)
    return result


def readback(base: str, fp: Any, timeout: float, fresh_required: bool = False) -> dict[str, Any]:
    """Read a fresh verified snapshot and retain raw register data."""
    status, body = http_request(
        base,
        "/get_current",
        {"command": "ALL", "format": "json", "fresh": 1},
        timeout,
    )
    if status == 200:
        try:
            info = json.loads(body)
        except json.JSONDecodeError:
            info = None
        if isinstance(info, dict):
            result = _snapshot_result(info, "get_current.ALL.fresh", status, fp)
            if result is not None:
                return result

    if fresh_required:
        result = {
            "timestamp_utc": utc_now(),
            "source": "get_current.ALL.fresh",
            "confirmed": False,
            "fresh": False,
            "expired": True,
            "missing_fields": REQUIRED_READBACK_FIELDS,
            "settings": {},
            "raw_response": body,
        }
        log_event(fp, "settings_readback", http_status=status, response=result)
        return result

    # Compatibility path for older controller builds. Characterization only
    # permits the fresh aggregate endpoint above, never this cached path.
    status, body = http_request(base, "/get_system_info", timeout=timeout)
    if status == 200:
        try:
            info = json.loads(body)
        except json.JSONDecodeError:
            info = None
        if isinstance(info, dict):
            result = _snapshot_result(info, "get_system_info.settings_readback", status, fp)
            if result is not None:
                return result

    # Compatibility path for older controller builds. Live characterization refuses this
    # path unless all required parsed fields are present and confirmed.
    status, body = http_request(base, "/settings_readback", {"format": "json"}, timeout)
    if status == 200:
        try:
            payload = json.loads(body)
            if isinstance(payload, dict) and payload.get("confirmed") is True:
                result = {"timestamp_utc": utc_now(), "source": "settings_readback", **payload}
                log_event(fp, "settings_readback", http_status=status, response=result)
                return result
        except json.JSONDecodeError:
            pass

    commands = {
        "TX": "TX",
        "RANGE": "RANGE",
        "PULSE": "PULSE",
        "GAIN": "GAIN",
        "CLUTTER": "CLUTTER",
        "ANTENNA": "ANTENNA",
        "STC": "STC",
        "MAINBANG": "MAINBANG",
        "IR": "IR",
        "BIRDMODE": "BIRDMODE",
        "DOPPLER": "DOPPLER",
        "NOISE": "NOISE",
        "HEIGHT": "HEIGHT",
    }
    raw: dict[str, str] = {}
    for name, command in commands.items():
        status, text = http_request(
            base, "/get_current", {"command": command, "format": "json"}, timeout
        )
        raw[name] = text
        log_event(fp, "settings_query", command=command, http_status=status, response=text)
    result = {
        "timestamp_utc": utc_now(),
        "source": "register_readback",
        **parse_settings_response(raw),
    }
    log_event(fp, "settings_readback", http_status=200, response=result)
    return result


def readback_with_retry(
    base: str,
    fp: Any,
    timeout: float,
    context: str,
    max_attempts: int = 3,
    delay_s: float = 0.25,
) -> tuple[dict[str, Any], list[str]]:
    """Retry transient fresh-read failures without hiding an unverified result."""
    failures: list[str] = []
    result: dict[str, Any] = {
        "confirmed": False,
        "fresh": False,
        "settings": {},
        "missing_fields": REQUIRED_READBACK_FIELDS,
    }
    for attempt in range(1, max_attempts + 1):
        try:
            result = readback(base, fp, timeout, fresh_required=True)
        except BaseException as exc:
            result = {
                "confirmed": False,
                "fresh": False,
                "settings": {},
                "missing_fields": REQUIRED_READBACK_FIELDS,
            }
            reason = repr(exc)
        else:
            if result.get("confirmed") is True and result.get("fresh") is True:
                if failures:
                    log_event(
                        fp,
                        "baseline_restore_readback_recovered",
                        context=context,
                        attempts=attempt,
                        prior_errors=failures,
                    )
                return result, failures
            reason = f"unverified fresh readback; missing={result.get('missing_fields', [])}"
        failures.append(reason)
        log_event(
            fp,
            "baseline_restore_readback_retry",
            context=context,
            attempt=attempt,
            max_attempts=max_attempts,
            error=reason,
        )
        if attempt < max_attempts:
            STOP.wait(delay_s * attempt)
    return result, failures


def require_confirmed_readback(readback_result: dict[str, Any], context: str) -> None:
    if readback_result.get("confirmed") is True and readback_result.get("fresh") is True:
        return
    missing = readback_result.get("missing_fields", REQUIRED_READBACK_FIELDS)
    reason = (
        "expired or missing finite valid_until"
        if readback_result.get("fresh") is not True
        else "failed or partial verification"
    )
    raise RuntimeError(f"{context}: settings readback is {reason}; missing={missing}")


def require_transmitting_readback(result: dict[str, Any], context: str) -> None:
    """Require a fresh settings readback that indicates active transmission."""
    require_confirmed_readback(result, context)
    tx_state = _verified_settings(result).get("tx_state")
    if tx_state not in {2, 3}:
        raise RuntimeError(f"{context}: expected TX state 2 or 3, got {tx_state!r}")


def require_success(status: int, body: str, context: str) -> None:
    """Reject HTTP or textual command failures before accepting acquisition."""
    lowered = body.lower()
    if (
        status < 200
        or status >= 300
        or any(
            token in lowered
            for token in ("failed", "failure", "error", "rejected", "denied", "not recording")
        )
    ):
        raise RuntimeError(f"{context} failed: HTTP {status}; response={body!r}")


def parse_system_info(status: int, body: str, context: str) -> dict[str, Any]:
    """Parse the controller status response without treating JSON ``error:null`` as failure."""
    if status < 200 or status >= 300:
        raise RuntimeError(f"{context} failed: HTTP {status}")
    try:
        info = json.loads(body)
    except json.JSONDecodeError as exc:
        raise RuntimeError(f"{context} was not JSON") from exc
    if not isinstance(info, dict):
        raise RuntimeError(f"{context} JSON root was not an object")
    return info


def require_system_info_settings(
    info: dict[str, Any], requested: dict[str, Any], context: str
) -> dict[str, Any]:
    """Reject acquisition when the recording status snapshot is unverified or drifts."""
    if info.get("settings_verified") is not True:
        raise RuntimeError(f"{context}: controller settings_verified is not true")
    actual = info.get("settings")
    if not isinstance(actual, dict):
        raise RuntimeError(f"{context}: controller did not return full settings")
    mismatches = {
        field: {"requested": expected, "actual": actual.get(field)}
        for field, expected in requested.items()
        if actual.get(field) != expected
    }
    if mismatches:
        raise RuntimeError(f"{context}: settings drifted: {mismatches}")
    return actual


def inspect_recording_output(record_dir: Path, expected_rotations: int) -> dict[str, Any]:
    """Check recording count and lightweight furuno-web-2 CSV metadata."""
    if not record_dir.is_dir():
        raise RuntimeError(f"recording directory does not exist: {record_dir}")
    settings_path = record_dir / "settings.json"
    if not settings_path.is_file() or settings_path.stat().st_size == 0:
        raise RuntimeError(f"recording settings metadata is missing or empty: {settings_path}")
    try:
        settings_snapshot = json.loads(settings_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise RuntimeError(f"recording settings metadata is unreadable: {settings_path}") from exc
    csv_files = sorted(record_dir.glob("csv/*.csv"))
    if len(csv_files) < expected_rotations:
        raise RuntimeError(
            f"recording produced {len(csv_files)} CSV rotations; "
            f"expected at least {expected_rotations}"
        )
    metadata: list[dict[str, Any]] = []
    for path in csv_files:
        if path.stat().st_size == 0:
            raise RuntimeError(f"recording CSV is empty: {path}")
        header: dict[str, str] = {}
        try:
            with path.open(encoding="utf-8", errors="replace") as fp:
                for _ in range(96):
                    line = fp.readline()
                    if not line:
                        break
                    if not line.startswith("# "):
                        break
                    key, separator, value = line[2:].rstrip("\n").partition(",")
                    if separator:
                        header[key] = value
        except OSError as exc:
            raise RuntimeError(f"recording CSV metadata is unreadable: {path}") from exc
        if header.get("schema_version") != "furuno-web-2":
            raise RuntimeError(
                f"recording CSV has unexpected schema {header.get('schema_version')!r}: {path}"
            )
        if header.get("settings_verified", "").lower() != "true":
            raise RuntimeError(f"recording CSV settings are not verified: {path}")
        for required in ("recording_started_at", "recording_stopped_at"):
            if required not in header:
                raise RuntimeError(f"recording CSV is missing {required}: {path}")
        metadata.append(
            {
                "path": str(path),
                "bytes": path.stat().st_size,
                "schema_version": header["schema_version"],
                "settings_verified": True,
                "recording_started_at": header["recording_started_at"],
                "recording_stopped_at": header["recording_stopped_at"],
            }
        )
    return {
        "settings_path": str(settings_path),
        "settings_snapshot_schema": settings_snapshot.get("schema_version"),
        "csv_count": len(csv_files),
        "csv_files": metadata,
    }


def mounted_and_writable(root: Path) -> tuple[bool, str]:
    root = root.resolve()
    if not root.is_absolute() or not root.exists() or not root.is_dir():
        return False, "output root must be an existing absolute directory"
    try:
        probe = root / f".paper1_probe_{os.getpid()}_{time.time_ns()}"
        probe.write_text("probe", encoding="utf-8")
        probe.unlink()
    except OSError as exc:
        return False, f"output root is not writable: {exc}"
    try:
        mount_text = subprocess.run(
            ["mount"], check=False, capture_output=True, text=True, timeout=3
        ).stdout
        if platform.system() == "Darwin":
            if "/Volumes/" not in str(root) or not any(
                str(root).startswith(line.split(" on ", 1)[1].split(" (", 1)[0])
                for line in mount_text.splitlines()
                if " on /Volumes/" in line
            ):
                return (
                    False,
                    "macOS output root is not proven to be on a mounted /Volumes filesystem",
                )
        elif not mount_text:
            return False, "could not inspect mounted filesystems"
    except (OSError, subprocess.SubprocessError) as exc:
        return False, f"could not inspect mounted filesystems: {exc}"
    return True, "mounted and writable"


def receive_raw(
    path: Path,
    bind: str,
    multicast: str | None,
    peer: str | None,
    fp: Any,
    receive_interface: str | None = None,
) -> tuple[threading.Thread, dict[str, int]]:
    host, port_text = bind.rsplit(":", 1)
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    if hasattr(socket, "SO_REUSEPORT"):
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)
    sock.bind((host, int(port_text)))
    if multicast:
        membership_interface = receive_interface or "0.0.0.0"
        sock.setsockopt(
            socket.IPPROTO_IP,
            socket.IP_ADD_MEMBERSHIP,
            socket.inet_aton(multicast) + socket.inet_aton(membership_interface),
        )
    sock.settimeout(0.25)
    counts = {"packets": 0, "bytes": 0, "peer_mismatch": 0}

    def loop() -> None:
        with path.open("w", encoding="utf-8") as raw_fp:
            while not STOP.is_set():
                try:
                    payload, address = sock.recvfrom(65535)
                except TimeoutError:
                    continue
                except OSError:
                    break
                counts["packets"] += 1
                counts["bytes"] += len(payload)
                if peer and address[0] != peer:
                    counts["peer_mismatch"] += 1
                raw_fp.write(
                    json.dumps(
                        {
                            "receive_utc": utc_now(),
                            "receive_unix_ns": time.time_ns(),
                            "peer": [address[0], address[1]],
                            "payload_b64": base64.b64encode(payload).decode("ascii"),
                        }
                    )
                    + "\n"
                )
                raw_fp.flush()
        sock.close()

    thread = threading.Thread(target=loop, name="paper1-raw-receiver", daemon=True)
    thread.start()
    log_event(
        fp,
        "raw_receiver_started",
        path=str(path),
        bind=bind,
        multicast=multicast,
        peer=peer,
        receive_interface=receive_interface,
    )
    return thread, counts


def setting_requests(
    setting: str, value: Any, settings: dict[str, Any]
) -> tuple[str, dict[str, Any]]:
    if setting == "range_index":
        return "/set_range", {"rangeIndex": value, "rangeUnit": 0}
    if setting == "gain_auto":
        manual_value = settings.get("gain_manual_value", settings.get("gain_value", 128))
        auto_value = settings.get("gain_auto_value", settings.get("gain_value", 0))
        return "/set_gain", {
            "status": 1 if value else 0,
            "manualValue": manual_value,
            "autoValue": auto_value,
        }
    if setting == "gain_value":
        return "/set_gain", {
            "status": 1 if settings.get("gain_auto", False) else 0,
            "manualValue": value,
            "autoValue": settings.get("gain_auto_value", 0),
        }
    if setting == "gain_auto_value":
        return "/set_gain", {
            # Preserve the freshly verified active mode when changing the
            # inactive automatic register. Restoration temporarily enables
            # auto gain, so its current readback still yields status 1 there.
            "status": 1 if settings.get("gain_auto", False) else 0,
            "manualValue": settings.get("gain_manual_value", 128),
            "autoValue": value,
        }
    if setting == "pulse_width":
        return "/set_pulse_width", {
            "pulse": value,
            "rangeIndex": settings.get("range_index", 0),
            "rangeUnit": 0,
        }
    if setting == "sea_clutter":
        return "/set_acsea", {
            "status": 1 if settings.get("sea_clutter_auto", False) else 0,
            "manual": value,
            "auto": settings.get("sea_clutter_auto_value", 0),
            "mode": settings.get("sea_clutter_mode", 0),
        }
    if setting == "sea_clutter_auto":
        return "/set_acsea", {
            "status": 1 if value else 0,
            "manual": settings.get("sea_clutter", 0),
            "auto": settings.get("sea_clutter_auto_value", 0),
            "mode": settings.get("sea_clutter_mode", 0),
        }
    if setting == "sea_clutter_auto_value":
        return "/set_acsea", {
            "status": 1 if settings.get("sea_clutter_auto", False) else 0,
            "manual": settings.get("sea_clutter", 0),
            "auto": value,
            "mode": settings.get("sea_clutter_mode", 0),
        }
    if setting == "sea_clutter_mode":
        return "/set_acsea", {
            "status": 1 if settings.get("sea_clutter_auto", False) else 0,
            "manual": settings.get("sea_clutter", 0),
            "auto": settings.get("sea_clutter_auto_value", 0),
            "mode": value,
        }
    if setting == "rain_clutter":
        return "/set_acrain", {
            "status": 1 if settings.get("rain_clutter_auto", False) else 0,
            "manual": value,
            "auto": settings.get("rain_clutter_auto_value", 0),
            "ftcMode": settings.get("rain_clutter_ftc_mode", 0),
            "rainMode": settings.get("rain_clutter_mode", 0),
        }
    if setting == "rain_clutter_auto":
        return "/set_acrain", {
            "status": 1 if value else 0,
            "manual": settings.get("rain_clutter", 0),
            "auto": settings.get("rain_clutter_auto_value", 0),
            "ftcMode": settings.get("rain_clutter_ftc_mode", 0),
            "rainMode": settings.get("rain_clutter_mode", 0),
        }
    if setting == "rain_clutter_auto_value":
        return "/set_acrain", {
            "status": 1 if settings.get("rain_clutter_auto", False) else 0,
            "manual": settings.get("rain_clutter", 0),
            "auto": value,
            "ftcMode": settings.get("rain_clutter_ftc_mode", 0),
            "rainMode": settings.get("rain_clutter_mode", 0),
        }
    if setting == "rain_clutter_ftc_mode":
        return "/set_acrain", {
            "status": 1 if settings.get("rain_clutter_auto", False) else 0,
            "manual": settings.get("rain_clutter", 0),
            "auto": settings.get("rain_clutter_auto_value", 0),
            "ftcMode": value,
            "rainMode": settings.get("rain_clutter_mode", 0),
        }
    if setting == "rain_clutter_mode":
        return "/set_acrain", {
            "status": 1 if settings.get("rain_clutter_auto", False) else 0,
            "manual": settings.get("rain_clutter", 0),
            "auto": settings.get("rain_clutter_auto_value", 0),
            "ftcMode": settings.get("rain_clutter_ftc_mode", 0),
            "rainMode": value,
        }
    if setting == "rezboost":
        return "/set_bearing_resolution", {"mode": value}
    if setting == "antenna_speed":
        return "/set_antenna_revolution", {
            "rollSpeed": value,
            "hiRpm": 1 if settings.get("antenna_hi_rpm", value == 1) else 0,
        }
    if setting == "antenna_hi_rpm":
        return "/set_antenna_revolution", {
            "rollSpeed": settings.get("antenna_speed", 0),
            "hiRpm": 1 if value else 0,
        }
    if setting == "near_stc":
        return "/set_near_stc", {"value": value}
    if setting == "middle_stc":
        return "/set_middle_stc", {"value": value}
    if setting == "far_stc":
        return "/set_far_stc", {"value": value}
    if setting == "main_bang":
        return "/set_main_bang", {"value": value}
    if setting == "interference_rejection":
        return "/set_interference_rejection", {"enabled": 1 if value else 0}
    if setting == "noise_reduction":
        return "/set_noise_reduction", {"enabled": 1 if value else 0}
    if setting == "antenna_height_raw":
        return "/set_antenna_height", {"value": value}
    if setting == "bird_mode":
        return "/set_bird_mode", {"level": value}
    if setting == "doppler":
        return "/set_doppler", {"mode": value}
    raise ValueError(f"unsupported setting: {setting}")


def requested_matches_readback(
    requested: dict[str, Any], readback_result: dict[str, Any]
) -> dict[str, bool | None]:
    values = readback_result.get("settings", readback_result)
    matches: dict[str, bool | None] = {}
    for name, expected in requested.items():
        actual = values.get(name) if isinstance(values, dict) else None
        if actual is None:
            matches[name] = None
        elif name == "gain_auto" and isinstance(actual, int):
            matches[name] = bool(actual) == bool(expected)
        else:
            matches[name] = actual == expected
    return matches


def classify_probe(
    setting: str,
    requested: Any,
    previous: Any,
    response_status: int,
    response_body: str,
    readback_result: dict[str, Any],
) -> str:
    lower = response_body.lower()
    rejection_tokens = (
        "error",
        "failed",
        "unavailable",
        "unknown",
        "invalid",
        "noack",
        "no ack",
        "not accepted",
        "rejected",
        "unsupported",
        "denied",
        "timeout",
        "mismatch",
    )
    if (
        response_status < 200
        or response_status >= 300
        or any(token in lower for token in rejection_tokens)
    ):
        return "rejected"
    values = readback_result.get("settings", readback_result)
    actual = values.get(setting) if isinstance(values, dict) else None
    if readback_result.get("confirmed") is not True or actual is None:
        return "verification_failed"
    if (
        setting == "gain_auto"
        or setting.endswith("_auto")
        or setting in {"interference_rejection", "noise_reduction"}
        or setting == "antenna_hi_rpm"
    ):
        actual = bool(actual)
        requested = bool(requested)
        previous = bool(previous) if previous is not None else previous
    if actual == requested:
        return "supported"
    if previous is not None and actual == previous:
        return "deferred"
    return "clamped"


def issue_setting(
    base: str,
    setting: str,
    value: Any,
    settings: dict[str, Any],
    fp: Any,
    timeout: float,
    context: str,
) -> dict[str, Any]:
    path, params = setting_requests(setting, value, settings)
    event = {
        "setting": setting,
        "requested": value,
        "context": context,
        "request_path": path,
        "request_params": params,
        "command_time_utc": utc_now(),
    }
    status, body = http_request(base, path, params, timeout)
    event.update({"http_status": status, "response": body, "ack_time_utc": utc_now()})
    log_event(fp, "setting_command", **event)
    return event


def characterize_controls(
    base: str,
    spec: dict[str, Any],
    fp: Any,
    timeout: float,
) -> dict[str, Any]:
    baseline = readback(base, fp, timeout, fresh_required=True)
    require_confirmed_readback(baseline, "characterization baseline")
    baseline_values = dict(baseline.get("settings", baseline))
    if baseline_values.get("tx_state") != 1:
        raise RuntimeError(
            "standby-only characterization requires TX state 1, "
            f"got {baseline_values.get('tx_state')}"
        )
    working = dict(baseline_values)
    pulse_baselines: dict[str, dict[str, Any]] = {}
    outcomes: list[dict[str, Any]] = []

    def refresh(context: str) -> dict[str, Any]:
        result = readback(base, fp, timeout, fresh_required=True)
        require_confirmed_readback(result, context)
        if result.get("settings", {}).get("tx_state") != 1:
            raise RuntimeError(f"TX left standby during {context}")
        return result

    def probe(
        setting: str,
        requested: Any,
        context: str,
        extra: dict[str, Any] | None = None,
        allow_stop: bool = False,
    ) -> dict[str, Any]:
        nonlocal working
        if STOP.is_set() and not allow_stop:
            raise InterruptedError("stop requested before setting probe")
        settings_for_command = dict(working)
        if extra:
            settings_for_command.update(extra)
        previous = working.get(setting)
        try:
            event = issue_setting(
                base, setting, requested, settings_for_command, fp, timeout, context
            )
        except BaseException as exc:
            record = {
                "context": context,
                "setting": setting,
                "requested": requested,
                "previous": previous,
                "outcome": "rejected",
                "command": {"error": repr(exc)},
                "readback": None,
                "actual": None,
                "error": repr(exc),
            }
            outcomes.append(record)
            raise
        try:
            result = refresh(f"readback after {context}")
        except BaseException as exc:
            record = {
                "context": context,
                "setting": setting,
                "requested": requested,
                "previous": previous,
                "outcome": "verification_failed",
                "command": event,
                "readback": None,
                "actual": None,
                "error": repr(exc),
            }
            outcomes.append(record)
            raise
        values = dict(result.get("settings", {}))
        outcome = classify_probe(
            setting, requested, previous, event["http_status"], event["response"], result
        )
        record = {
            "context": context,
            "setting": setting,
            "requested": requested,
            "previous": previous,
            "outcome": outcome,
            "command": event,
            "readback": result,
            "actual": values.get(setting),
        }
        outcomes.append(record)
        working.update(values)
        return record

    def run_probe_sequence() -> None:
        for control in spec.get("controls", []):
            setting = control["setting"]
            for key, expected in control.get("precondition", {}).items():
                if working.get(key) != expected:
                    probe(key, expected, f"precondition:{control['id']}:{key}")
            if setting == "pulse_width":
                contexts = [
                    (range_index, value)
                    for range_index in RANGE_LABELS
                    for value in control["values"]
                ]
            elif setting == "antenna_speed" and control.get("companion_setting"):
                contexts = [
                    (value, companion)
                    for value in control["values"]
                    for companion in control["companion_values"]
                ]
            else:
                contexts = [(None, value) for value in control["values"]]
            for context_value, requested in contexts:
                if setting == "pulse_width":
                    if context_value is None:
                        raise RuntimeError("pulse width characterization requires a range context")
                    range_index = int(context_value)
                    if working.get("range_index") != range_index:
                        probe(
                            "range_index",
                            range_index,
                            f"context:{control['id']}:range:{range_index}",
                        )
                    if working.get("range_index") != range_index:
                        for pulse_value in control["values"]:
                            outcomes.append(
                                {
                                    "context": (
                                        f"{control['id']}:range:{range_index}:value:{pulse_value}"
                                    ),
                                    "setting": "pulse_width",
                                    "requested": pulse_value,
                                    "previous": working.get("pulse_width"),
                                    "actual": working.get("pulse_width"),
                                    "outcome": "not_applicable",
                                    "reason": (
                                        "requested range was not active after verified readback"
                                    ),
                                    "requested_range_index": range_index,
                                    "actual_range_index": working.get("range_index"),
                                }
                            )
                        continue
                    if str(range_index) not in pulse_baselines:
                        current = refresh(f"pulse baseline range {range_index}")
                        pulse_baselines[str(range_index)] = {
                            "pulse_width": current["settings"].get("pulse_width"),
                            "pulse_range_index": current["settings"].get("pulse_range_index"),
                        }
                    probe(
                        setting,
                        requested,
                        f"{control['id']}:range:{range_index}:value:{requested}",
                    )
                elif setting == "antenna_speed" and control.get("companion_setting"):
                    probe(
                        "antenna_speed",
                        context_value,
                        f"{control['id']}:speed:{context_value}:hi_rpm:{requested}",
                        {"antenna_hi_rpm": requested},
                    )
                    if working.get("antenna_hi_rpm") != requested:
                        probe(
                            "antenna_hi_rpm",
                            requested,
                            f"{control['id']}:hi_rpm:{requested}",
                            {"antenna_speed": context_value},
                        )
                else:
                    probe(setting, requested, f"{control['id']}:value:{requested}")

    def restore_baseline() -> dict[str, Any]:
        return restore_verified_baseline(
            base,
            baseline,
            fp,
            timeout,
            "characterization-baseline",
            pulse_states=pulse_baselines,
        )

    run_error: BaseException | None = None
    try:
        run_probe_sequence()
    except BaseException as exc:
        run_error = exc
        log_event(fp, "characterization_error", error=repr(exc), outcome_count=len(outcomes))
    try:
        restore = restore_baseline()
    except BaseException as exc:
        # Keep the probe records and the original probe exception even if an
        # unexpected restoration failure occurs while the controller is down.
        log_event(fp, "restore_error", setting="<restore_baseline>", error=repr(exc))
        restore = {
            "final_readback": None,
            "restore_matches": {field: False for field in REQUIRED_READBACK_FIELDS},
            "restore_complete": False,
            "restore_errors": [{"setting": "<restore_baseline>", "error": repr(exc)}],
            "restore_outcomes": [],
        }
    result = {
        "baseline": baseline,
        "outcomes": outcomes,
        "pulse_baselines_by_range": pulse_baselines,
        "final_readback": restore["final_readback"],
        "restore_matches": restore["restore_matches"],
        "restore_complete": restore["restore_complete"],
        "restore_errors": restore["restore_errors"],
        "restore_outcomes": restore["restore_outcomes"],
    }
    if run_error is not None:
        result["error"] = repr(run_error)
        result["_exception"] = run_error
    return result


def apply_phase(base: str, phase: dict[str, Any], fp: Any, timeout: float) -> list[dict[str, Any]]:
    changes: list[dict[str, Any]] = []
    settings = dict(phase.get("settings", {}))
    current_result = readback(base, fp, timeout, fresh_required=True)
    require_confirmed_readback(current_result, f"phase {phase.get('id', '<unknown>')} pre-apply")
    current = _verified_settings(current_result)
    if current.get("tx_state") != 1:
        raise RuntimeError("phase settings may only be applied while TX is in standby")

    # Doppler mode can force gain and sea-clutter modes on the installed unit.
    # Apply it first, then refresh the complete snapshot before constructing
    # grouped register payloads. An unchanged mode is deliberately skipped so
    # a reference phase cannot trigger those side effects again.
    if "doppler" in settings:
        requested_doppler = settings.pop("doppler")
        if current.get("doppler") == requested_doppler:
            event = {
                "setting": "doppler",
                "requested": requested_doppler,
                "actual": current.get("doppler"),
                "actual_source": "pre_apply_fresh_readback",
                "skipped": True,
                "reason": "fresh readback already matched requested mode",
                "command_time_utc": utc_now(),
            }
            log_event(fp, "setting_skipped", **event)
            changes.append(event)
        else:
            path, params = setting_requests("doppler", requested_doppler, current)
            status, body = http_request(base, path, params, timeout)
            require_success(status, body, f"phase {phase.get('id', '<unknown>')} doppler")
            event = {
                "setting": "doppler",
                "requested": requested_doppler,
                "request_path": path,
                "request_params": params,
                "http_status": status,
                "response": body,
                "command_time_utc": utc_now(),
                "ack_time_utc": utc_now(),
            }
            log_event(fp, "setting_command", **event)
            changes.append(event)
            current_result = readback(base, fp, timeout, fresh_required=True)
            require_confirmed_readback(
                current_result, f"phase {phase.get('id', '<unknown>')} post-doppler readback"
            )
            current = _verified_settings(current_result)

    groups = {
        "gain_register": {"gain_auto", "gain_value", "gain_manual_value", "gain_auto_value"},
        "sea_clutter_register": {
            "sea_clutter",
            "sea_clutter_auto",
            "sea_clutter_auto_value",
            "sea_clutter_mode",
        },
        "rain_clutter_register": {
            "rain_clutter",
            "rain_clutter_auto",
            "rain_clutter_auto_value",
            "rain_clutter_ftc_mode",
            "rain_clutter_mode",
        },
        "antenna_register": {"antenna_speed", "antenna_hi_rpm"},
    }
    grouped: dict[str, dict[str, Any]] = {}
    ordered: list[tuple[str, Any]] = []
    seen_groups: set[str] = set()
    range_changed = (
        "range_index" in settings and current.get("range_index") != settings["range_index"]
    )
    for setting, value in settings.items():
        group = next((name for name, members in groups.items() if setting in members), None)
        if group is not None:
            if group not in seen_groups:
                seen_groups.add(group)
                ordered.append((group, None))
            grouped.setdefault(group, {})[setting] = value
        else:
            ordered.append((setting, value))

    def group_request(
        group: str, desired: dict[str, Any]
    ) -> tuple[str, dict[str, Any], dict[str, Any]]:
        if group == "gain_register":
            target = {
                "status": int(bool(desired.get("gain_auto", current.get("gain_auto", False)))),
                "manualValue": desired.get(
                    "gain_manual_value",
                    desired.get(
                        "gain_value", current.get("gain_manual_value", current.get("gain_value", 0))
                    ),
                ),
                "autoValue": desired.get("gain_auto_value", current.get("gain_auto_value", 0)),
            }
            actual = {
                "status": int(bool(current.get("gain_auto"))),
                "manualValue": current.get("gain_manual_value"),
                "autoValue": current.get("gain_auto_value"),
            }
            return "/set_gain", target, actual
        if group == "sea_clutter_register":
            target = {
                "status": int(
                    bool(desired.get("sea_clutter_auto", current.get("sea_clutter_auto", False)))
                ),
                "manual": desired.get("sea_clutter", current.get("sea_clutter", 0)),
                "auto": desired.get(
                    "sea_clutter_auto_value", current.get("sea_clutter_auto_value", 0)
                ),
                "mode": desired.get("sea_clutter_mode", current.get("sea_clutter_mode", 0)),
            }
            actual = {
                "status": int(bool(current.get("sea_clutter_auto"))),
                "manual": current.get("sea_clutter"),
                "auto": current.get("sea_clutter_auto_value"),
                "mode": current.get("sea_clutter_mode"),
            }
            return "/set_acsea", target, actual
        if group == "rain_clutter_register":
            target = {
                "status": int(
                    bool(desired.get("rain_clutter_auto", current.get("rain_clutter_auto", False)))
                ),
                "manual": desired.get("rain_clutter", current.get("rain_clutter", 0)),
                "auto": desired.get(
                    "rain_clutter_auto_value", current.get("rain_clutter_auto_value", 0)
                ),
                "ftcMode": desired.get(
                    "rain_clutter_ftc_mode", current.get("rain_clutter_ftc_mode", 0)
                ),
                "rainMode": desired.get("rain_clutter_mode", current.get("rain_clutter_mode", 0)),
            }
            actual = {
                "status": int(bool(current.get("rain_clutter_auto"))),
                "manual": current.get("rain_clutter"),
                "auto": current.get("rain_clutter_auto_value"),
                "ftcMode": current.get("rain_clutter_ftc_mode"),
                "rainMode": current.get("rain_clutter_mode"),
            }
            return "/set_acrain", target, actual
        target = {
            "rollSpeed": desired.get("antenna_speed", current.get("antenna_speed", 0)),
            "hiRpm": int(bool(desired.get("antenna_hi_rpm", current.get("antenna_hi_rpm", False)))),
        }
        actual = {
            "rollSpeed": current.get("antenna_speed"),
            "hiRpm": int(bool(current.get("antenna_hi_rpm"))),
        }
        return "/set_antenna_revolution", target, actual

    for setting, value in ordered:
        if setting in grouped:
            path, params, actual = group_request(setting, grouped[setting])
            requested: Any = dict(grouped[setting])
            if params == actual:
                event = {
                    "setting": setting,
                    "requested": requested,
                    "actual": actual,
                    "actual_source": "pre_apply_fresh_readback",
                    "request_path": path,
                    "request_params": params,
                    "skipped": True,
                    "reason": "fresh readback already matched full register payload",
                    "command_time_utc": utc_now(),
                }
                log_event(fp, "setting_skipped", **event)
                changes.append(event)
                continue
        else:
            scalar_actual = current.get(setting)
            matches_current = (
                bool(scalar_actual) == bool(value)
                if setting in {"interference_rejection", "noise_reduction"}
                else scalar_actual == value
            )
            pulse_context_matches = current.get("pulse_range_index") == settings.get(
                "range_index", current.get("range_index")
            )
            can_skip = setting in UNCHANGED_SCALAR_FIELDS and matches_current
            if setting == "pulse_width":
                # Pulse codes are persisted per range. A range transition
                # therefore requires the pulse command even when its numeric
                # code is unchanged.
                can_skip = can_skip and not range_changed and pulse_context_matches
            if can_skip:
                event = {
                    "setting": setting,
                    "requested": value,
                    "actual": scalar_actual,
                    "actual_source": "pre_apply_fresh_readback",
                    "skipped": True,
                    "reason": "fresh readback already matched requested state",
                    "command_time_utc": utc_now(),
                }
                log_event(fp, "setting_skipped", **event)
                changes.append(event)
                continue
            request = setting_requests(setting, value, {**current, **settings})
            if request is None:
                continue
            path, params = request
            requested = value
        requests_to_send: list[tuple[dict[str, Any], Any]] = [(params, requested)]
        if (
            setting == "gain_register"
            and params.get("status") != actual.get("status")
            and params.get("manualValue") is not None
            and params.get("autoValue") is not None
        ):
            # Preserve both gain registers by first writing the inactive value
            # while its mode is active, then select the requested mode.
            staged_params = dict(params)
            staged_params["status"] = actual["status"]
            if staged_params != actual:
                requests_to_send.insert(
                    0,
                    (
                        staged_params,
                        {**requested, "staged_mode": bool(actual["status"])},
                    ),
                )
        for request_params, request_value in requests_to_send:
            sent = {
                "setting": setting,
                "requested": request_value,
                "request_path": path,
                "request_params": request_params,
                "command_time_utc": utc_now(),
            }
            status, body = http_request(base, path, request_params, timeout)
            sent.update({"http_status": status, "response": body, "ack_time_utc": utc_now()})
            log_event(fp, "setting_command", **sent)
            changes.append(sent)
    return changes


def restore_settings_baseline(
    base: str, baseline: dict[str, Any], fp: Any, timeout: float, context: str
) -> dict[str, Any]:
    """Restore through fresh, field-verified readbacks before every command."""
    report = restore_verified_baseline(base, baseline, fp, timeout, context)
    if not report["restore_complete"]:
        raise RuntimeError(f"{context} mismatch: {report['restore_matches']}\n{report}")
    return report["final_readback"]


def _verified_settings(result: dict[str, Any]) -> dict[str, Any]:
    """Return only fields verified by the most recent controller response."""
    fields = result.get("settings_readback", {}).get("fields")
    if isinstance(fields, dict):
        return {
            name: field.get("value")
            for name, field in fields.items()
            if isinstance(field, dict) and field.get("verified") is True
        }
    if result.get("confirmed") is True and result.get("fresh") is True:
        values = result.get("settings", result)
        return dict(values) if isinstance(values, dict) else {}
    return {}


def _restore_requirements(setting: str) -> set[str]:
    """Fields required from the current fresh readback for one command."""
    if setting in {"gain_auto", "gain_value", "gain_auto_value"}:
        return {"tx_state", "gain_auto"}
    if setting.startswith("sea_clutter"):
        return {"tx_state", "sea_clutter_auto"}
    if setting.startswith("rain_clutter"):
        return {"tx_state", "rain_clutter_auto"}
    if setting in {"antenna_speed", "antenna_hi_rpm"}:
        return {"tx_state", "antenna_speed", "antenna_hi_rpm"}
    if setting == "pulse_width":
        return {"tx_state", "range_index"}
    return {"tx_state", setting}


def _restore_grouped_baseline(
    base: str,
    target: dict[str, Any],
    fp: Any,
    timeout: float,
    context: str,
) -> dict[str, Any]:
    """Restore an ordinary phase baseline with minimal full-register writes."""
    outcomes: list[dict[str, Any]] = []
    errors: list[dict[str, Any]] = []
    current_result, initial_readback_errors = readback_with_retry(
        base, fp, timeout, f"{context}:initial_readback"
    )
    if initial_readback_errors and current_result.get("confirmed") is not True:
        errors.extend(
            {"setting": "<initial_readback>", "error": error} for error in initial_readback_errors
        )
    current = _verified_settings(current_result)
    if current_result.get("confirmed") is not True or current_result.get("fresh") is not True:
        errors.append(
            {
                "setting": "<initial_readback>",
                "error": "fresh confirmed readback unavailable",
            }
        )

    def record_skip(setting: str, desired: Any, reason: str) -> None:
        outcome = {
            "setting": setting,
            "requested": desired,
            "outcome": "unchanged",
            "reason": reason,
        }
        outcomes.append(outcome)
        log_event(fp, "baseline_restore_skipped", context=context, **outcome)

    def perform(
        setting: str,
        desired: Any,
        command_settings: dict[str, Any],
        required: set[str],
    ) -> bool:
        nonlocal current_result, current
        if current_result.get("confirmed") is not True or current_result.get("fresh") is not True:
            errors.append({"setting": setting, "error": "fresh readback unavailable"})
            return False
        if current.get("tx_state") != 1:
            errors.append({"setting": setting, "error": "TX state was not verified as standby"})
            return False
        missing = sorted(field for field in required if field not in current)
        if missing:
            errors.append(
                {"setting": setting, "error": "missing verified fields", "missing": missing}
            )
            return False
        try:
            event = issue_setting(
                base, setting, desired, command_settings, fp, timeout, f"{context}:{setting}"
            )
        except BaseException as exc:
            errors.append({"setting": setting, "error": repr(exc)})
            outcomes.append(
                {
                    "setting": setting,
                    "requested": desired,
                    "outcome": "rejected",
                    "error": repr(exc),
                }
            )
            return False
        current_result, readback_errors = readback_with_retry(
            base, fp, timeout, f"{context}:{setting}:post_command"
        )
        if readback_errors and current_result.get("confirmed") is not True:
            errors.extend({"setting": setting, "error": error} for error in readback_errors)
        current = _verified_settings(current_result)
        rejected = event.get("http_status", 500) < 200 or event.get("http_status", 500) >= 300
        body = event.get("response", "")
        if isinstance(body, str):
            rejected = rejected or any(
                token in body.lower()
                for token in ("error", "failed", "rejected", "invalid", "noack", "timeout")
            )
        actual = current.get(setting)
        outcome_name = "supported" if not rejected and actual == desired else "rejected"
        if current_result.get("confirmed") is not True or current_result.get("fresh") is not True:
            outcome_name = "verification_failed"
        outcome = {
            "setting": setting,
            "requested": desired,
            "actual": actual,
            "outcome": outcome_name,
            "command": event,
        }
        outcomes.append(outcome)
        if outcome_name != "supported":
            errors.append({"setting": setting, "error": outcome_name, "actual": actual})
            return False
        return True

    if current_result.get("confirmed") is True and current_result.get("fresh") is True:
        if current.get("tx_state") != 1:
            errors.append({"setting": "<initial_readback>", "error": "TX state was not standby"})
        else:
            target_doppler = target.get("doppler")
            if target_doppler is not None and current.get("doppler") not in {None, 0}:
                perform("doppler", 0, target, {"tx_state", "doppler"})
            # Scalar settings are only sent when the verified value differs.
            for setting in ("range_index", "pulse_width"):
                if setting in target and current.get(setting) != target[setting]:
                    if not perform(
                        setting, target[setting], target, _restore_requirements(setting)
                    ):
                        break
            gain_manual = target.get("gain_manual_value", target.get("gain_value"))
            gain_auto_value = target.get("gain_auto_value")
            if (
                gain_manual is not None
                and target.get("gain_auto") is not None
                and (
                    current.get("gain_auto") != target.get("gain_auto")
                    or current.get("gain_manual_value") != gain_manual
                    or current.get("gain_auto_value") != gain_auto_value
                )
            ):
                gain_settings = dict(target)
                gain_settings["gain_manual_value"] = gain_manual
                gain_settings["gain_auto_value"] = gain_auto_value
                if bool(target["gain_auto"]):
                    perform("gain_auto", True, gain_settings, {"tx_state", "gain_auto"})
                elif (
                    current.get("gain_auto") is False
                    and current.get("gain_auto_value") != gain_auto_value
                ):
                    perform("gain_auto", True, gain_settings, {"tx_state", "gain_auto"})
                    if current_result.get("confirmed") is True:
                        perform("gain_auto", False, gain_settings, {"tx_state", "gain_auto"})
                else:
                    perform("gain_auto", False, gain_settings, {"tx_state", "gain_auto"})
            else:
                record_skip(
                    "gain_register",
                    target.get("gain_auto"),
                    "fresh readback matched all gain fields",
                )

            for prefix, mode_setting, required in (
                ("sea_clutter", "sea_clutter_auto", {"tx_state", "sea_clutter_auto"}),
                ("rain_clutter", "rain_clutter_auto", {"tx_state", "rain_clutter_auto"}),
            ):
                fields = (
                    mode_setting,
                    prefix,
                    f"{prefix}_auto_value",
                    f"{prefix}_mode",
                )
                if prefix == "rain_clutter":
                    fields += ("rain_clutter_ftc_mode",)
                if all(field not in target for field in fields):
                    continue
                if any(
                    current.get(field) != target.get(field) for field in fields if field in target
                ):
                    perform(mode_setting, bool(target.get(mode_setting)), dict(target), required)
                else:
                    record_skip(
                        f"{prefix}_register",
                        target.get(mode_setting),
                        "fresh readback matched full register",
                    )

            if any(
                current.get(field) != target.get(field)
                for field in ("antenna_speed", "antenna_hi_rpm")
                if field in target
            ):
                perform(
                    "antenna_speed",
                    target.get("antenna_speed"),
                    dict(target),
                    {"tx_state", "antenna_speed", "antenna_hi_rpm"},
                )
            else:
                record_skip(
                    "antenna_register",
                    target.get("antenna_speed"),
                    "fresh readback matched full register",
                )

            for setting in (
                "near_stc",
                "middle_stc",
                "far_stc",
                "rezboost",
                "interference_rejection",
                "noise_reduction",
                "antenna_height_raw",
                "bird_mode",
                "main_bang",
            ):
                if setting in target and current.get(setting) != target[setting]:
                    if not perform(setting, target[setting], target, {"tx_state", setting}):
                        break
            if target_doppler is not None and target_doppler != 0:
                if current.get("doppler") != target_doppler:
                    perform("doppler", target_doppler, target, {"tx_state", "doppler"})

    final, final_readback_errors = readback_with_retry(
        base, fp, timeout, f"{context}:final_readback"
    )
    if final_readback_errors and final.get("confirmed") is not True:
        errors.extend(
            {"setting": "<final_readback>", "error": error} for error in final_readback_errors
        )
    final_values = _verified_settings(final)
    matches = {
        field: (final_values.get(field) == expected)
        if field in final_values and expected is not None
        else False
        for field, expected in target.items()
        if field in REQUIRED_READBACK_FIELDS and expected is not None
    }
    complete = (
        final.get("confirmed") is True
        and final.get("fresh") is True
        and not errors
        and all(matches.values())
    )
    return {
        "final_readback": final,
        "restore_matches": matches,
        "restore_complete": complete,
        "restore_errors": errors,
        "restore_outcomes": outcomes,
    }


def restore_verified_baseline(
    base: str,
    baseline: dict[str, Any],
    fp: Any,
    timeout: float,
    context: str,
    pulse_states: dict[str, dict[str, Any]] | None = None,
) -> dict[str, Any]:
    """Restore a baseline without constructing commands from stale state.

    Every individual write is preceded by a fresh aggregate readback. A partial
    response causes that command to be recorded as skipped, while independent
    commands may continue. The desired command payload comes only from the
    captured baseline, never from a previous working dictionary.
    """
    values = baseline.get("settings", baseline)
    target = dict(values) if isinstance(values, dict) else {}
    if not pulse_states:
        return _restore_grouped_baseline(base, target, fp, timeout, context)
    outcomes: list[dict[str, Any]] = []
    errors: list[dict[str, Any]] = []
    command_order: list[tuple[str, Any]] = []

    def add(setting: str, value: Any) -> None:
        if value is not None:
            command_order.append((setting, value))

    # Mode-sensitive gain restoration stages the hidden value in the opposite
    # mode, then restores the requested mode and visible value.
    desired_gain_auto = target.get("gain_auto")
    if desired_gain_auto is not None:
        if bool(desired_gain_auto):
            add("gain_auto", False)
            add("gain_value", target.get("gain_manual_value"))
            add("gain_auto", True)
            add("gain_auto_value", target.get("gain_auto_value"))
        else:
            add("gain_auto", True)
            add("gain_auto_value", target.get("gain_auto_value"))
            add("gain_auto", False)
            add("gain_value", target.get("gain_manual_value"))

    for setting in ("range_index",):
        add(setting, target.get(setting))
    # Clutter values are staged in manual mode so a write cannot be applied to
    # the wrong automatic/manual register. The final write restores the mode.
    if target.get("sea_clutter_auto") is not None:
        add("sea_clutter_auto", False)
        add("sea_clutter", target.get("sea_clutter"))
        add("sea_clutter_auto_value", target.get("sea_clutter_auto_value"))
        add("sea_clutter_mode", target.get("sea_clutter_mode"))
        add("sea_clutter_auto", target.get("sea_clutter_auto"))
    if target.get("rain_clutter_auto") is not None:
        add("rain_clutter_auto", False)
        add("rain_clutter", target.get("rain_clutter"))
        add("rain_clutter_auto_value", target.get("rain_clutter_auto_value"))
        add("rain_clutter_ftc_mode", target.get("rain_clutter_ftc_mode"))
        add("rain_clutter_mode", target.get("rain_clutter_mode"))
        add("rain_clutter_auto", target.get("rain_clutter_auto"))
    for setting in (
        "near_stc",
        "middle_stc",
        "far_stc",
        "rezboost",
        "interference_rejection",
        "noise_reduction",
        "antenna_height_raw",
        "bird_mode",
        "main_bang",
        "antenna_speed",
        "antenna_hi_rpm",
    ):
        add(setting, target.get(setting))
    # Characterization can visit several active range contexts. Restore every
    # observed pulse register with a fresh range readback, then finish at the
    # captured baseline range and pulse value.
    range_position = next(
        (position for position, item in enumerate(command_order) if item[0] == "range_index"),
        len(command_order),
    )
    if target.get("pulse_width") is not None:
        command_order.insert(range_position + 1, ("pulse_width", target.get("pulse_width")))
    if pulse_states:
        insert_at = range_position + (2 if target.get("pulse_width") is not None else 1)
        for range_text, state in pulse_states.items():
            range_value = int(range_text)
            pulse_value = state.get("pulse_width")
            if pulse_value is None:
                continue
            command_order.insert(insert_at, ("range_index", range_value))
            command_order.insert(insert_at + 1, ("pulse_width", pulse_value))
            insert_at += 2
        # Ensure final settings use the original baseline context.
        if target.get("range_index") is not None:
            command_order.append(("range_index", target.get("range_index")))
        if target.get("pulse_width") is not None:
            command_order.append(("pulse_width", target.get("pulse_width")))

    target_doppler = target.get("doppler")
    if target_doppler is not None:
        command_order.insert(0, ("doppler", 0))
        if target_doppler != 0:
            command_order.append(("doppler", target_doppler))

    active_range: Any = None
    for index, (setting, desired) in enumerate(command_order):
        current_result, readback_errors = readback_with_retry(
            base, fp, timeout, f"{context}:{index}:{setting}"
        )
        if readback_errors and current_result.get("confirmed") is not True:
            errors.extend(
                {"setting": setting, "index": index, "error": error} for error in readback_errors
            )
        current = _verified_settings(current_result)
        required = _restore_requirements(setting)
        missing = sorted(field for field in required if field not in current)
        if current.get("tx_state") != 1:
            reason = "TX state was not freshly verified as standby"
            outcome = {
                "setting": setting,
                "requested": desired,
                "outcome": "skipped",
                "reason": reason,
            }
            outcomes.append(outcome)
            errors.append({"setting": setting, "index": index, "error": reason})
            log_event(fp, "baseline_restore_skipped", context=context, **outcome)
            # Sending another command while TX state is unknown or active is unsafe.
            break
        if missing:
            outcome = {
                "setting": setting,
                "requested": desired,
                "outcome": "skipped",
                "reason": "fresh readback missing verified fields",
                "missing_fields": missing,
            }
            outcomes.append(outcome)
            errors.append(
                {
                    "setting": setting,
                    "index": index,
                    "error": "missing verified fields",
                    "missing": missing,
                }
            )
            log_event(fp, "baseline_restore_skipped", context=context, **outcome)
            continue

        if setting == "doppler" and current.get("doppler") == desired:
            outcome = {
                "setting": setting,
                "requested": desired,
                "outcome": "unchanged",
                "reason": "fresh readback already matched requested mode",
            }
            outcomes.append(outcome)
            log_event(fp, "baseline_restore_skipped", context=context, **outcome)
            continue

        if setting == "pulse_width" and active_range != current.get("range_index"):
            reason = "active range did not match the requested pulse context"
            outcome = {
                "setting": setting,
                "requested": desired,
                "outcome": "skipped",
                "reason": reason,
                "requested_range_index": active_range,
                "actual_range_index": current.get("range_index"),
            }
            outcomes.append(outcome)
            errors.append({"setting": setting, "index": index, "error": reason})
            log_event(fp, "baseline_restore_skipped", context=context, **outcome)
            continue

        mode_guard: tuple[str, str] | None = None
        if setting == "gain_value" and current.get("gain_auto") is not False:
            mode_guard = ("gain_auto", "manual gain mode was not freshly verified")
        elif setting in {"sea_clutter", "rain_clutter"}:
            mode_field = f"{setting}_auto"
            if current.get(mode_field) is not False:
                mode_guard = (mode_field, "manual clutter mode was not freshly verified")
        if mode_guard is not None:
            field, reason = mode_guard
            outcome = {
                "setting": setting,
                "requested": desired,
                "outcome": "skipped",
                "reason": reason,
                "guard_field": field,
            }
            outcomes.append(outcome)
            errors.append({"setting": setting, "index": index, "error": reason})
            log_event(fp, "baseline_restore_skipped", context=context, **outcome)
            continue

        # All coupled payload values are desired baseline values. The current
        # snapshot supplies only verified mode/companion fields, never stale
        # working state. Desired scalar values remain from the baseline.
        command_settings = dict(target)
        for field in (
            "gain_auto",
            "gain_manual_value",
            "gain_auto_value",
            "sea_clutter_auto",
            "sea_clutter_mode",
            "rain_clutter_auto",
            "rain_clutter_ftc_mode",
            "rain_clutter_mode",
            "antenna_speed",
            "antenna_hi_rpm",
        ):
            if field in current:
                command_settings[field] = current[field]
        if setting == "gain_auto" and "gain_manual_value" not in current:
            command_settings["gain_manual_value"] = target.get(
                "gain_manual_value", target.get("gain_value", 128)
            )
        try:
            event = issue_setting(
                base, setting, desired, command_settings, fp, timeout, f"{context}:{index}"
            )
        except BaseException as exc:
            outcome = {
                "setting": setting,
                "requested": desired,
                "outcome": "rejected",
                "error": repr(exc),
            }
            outcomes.append(outcome)
            errors.append({"setting": setting, "index": index, "error": repr(exc)})
            log_event(fp, "baseline_restore_error", context=context, **outcome)
            continue
        after, after_readback_errors = readback_with_retry(
            base, fp, timeout, f"{context}:{index}:{setting}:post_command"
        )
        if after_readback_errors and after.get("confirmed") is not True:
            errors.extend(
                {"setting": setting, "index": index, "error": error}
                for error in after_readback_errors
            )
        actual = _verified_settings(after).get(setting)
        rejected = event.get("http_status", 500) < 200 or event.get("http_status", 500) >= 300
        if isinstance(event.get("response"), str):
            rejected = rejected or any(
                token in event["response"].lower()
                for token in (
                    "error",
                    "failed",
                    "rejected",
                    "unknown",
                    "invalid",
                    "noack",
                    "no ack",
                    "timeout",
                )
            )
        if after.get("confirmed") is not True or actual is None:
            outcome_name = "verification_failed"
        elif rejected:
            outcome_name = "rejected"
        elif setting in {
            "gain_auto",
            "interference_rejection",
            "noise_reduction",
            "antenna_hi_rpm",
        } or (setting.endswith("_auto")):
            outcome_name = "supported" if bool(actual) == bool(desired) else "clamped"
        else:
            outcome_name = "supported" if actual == desired else "clamped"
        outcome = {
            "setting": setting,
            "requested": desired,
            "actual": actual,
            "outcome": outcome_name,
            "command": event,
        }
        outcomes.append(outcome)
        if outcome_name != "supported":
            errors.append(
                {"setting": setting, "index": index, "error": outcome_name, "actual": actual}
            )
        if setting == "range_index" and outcome_name == "supported":
            active_range = desired

    final, final_readback_errors = readback_with_retry(
        base, fp, timeout, f"{context}:final_readback"
    )
    if final_readback_errors and final.get("confirmed") is not True:
        errors.extend(
            {"setting": "<final_readback>", "error": error} for error in final_readback_errors
        )
    final_values = _verified_settings(final)
    matches = {
        field: (final_values.get(field) == target.get(field))
        if field in target and target.get(field) is not None
        else True
        for field in REQUIRED_READBACK_FIELDS
    }
    complete = (
        final.get("confirmed") is True
        and final.get("fresh") is True
        and not errors
        and all(matches.values())
    )
    return {
        "final_readback": final,
        "restore_matches": matches,
        "restore_complete": complete,
        "restore_errors": errors,
        "restore_outcomes": outcomes,
    }


def load_json(path: Path) -> dict[str, Any]:
    with path.open(encoding="utf-8") as fp:
        value = json.load(fp)
    if not isinstance(value, dict):
        raise ValueError(f"JSON object required: {path}")
    return value


def load_restore_baseline(path: Path) -> dict[str, Any]:
    """Load a verified baseline file for the standby-only recovery command."""
    baseline = load_json(path)
    settings = baseline.get("settings")
    if not isinstance(settings, dict):
        raise ValueError("restore baseline must contain a settings object")
    if settings.get("tx_state") != 1:
        raise ValueError("restore baseline must describe TX standby (tx_state=1)")
    readback_snapshot = baseline.get("settings_readback")
    if baseline.get("settings_verified") is not True and not isinstance(readback_snapshot, dict):
        raise ValueError("restore baseline must retain its verified settings_readback")
    return baseline


def run_restore_only(
    args: argparse.Namespace,
    metadata: dict[str, Any],
    session_dir: Path,
    mounted: bool,
    storage_reason: str,
) -> int:
    """Run explicit standby recovery in a new output directory.

    The command never turns TX on. It requires a new session directory so a
    stopped characterization cannot be overwritten while recovery is audited.
    """
    if args.transmit or args.physical_ready:
        raise SystemExit("--restore-only cannot be combined with transmit flags")
    if args.baseline is None:
        raise SystemExit("--restore-only requires --baseline")
    if session_dir.exists():
        raise SystemExit(
            "refusing restore into existing session directory: "
            f"{session_dir}; choose a new --session-id"
        )
    baseline = load_restore_baseline(args.baseline)
    session_dir.mkdir(parents=True, exist_ok=False)
    events_path = session_dir / "events.jsonl"
    manifest_path = session_dir / "session_manifest.json"
    manifest: dict[str, Any] = {
        "schema_version": "paper1-session-manifest-1.0",
        "session_id": args.session_id,
        "experiment_id": "standby_restore_only",
        "operator": metadata.get("operator", "unknown"),
        "host": {
            "name": platform.node(),
            "os": platform.platform(),
            "clock_utc": utc_now(),
            "clock_sync": metadata.get("clock_sync", "operator-recorded"),
        },
        "radar": metadata.get("radar", {}),
        "storage": {
            "root": str(args.output_root),
            "mounted": mounted,
            "writable": mounted,
            "check": storage_reason,
        },
        "runner": {
            "restore_only": True,
            "transmit_requested": False,
            "physical_ready": False,
            "baseline_path": str(args.baseline),
        },
        "provenance": provenance(),
    }
    with events_path.open("w", encoding="utf-8") as events:
        log_event(events, "restore_session_start", baseline_path=str(args.baseline))
        try:
            report = restore_verified_baseline(
                args.base_url,
                baseline,
                events,
                args.http_timeout,
                "standby-recovery",
            )
            manifest["restore_report"] = report
            manifest["status"] = "complete" if report["restore_complete"] else "partial_failed"
        except BaseException as exc:
            manifest["status"] = "failed"
            manifest["error"] = repr(exc)
            log_event(events, "restore_session_error", error=repr(exc))
        finally:
            # Redundant safety commands are TX-off/recording-stop only.
            cleanup_errors: list[str] = []
            for endpoint, params, event_name in (
                ("/toggle_echo", {"mode": "off"}, "restore_safety_tx_off"),
                ("/toggle_recording", {"mode": "stop"}, "restore_safety_recording_stop"),
            ):
                try:
                    status, body = http_request(args.base_url, endpoint, params, args.http_timeout)
                    log_event(events, event_name, http_status=status, response=body)
                except BaseException as exc:
                    cleanup_errors.append(f"{event_name}: {exc!r}")
                    log_event(
                        events,
                        "restore_cleanup_error",
                        cleanup_event=event_name,
                        error=repr(exc),
                    )
            if cleanup_errors:
                manifest["cleanup_errors"] = cleanup_errors
            log_event(events, "restore_session_end", status=manifest.get("status"))
            write_manifest_atomic(manifest_path, manifest)
    return 0 if manifest.get("status") == "complete" else 2


def expand_control_matrix(experiment: dict[str, Any]) -> dict[str, Any]:
    """Expand a finite response matrix into interleaved reference conditions."""
    matrix = experiment.get("control_matrix")
    if not isinstance(matrix, list):
        return experiment
    phases = experiment.get("phases", [])
    if not phases:
        raise ValueError("control matrix requires a reference phase")
    reference = dict(phases[0])
    base_reference_settings = dict(reference.get("settings", {}))
    expanded: list[dict[str, Any]] = []
    for entry in matrix:
        setting = entry["setting"]
        values = entry.get("values", [])
        context_id = entry.get("context_id")
        reference_settings = {
            **base_reference_settings,
            **entry.get("reference_settings", {}),
        }
        reference_phase = dict(reference)
        reference_phase["settings"] = dict(reference_settings)
        if context_id:
            reference_phase["id"] = f"reference_{context_id}"
            reference_phase["control"] = {
                "role": "reference",
                "setting": setting,
                "reference_context": context_id,
                "reference_settings": dict(entry.get("reference_settings", {})),
            }
        expanded.append(reference_phase)
        companion = entry.get("companion_setting")
        companion_values = entry.get("companion_values", [None])
        for value in values:
            pairs = (
                zip(companion_values, [value] * len(companion_values), strict=True)
                if companion
                else [(None, value)]
            )
            for companion_value, requested in pairs:
                phase_id = f"{setting}_{requested}"
                if companion:
                    phase_id += f"_{companion}_{companion_value}"
                if context_id:
                    phase_id += f"_{context_id}"
                phase = {
                    "id": phase_id,
                    "settings": dict(reference_settings),
                    "settle_s": entry.get("settle_s", 5),
                    "record_s": entry.get("record_s", 30),
                    "scans": entry.get("scans", 6),
                    "control": {
                        "role": "condition",
                        "setting": setting,
                        "requested": requested,
                        "companion_setting": companion,
                        "companion_requested": companion_value,
                        "fixed_settings": entry.get("fixed_settings", {}),
                        "reference_context": context_id,
                        "reference_settings": dict(entry.get("reference_settings", {})),
                    },
                }
                phase["settings"].update(entry.get("fixed_settings", {}))
                phase["settings"][setting] = requested
                value_settings_map = entry.get("value_settings", {})
                value_settings = value_settings_map.get(
                    str(requested).lower(), value_settings_map.get(str(requested), {})
                )
                phase["settings"].update(value_settings)
                active_value_field = entry.get("active_value_field")
                if active_value_field:
                    phase["settings"][active_value_field] = requested
                if companion:
                    phase["settings"][companion] = companion_value
                if (
                    phase["settings"].get("gain_auto") is True
                    and phase["settings"].get("gain_value") is not None
                    and phase["settings"].get("gain_auto_value") is not None
                    and phase["settings"]["gain_value"] != phase["settings"]["gain_auto_value"]
                ):
                    raise ValueError(f"{phase_id} has inconsistent active automatic gain value")
                if (
                    phase["settings"].get("gain_auto") is False
                    and phase["settings"].get("gain_value") is not None
                    and phase["settings"].get("gain_manual_value") is not None
                    and phase["settings"]["gain_value"] != phase["settings"]["gain_manual_value"]
                ):
                    raise ValueError(f"{phase_id} has inconsistent active manual gain value")
                expanded.append(phase)
            after_reference = dict(reference_phase)
            expanded.append(after_reference)
    result = dict(experiment)
    result["phases"] = expanded
    return result


def select_control_family(experiment: dict[str, Any], family: str) -> dict[str, Any]:
    """Select one matrix setting while retaining its reference contexts."""
    matrix = experiment.get("control_matrix")
    if not isinstance(matrix, list):
        raise ValueError("--control-family requires an experiment control matrix")
    selected = [entry for entry in matrix if entry.get("setting") == family]
    if not selected:
        available = sorted({str(entry.get("setting")) for entry in matrix if entry.get("setting")})
        raise ValueError(f"unknown control family {family!r}; available={available}")
    result = dict(experiment)
    result["control_matrix"] = selected
    # E6 has one reference phase. Preserve only that shared phase before the
    # matrix expansion adds the before/after reference around each condition.
    result["phases"] = list(experiment.get("phases", []))[:1]
    if family == "doppler" and result["phases"]:
        # Doppler changes can force gain and sea-clutter mode registers. Keep
        # those fields identical for the reference and all Doppler variants so
        # a response comparison does not confound mode side effects. Rain
        # clutter is firmware-mode dependent, so paired matrix entries provide
        # their own reference_settings for rain-manual and rain-auto contexts.
        result["phases"][0] = dict(result["phases"][0])
        common_settings = {
            "gain_auto": True,
            "gain_value": 80,
            "gain_manual_value": 0,
            "gain_auto_value": 80,
            "sea_clutter_auto": True,
            "sea_clutter": 0,
            "sea_clutter_auto_value": 10,
        }
        result["phases"][0]["settings"] = {
            **result["phases"][0].get("settings", {}),
            **common_settings,
        }
        if not all(entry.get("reference_settings") for entry in selected):
            result["phases"][0]["settings"].update(
                {
                    "rain_clutter_auto": True,
                    "rain_clutter": 0,
                    "rain_clutter_auto_value": 0,
                }
            )
    result["selected_control_family"] = family
    return result


def expand_phase_repeats(experiment: dict[str, Any]) -> dict[str, Any]:
    """Expand explicit phase repeats and give every executed phase a unique ID."""
    expanded: list[dict[str, Any]] = []
    seen_ids: set[str] = set()
    for source_index, phase in enumerate(experiment.get("phases", [])):
        source_id = str(phase.get("id", f"phase_{source_index}"))
        try:
            repeat_count = int(phase.get("repeat", 1))
        except (TypeError, ValueError) as exc:
            raise ValueError(f"phase {source_id} repeat must be an integer") from exc
        if repeat_count < 1:
            raise ValueError(f"phase {source_id} repeat must be at least one")
        for repeat_index in range(repeat_count):
            clone = dict(phase)
            clone.pop("repeat", None)
            clone["source_phase_id"] = source_id
            clone["repeat_index"] = repeat_index
            clone["repeat_count"] = repeat_count
            clone["phase_index"] = len(expanded)
            if repeat_count > 1:
                clone["id"] = f"{source_id}__r{repeat_index + 1:02d}"
            elif source_id in seen_ids:
                clone["id"] = f"{source_id}__p{len(expanded) + 1:03d}"
            else:
                clone["id"] = source_id
            seen_ids.add(source_id)
            expanded.append(clone)
    result = dict(experiment)
    result["phases"] = expanded
    return result


def validate_phase_settings(experiment: dict[str, Any]) -> None:
    """Reject readback-only fields before a live phase can reach the controller."""
    for phase in experiment.get("phases", []):
        settings = phase.get("settings", {})
        if not isinstance(settings, dict):
            raise ValueError(f"phase {phase.get('id', '<unknown>')} settings must be an object")
        unsupported = sorted(set(settings) - PHASE_SETTING_FIELDS)
        if unsupported:
            raise ValueError(
                f"phase {phase.get('id', '<unknown>')} requests unsupported writable fields "
                f"{unsupported}; retain them only in complete readback verification"
            )


def filter_by_characterization_report(
    experiment: dict[str, Any], report: dict[str, Any]
) -> tuple[dict[str, Any], list[dict[str, Any]]]:
    outcomes = report.get("control_characterization", {}).get("outcomes", [])
    accepted: set[tuple[str, str, str]] = set()
    deferred: dict[tuple[str, str, str], dict[str, Any]] = {}
    for outcome in outcomes:
        outcome_name = outcome.get("outcome")
        if outcome_name not in {"supported", "deferred"}:
            continue
        key = (
            outcome.get("setting", ""),
            json.dumps(outcome.get("requested"), sort_keys=True),
            outcome.get("context", ""),
        )
        accepted.add(key)
        if outcome_name == "deferred":
            deferred[key] = outcome
    kept: list[dict[str, Any]] = []
    skipped: list[dict[str, Any]] = []
    for phase in experiment.get("phases", []):
        control = phase.get("control")
        if not control:
            if kept and kept[-1].get("id") == phase.get("id"):
                continue
            kept.append(phase)
            continue
        if control.get("role") == "reference":
            # Context-specific references are valid phases, not characterization
            # probes. They intentionally have no requested control value.
            candidate = dict(phase)
            candidate["phase_role"] = "reference"
            candidate["reference_context"] = control.get("reference_context")
            kept.append(candidate)
            continue
        key_prefix = (
            control.get("setting", ""),
            json.dumps(control.get("requested"), sort_keys=True),
        )
        fixed = control.get("fixed_settings", {})
        context_ok = any(
            key[:2] == key_prefix
            and (not fixed.get("range_index") or f"range:{fixed['range_index']}" in key[2])
            for key in accepted
        )
        if context_ok:
            candidate = dict(phase)
            matching = [
                (key, deferred[key])
                for key in deferred
                if key[:2] == key_prefix
                and (not fixed.get("range_index") or f"range:{fixed['range_index']}" in key[2])
            ]
            if matching:
                candidate["characterization_deferred_fields"] = [key[0] for key, _ in matching]
                candidate["characterization_deferred_observations"] = [
                    {
                        "setting": key[0],
                        "requested": observation.get("requested"),
                        "actual": observation.get("actual"),
                        "context": key[2],
                    }
                    for key, observation in matching
                ]
            kept.append(candidate)
        else:
            known = [
                outcome
                for outcome in outcomes
                if (
                    outcome.get("setting", ""),
                    json.dumps(outcome.get("requested"), sort_keys=True),
                )
                == key_prefix
            ]
            skipped.append(
                {
                    "phase_id": phase.get("id"),
                    "setting": control.get("setting"),
                    "requested": control.get("requested"),
                    "reason": (
                        "characterization did not confirm the requested value; "
                        "clamped values are not run as requested conditions"
                    ),
                    "characterization_outcomes": [item.get("outcome") for item in known],
                }
            )
    result = dict(experiment)
    result["phases"] = kept
    result["skipped_controls"] = skipped
    return result, skipped


def merge_characterization_reports(
    reports: list[tuple[str, dict[str, Any]]],
) -> dict[str, Any]:
    """Merge E5 reports with later reports superseding matching control keys.

    The original reports remain referenced and all outcomes are retained under
    ``outcomes_all_sources``. The filtered ``outcomes`` list contains the latest
    observation for each setting and requested value, so a corrected IR query
    cannot be blocked by an older selector-bug result.
    """
    latest: dict[tuple[str, str], tuple[int, dict[str, Any]]] = {}
    all_outcomes: list[dict[str, Any]] = []
    sources: list[dict[str, Any]] = []
    for source_index, (path, report) in enumerate(reports):
        characterization = report.get("control_characterization", {})
        outcomes = characterization.get("outcomes", [])
        if not isinstance(outcomes, list):
            outcomes = []
        sources.append(
            {
                "source_index": source_index,
                "path": path,
                "session_id": report.get("session_id"),
                "schema_version": report.get("schema_version"),
                "runner": report.get("runner"),
                "provenance": report.get("provenance"),
                "outcome_count": len(outcomes),
            }
        )
        for outcome in outcomes:
            if not isinstance(outcome, dict):
                continue
            setting = str(outcome.get("setting", ""))
            requested = json.dumps(outcome.get("requested"), sort_keys=True)
            key = (setting, requested)
            tagged = {"source_index": source_index, "source_path": path, **outcome}
            all_outcomes.append(tagged)
            latest[key] = (source_index, tagged)
    merged_outcomes = [
        outcome
        for _key, (_source_index, outcome) in sorted(
            latest.items(), key=lambda item: (item[1][0], item[0])
        )
    ]
    return {
        "schema_version": "paper1-characterization-merge-1.0",
        "merge_policy": "latest source per setting and requested value supersedes earlier outcome",
        "sources": sources,
        "control_characterization": {
            "outcomes": merged_outcomes,
            "outcomes_all_sources": all_outcomes,
        },
    }


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--plan", type=Path, required=True)
    parser.add_argument("--experiment", required=True)
    parser.add_argument(
        "--control-family",
        help="For a matrix experiment, run one setting family with its interleaved references",
    )
    parser.add_argument("--metadata", type=Path, required=True)
    parser.add_argument("--target-manifest", type=Path)
    parser.add_argument(
        "--characterization-report",
        type=Path,
        help="E5 session_manifest.json used to filter response cases to observed controls",
    )
    parser.add_argument(
        "--characterization-report-supplement",
        type=Path,
        action="append",
        default=[],
        help=(
            "Later corrected E5 report; repeated values are merged in order and "
            "supersede matching outcomes"
        ),
    )
    parser.add_argument(
        "--merged-characterization-report-out",
        type=Path,
        help="Optional JSON path for the merged primary and corrected E5 reports",
    )
    parser.add_argument("--output-root", type=Path, required=True)
    parser.add_argument("--base-url", default="http://127.0.0.1:8080")
    parser.add_argument("--session-id", required=True)
    parser.add_argument(
        "--transmit", action="store_true", help="Explicitly permit radar transmission"
    )
    parser.add_argument(
        "--physical-ready",
        action="store_true",
        help="Confirm the operator has cleared the antenna sector",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Validate and write a plan manifest without HTTP or hardware actions",
    )
    parser.add_argument(
        "--restore-only",
        action="store_true",
        help="Restore a verified standby baseline in a new session directory; never enables TX",
    )
    parser.add_argument(
        "--baseline",
        type=Path,
        help="JSON baseline captured by settings readback, required with --restore-only",
    )
    parser.add_argument("--receive-bind", default="0.0.0.0:10024")
    parser.add_argument(
        "--receive-interface",
        default=None,
        help=(
            "IPv4 address of the radar Ethernet interface for multicast membership; "
            "defaults to metadata radar.host_ip"
        ),
    )
    parser.add_argument("--multicast", default="239.255.0.2")
    parser.add_argument("--peer", default=None)
    parser.add_argument("--timeout-per-phase", type=float, default=90.0)
    parser.add_argument("--campaign-timeout", type=float, default=7200.0)
    parser.add_argument("--http-timeout", type=float, default=8.0)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    signal.signal(signal.SIGINT, lambda _sig, _frame: STOP.set())
    signal.signal(signal.SIGTERM, lambda _sig, _frame: STOP.set())
    plan = load_json(args.plan)
    metadata = load_json(args.metadata)
    experiment = next((item for item in plan["experiments"] if item["id"] == args.experiment), None)
    if experiment is None:
        raise SystemExit(f"unknown experiment: {args.experiment}")
    if args.control_family:
        experiment = select_control_family(experiment, args.control_family)
    experiment = expand_control_matrix(experiment)
    skipped_controls: list[dict[str, Any]] = []
    characterization_merge: dict[str, Any] | None = None
    if experiment.get("requires_characterization_report"):
        if args.characterization_report is None:
            raise SystemExit("This response plan requires --characterization-report from E5.")
        characterization_report = load_json(args.characterization_report)
        if args.characterization_report_supplement:
            characterization_merge = merge_characterization_reports(
                [
                    (str(args.characterization_report), characterization_report),
                    *[
                        (str(path), load_json(path))
                        for path in args.characterization_report_supplement
                    ],
                ]
            )
            if args.merged_characterization_report_out is not None:
                args.merged_characterization_report_out.write_text(
                    json.dumps(characterization_merge, indent=2) + "\n",
                    encoding="utf-8",
                )
            characterization_report = characterization_merge
        experiment, skipped_controls = filter_by_characterization_report(
            experiment, characterization_report
        )
    experiment = expand_phase_repeats(experiment)
    validate_phase_settings(experiment)
    target_manifest: dict[str, Any] | None = None
    if experiment.get("requires_operator_fields"):
        if args.target_manifest is None:
            raise SystemExit(
                "This experiment requires --target-manifest with surveyed target fields."
            )
        target_manifest = load_json(args.target_manifest)
        targets = target_manifest.get("targets", [])
        if not targets or any(
            not target.get("target_id")
            or target.get("range_m") in (None, 0)
            or target.get("bearing_deg") is None
            or not target.get("position_reference")
            or not target.get("measurement_method")
            or target.get("uncertainty_m") is None
            for target in targets
        ):
            raise SystemExit("Target manifest contains missing or placeholder ground-truth fields.")
    if (
        experiment.get("transmit")
        and not args.dry_run
        and not (args.transmit and args.physical_ready)
    ):
        raise SystemExit(
            "This experiment transmits. Provide both --transmit and --physical-ready explicitly."
        )
    mounted, reason = mounted_and_writable(args.output_root)
    if not mounted and not args.dry_run:
        raise SystemExit(f"refusing capture: {reason}")
    session_dir = args.output_root / args.session_id
    if args.restore_only:
        if args.dry_run:
            raise SystemExit("--restore-only cannot be combined with --dry-run")
        if provenance().get("git_commit") is None:
            raise SystemExit("refusing restore without Git provenance")
        return run_restore_only(args, metadata, session_dir, mounted, reason)
    session_dir.mkdir(parents=True, exist_ok=True)
    events_path = session_dir / "events.jsonl"
    manifest_path = session_dir / "session_manifest.json"
    manifest: dict[str, Any] = {
        "schema_version": "paper1-session-manifest-1.0",
        "session_id": args.session_id,
        "experiment_id": args.experiment,
        "selected_control_family": experiment.get("selected_control_family"),
        "operator": metadata.get("operator", "unknown"),
        "host": {
            "name": platform.node(),
            "os": platform.platform(),
            "clock_utc": utc_now(),
            "clock_sync": metadata.get("clock_sync", "operator-recorded"),
        },
        "radar": metadata.get("radar", {}),
        "scene": metadata.get("scene", {}),
        "storage": {
            "root": str(args.output_root),
            "mounted": mounted,
            "writable": mounted,
            "check": reason,
        },
        "phases": [],
        "skipped_controls": skipped_controls,
        "runner": {
            "plan_path": str(args.plan),
            "transmit_requested": args.transmit,
            "physical_ready": args.physical_ready,
            "dry_run": args.dry_run,
            "receive_bind": args.receive_bind,
            "receive_interface": args.receive_interface or metadata.get("radar", {}).get("host_ip"),
            "multicast": args.multicast,
            "peer": args.peer,
        },
    }
    if characterization_merge is not None:
        manifest["characterization_report_merge"] = characterization_merge
    manifest["hardware_record"] = metadata.get("hardware_record", {})
    manifest["provenance"] = provenance()
    if not args.dry_run and manifest["provenance"].get("git_commit") is None:
        raise SystemExit("refusing live acquisition without Git provenance")
    estimated_record_s = sum(float(phase.get("record_s", 0)) for phase in experiment["phases"])
    estimated_runtime_s = sum(
        float(phase.get("settle_s", 0)) + float(phase.get("record_s", 0))
        for phase in experiment["phases"]
    )
    timing_estimate = metadata.get("timing_estimate", {})
    control_write_s = timing_estimate.get("control_write_s_per_phase")
    restoration_s = timing_estimate.get("restoration_s_per_phase")
    measured_overhead_s: float | None = None
    if isinstance(control_write_s, (int, float)) and isinstance(restoration_s, (int, float)):
        measured_overhead_s = len(experiment["phases"]) * (
            float(control_write_s) + float(restoration_s)
        )
        estimated_runtime_s += measured_overhead_s
    packet_rate = metadata.get("capture_estimate", {}).get("packets_per_second")
    payload_bytes = metadata.get("capture_estimate", {}).get("payload_bytes")
    estimated_raw_bytes = (
        estimated_record_s * float(packet_rate) * float(payload_bytes)
        if packet_rate is not None and payload_bytes is not None
        else None
    )
    manifest["campaign_estimate"] = {
        "phase_count": len(experiment["phases"]),
        "record_s": estimated_record_s,
        "runtime_s": estimated_runtime_s,
        "runtime_lower_bound_s": sum(
            float(phase.get("settle_s", 0)) + float(phase.get("record_s", 0))
            for phase in experiment["phases"]
        ),
        "control_write_s_per_phase": control_write_s,
        "restoration_s_per_phase": restoration_s,
        "runtime_is_lower_bound": measured_overhead_s is None,
        "raw_udp_bytes": estimated_raw_bytes,
        "basis": (
            "settle and record windows plus operator timing_estimate"
            if measured_overhead_s is not None
            else "settle and record windows only; control and restoration time not supplied"
        ),
    }
    if not args.dry_run and estimated_runtime_s > args.campaign_timeout:
        raise SystemExit(
            f"estimated campaign runtime {estimated_runtime_s:.1f}s exceeds "
            f"campaign-timeout={args.campaign_timeout:.1f}s"
        )
    if target_manifest is not None:
        manifest["target_manifest"] = target_manifest
    with events_path.open("w", encoding="utf-8") as events:
        log_event(
            events,
            "session_start",
            experiment=args.experiment,
            dry_run=args.dry_run,
            storage_check=reason,
            skipped_control_count=len(skipped_controls),
        )
        if args.dry_run:
            manifest["phases"] = [
                {
                    "phase_id": phase["id"],
                    "source_phase_id": phase.get("source_phase_id", phase["id"]),
                    "phase_index": phase.get("phase_index"),
                    "repeat_index": phase.get("repeat_index", 0),
                    "repeat_count": phase.get("repeat_count", 1),
                    "phase_role": phase.get("phase_role")
                    or phase.get("control", {}).get("role", "condition"),
                    "control": phase.get("control", {}),
                    "reference_context": phase.get("control", {}).get("reference_context"),
                    "requested_settings": phase.get("settings", {}),
                    "device_readback": None,
                    "settle_s": phase.get("settle_s", 0),
                    "record_s": phase.get("record_s", 0),
                    "scans": phase.get("scans", 0),
                    "command_events": [],
                    "rotation_files": [],
                }
                for phase in experiment["phases"]
            ]
            manifest["status"] = "dry_run_validated"
            write_manifest_atomic(manifest_path, manifest)
            log_event(events, "session_end", status="dry_run_validated")
            return 0

        is_characterization = bool(experiment.get("characterize_controls"))
        if is_characterization and (args.transmit or args.physical_ready):
            raise SystemExit("standby characterization does not accept transmit flags")
        raw_path = session_dir / "raw_udp_payloads.jsonl"
        receiver_thread: threading.Thread | None = None
        receiver_counts: dict[str, int] = {"packets": 0, "bytes": 0, "peer_mismatch": 0}
        if not is_characterization:
            receive_interface = args.receive_interface or metadata.get("radar", {}).get("host_ip")
            receiver_thread, receiver_counts = receive_raw(
                raw_path,
                args.receive_bind,
                args.multicast,
                args.peer,
                events,
                receive_interface,
            )
        try:
            campaign_start = time.monotonic()
            before = readback(
                args.base_url,
                events,
                args.http_timeout,
                fresh_required=is_characterization,
            )
            require_confirmed_readback(before, "initial device readback")
            manifest["initial_device_readback"] = before
            manifest["status"] = "running"
            manifest["checkpoint"] = "initial_readback"
            write_manifest_atomic(manifest_path, manifest)
            if is_characterization:
                characterization_spec = experiment.get(
                    "control_characterization_override",
                    plan["control_characterization"],
                )
                characterization = characterize_controls(
                    args.base_url,
                    characterization_spec,
                    events,
                    args.http_timeout,
                )
                original_characterization_error = characterization.pop("_exception", None)
                manifest["control_characterization"] = characterization
                if original_characterization_error is not None:
                    raise original_characterization_error
                if not characterization.get("restore_complete"):
                    raise RuntimeError(
                        "control characterization did not restore the verified baseline"
                    )
                manifest["status"] = "complete"
            for phase in [] if is_characterization else experiment["phases"]:
                if time.monotonic() - campaign_start > args.campaign_timeout:
                    raise TimeoutError("campaign wall-clock deadline exceeded")
                if STOP.is_set():
                    break
                phase_record: dict[str, Any] = {
                    "phase_id": phase["id"],
                    "source_phase_id": phase.get("source_phase_id", phase["id"]),
                    "phase_index": phase.get("phase_index"),
                    "repeat_index": phase.get("repeat_index", 0),
                    "repeat_count": phase.get("repeat_count", 1),
                    "phase_role": phase.get("phase_role")
                    or phase.get("control", {}).get("role", "condition"),
                    "control": phase.get("control", {}),
                    "reference_context": phase.get("control", {}).get("reference_context"),
                    "requested_settings": phase.get("settings", {}),
                    "settle_s": phase.get("settle_s", 0),
                    "record_s": phase.get("record_s", 0),
                    "scans": phase.get("scans", 0),
                    "command_events": [],
                    "rotation_files": [],
                }
                manifest["active_phase"] = phase_record
                manifest["status"] = "running"
                manifest["checkpoint"] = "phase_started"
                write_manifest_atomic(manifest_path, manifest)
                phase_record["raw_receiver_start"] = dict(receiver_counts)
                phase_record["command_events"] = apply_phase(
                    args.base_url, phase, events, args.http_timeout
                )
                settle = float(phase.get("settle_s", 0))
                pre_tx_readback = readback(args.base_url, events, args.http_timeout)
                phase_record["pre_tx_readback"] = pre_tx_readback
                phase_record["device_readback"] = pre_tx_readback
                require_confirmed_readback(pre_tx_readback, f"phase {phase['id']} pre-TX readback")
                phase_record["readback_matches"] = requested_matches_readback(
                    phase.get("settings", {}), pre_tx_readback
                )
                mismatches = [
                    field
                    for field, matched in phase_record["readback_matches"].items()
                    if matched is not True
                    and field not in phase.get("characterization_deferred_fields", [])
                ]
                if mismatches:
                    raise RuntimeError(f"phase {phase['id']} readback mismatch for {mismatches}")
                log_event(
                    events,
                    "phase_readback",
                    phase_id=phase["id"],
                    readback=phase_record["device_readback"],
                )
                if args.transmit and phase.get("scans", 0) > 0:
                    record_s = float(phase.get("record_s", 0))
                    if record_s > args.timeout_per_phase:
                        raise RuntimeError(
                            f"phase {phase['id']} record_s={record_s} exceeds "
                            f"timeout-per-phase={args.timeout_per_phase}"
                        )
                    status, body = http_request(
                        args.base_url, "/toggle_echo", {"mode": "on"}, args.http_timeout
                    )
                    log_event(
                        events, "tx_on", phase_id=phase["id"], http_status=status, response=body
                    )
                    tx_readback = readback(
                        args.base_url, events, args.http_timeout, fresh_required=True
                    )
                    require_confirmed_readback(
                        tx_readback, f"phase {phase['id']} TX-start readback"
                    )
                    phase_record["tx_readback"] = tx_readback
                    require_transmitting_readback(
                        tx_readback, f"phase {phase['id']} TX-start readback"
                    )
                    tx_matches = requested_matches_readback(phase.get("settings", {}), tx_readback)
                    tx_mismatches = [
                        field for field, matched in tx_matches.items() if matched is not True
                    ]
                    if tx_mismatches:
                        raise RuntimeError(
                            f"phase {phase['id']} settings did not match after TX startup: "
                            f"{tx_mismatches}"
                        )
                    if settle:
                        log_event(
                            events,
                            "settle_start_after_tx",
                            phase_id=phase["id"],
                            settle_s=settle,
                        )
                        STOP.wait(settle)
                    stable_readback = readback(
                        args.base_url, events, args.http_timeout, fresh_required=True
                    )
                    require_confirmed_readback(
                        stable_readback, f"phase {phase['id']} post-settle readback"
                    )
                    require_transmitting_readback(
                        stable_readback, f"phase {phase['id']} post-settle readback"
                    )
                    phase_record["post_settle_readback"] = stable_readback
                    phase_record["device_readback"] = stable_readback
                    stable_matches = requested_matches_readback(
                        phase.get("settings", {}), stable_readback
                    )
                    stable_mismatches = [
                        field for field, matched in stable_matches.items() if matched is not True
                    ]
                    if stable_mismatches:
                        raise RuntimeError(
                            f"phase {phase['id']} settings did not remain matched after settling: "
                            f"{stable_mismatches}"
                        )
                    status, body = http_request(
                        args.base_url, "/toggle_recording", {"mode": "start"}, args.http_timeout
                    )
                    require_success(status, body, f"phase {phase['id']} recording start")
                    log_event(
                        events,
                        "recording_start",
                        phase_id=phase["id"],
                        http_status=status,
                        response=body,
                    )
                    status, body = http_request(
                        args.base_url, "/get_system_info", timeout=args.http_timeout
                    )
                    recording_info = parse_system_info(
                        status, body, f"phase {phase['id']} recording status"
                    )
                    if recording_info.get("recording") is not True:
                        raise RuntimeError(
                            f"phase {phase['id']} recording did not become active: {recording_info}"
                        )
                    if recording_info.get("connected") is False:
                        raise RuntimeError(
                            f"phase {phase['id']} controller disconnected at recording start"
                        )
                    require_system_info_settings(
                        recording_info,
                        phase.get("settings", {}),
                        f"phase {phase['id']} recording start",
                    )
                    phase_record["recording_start_info"] = recording_info
                    start = time.monotonic()
                    log_event(
                        events, "recording_window_start", phase_id=phase["id"], record_s=record_s
                    )
                    start_rotations: int | None = None
                    observed_rotations = 0
                    while time.monotonic() - start < args.timeout_per_phase and not STOP.is_set():
                        status, body = http_request(
                            args.base_url, "/get_system_info", timeout=args.http_timeout
                        )
                        info = parse_system_info(
                            status, body, f"phase {phase['id']} recording status poll"
                        )
                        if info.get("recording") is False:
                            raise RuntimeError(
                                f"phase {phase['id']} recording stopped unexpectedly"
                            )
                        if info.get("connected") is False:
                            raise RuntimeError(f"phase {phase['id']} controller disconnected")
                        require_system_info_settings(
                            info,
                            phase.get("settings", {}),
                            f"phase {phase['id']} recording status poll",
                        )
                        start_rotations = (
                            info.get("rotation_count")
                            if start_rotations is None
                            else start_rotations
                        )
                        elapsed = time.monotonic() - start
                        if isinstance(start_rotations, int) and isinstance(
                            info.get("rotation_count"), int
                        ):
                            observed_rotations = info["rotation_count"] - start_rotations
                        if (
                            elapsed >= record_s
                            and isinstance(start_rotations, int)
                            and info.get("rotation_count", 0) - start_rotations
                            >= int(phase["scans"])
                        ):
                            break
                        STOP.wait(0.5)
                    if observed_rotations < int(phase["scans"]):
                        raise TimeoutError(
                            f"phase {phase['id']} reached {observed_rotations} rotations; "
                            f"required {phase['scans']}"
                        )
                    log_event(
                        events,
                        "recording_window_end",
                        phase_id=phase["id"],
                        record_s=time.monotonic() - start,
                        rotations=observed_rotations,
                    )
                    status, body = http_request(
                        args.base_url, "/toggle_echo", {"mode": "off"}, args.http_timeout
                    )
                    log_event(
                        events, "tx_off", phase_id=phase["id"], http_status=status, response=body
                    )
                    status, body = http_request(
                        args.base_url, "/toggle_recording", {"mode": "stop"}, args.http_timeout
                    )
                    require_success(status, body, f"phase {phase['id']} recording stop")
                    log_event(
                        events,
                        "recording_stop",
                        phase_id=phase["id"],
                        http_status=status,
                        response=body,
                    )
                    record_match = re.search(r"Files in (.+?)/?$", body.strip())
                    if record_match:
                        record_dir = Path(record_match.group(1).rstrip("/"))
                        phase_record["record_dir"] = str(record_dir)
                        phase_record["rotation_files"] = [
                            str(path) for path in sorted(record_dir.glob("csv/*.csv"))
                        ]
                        phase_record["recording_output"] = inspect_recording_output(
                            record_dir, int(phase["scans"])
                        )
                    else:
                        raise RuntimeError(
                            f"phase {phase['id']} recording stop did not identify "
                            "a recording directory"
                        )
                if experiment.get("reset_to_baseline"):
                    phase_record["baseline_reset_readback"] = restore_settings_baseline(
                        args.base_url,
                        before,
                        events,
                        args.http_timeout,
                        f"after:{phase['id']}",
                    )
                phase_record["raw_receiver_end"] = dict(receiver_counts)
                phase_record["raw_packet_count"] = receiver_counts.get("packets", 0) - phase_record[
                    "raw_receiver_start"
                ].get("packets", 0)
                if int(phase_record["raw_packet_count"]) <= 0:
                    raise RuntimeError(
                        f"phase {phase['id']} received no raw UDP packets during its capture window"
                    )
                manifest["phases"].append(phase_record)
                manifest.pop("active_phase", None)
                manifest["status"] = "running"
                manifest["checkpoint"] = "phase_completed"
                manifest["completed_phase_count"] = len(manifest["phases"])
                manifest["last_completed_phase_index"] = phase_record.get("phase_index")
                manifest["last_completed_phase_id"] = phase_record.get("phase_id")
                write_manifest_atomic(manifest_path, manifest)
            manifest["raw_receiver"] = receiver_counts
            if not is_characterization:
                manifest["status"] = "complete" if not STOP.is_set() else "interrupted"
        except BaseException as exc:
            manifest["status"] = "failed"
            manifest["error"] = repr(exc)
            log_event(events, "session_error", error=repr(exc))
            raise
        finally:
            # Safety cleanup is attempted even when a request, write, or signal fails.
            cleanup_errors: list[str] = []
            for endpoint, params, event in (
                ("/toggle_echo", {"mode": "off"}, "safety_tx_off"),
                ("/toggle_recording", {"mode": "stop"}, "safety_recording_stop"),
            ):
                try:
                    status, body = http_request(args.base_url, endpoint, params, args.http_timeout)
                    log_event(events, event, http_status=status, response=body)
                except BaseException as exc:
                    cleanup_errors.append(f"{event}: {exc!r}")
                    log_event(events, "cleanup_error", cleanup_event=event, error=repr(exc))
            if cleanup_errors:
                manifest["cleanup_errors"] = cleanup_errors
            if not is_characterization and manifest.get("initial_device_readback"):
                try:
                    manifest["final_baseline_restore"] = restore_settings_baseline(
                        args.base_url,
                        manifest["initial_device_readback"],
                        events,
                        args.http_timeout,
                        "campaign-final",
                    )
                except BaseException as exc:
                    manifest["final_baseline_restore_error"] = repr(exc)
                    log_event(
                        events,
                        "cleanup_error",
                        cleanup_event="campaign-final-baseline",
                        error=repr(exc),
                    )
            STOP.set()
            if receiver_thread is not None:
                receiver_thread.join(timeout=2)
            # Persist the manifest before leaving the events context, including
            # characterization outcomes and restoration failures when the main
            # body re-raises an exception.
            if manifest.get("status") is None:
                manifest["status"] = "failed"
            log_event(
                events, "session_end", status=manifest.get("status"), raw_receiver=receiver_counts
            )
            write_manifest_atomic(manifest_path, manifest)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

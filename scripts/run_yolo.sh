#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MAIN_DIR="$ROOT_DIR/main"

cd "$MAIN_DIR"
exec python3 "$MAIN_DIR/ml/radar_yolo_daemon.py" "$@"

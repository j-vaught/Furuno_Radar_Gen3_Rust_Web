#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

if [[ ! -d .venv ]]; then
  python3 -m venv .venv
fi

source .venv/bin/activate
python -m pip install --upgrade pip
python -m pip install -r requirements.txt

# Default: single-thread CPU, 640x640
python benchmark_yolo_onnx.py --mode cpu --threads 1 --size 640 --warmup 5 --runs 30 "$@"

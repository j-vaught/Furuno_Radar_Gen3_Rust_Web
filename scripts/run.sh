#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MAIN_DIR="$ROOT_DIR/main"

"$ROOT_DIR/scripts/build.sh"

IP="${1:-172.31.1.100}"
HOST="${2:-PC1}"

export LD_LIBRARY_PATH="$ROOT_DIR/furuno/lib"
cd "$MAIN_DIR"
exec "$MAIN_DIR/bin/navnet_main" \
  "$MAIN_DIR/runtime/license/vendor.txt" \
  "$MAIN_DIR/runtime/license/target.dat" \
  "$IP" \
  "$HOST"

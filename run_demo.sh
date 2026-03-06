#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

for cmd in g++; do
  if ! command -v "$cmd" >/dev/null 2>&1; then
    echo "Missing required command: $cmd"
    exit 1
  fi
done

if [ ! -f "$ROOT_DIR/main/runtime/license/vendor.txt" ] || [ ! -f "$ROOT_DIR/main/runtime/license/target.dat" ]; then
  echo "Missing license files in main/runtime/license"
  exit 1
fi

if [ ! -f "$ROOT_DIR/furuno/lib/libNAVNETDLL.so" ]; then
  echo "Missing NAVNET DLL in furuno/lib/"
  exit 1
fi

echo "Building and starting NAVNET sample..."
echo "Web UI: http://localhost:8080"
echo "Use Ctrl+C to stop."

exec "$ROOT_DIR/scripts/run.sh" "$@"

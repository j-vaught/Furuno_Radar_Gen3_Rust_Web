#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [ -x "$ROOT_DIR/scripts/check_prereqs.sh" ]; then
  "$ROOT_DIR/scripts/check_prereqs.sh"
fi

echo "Building and starting NAVNET sample..."
echo "Web UI: http://localhost:8080"
echo "Use Ctrl+C to stop."

exec "$ROOT_DIR/scripts/run.sh" "$@"

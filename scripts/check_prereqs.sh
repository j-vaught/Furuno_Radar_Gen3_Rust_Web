#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

missing=0

have_cmd() {
  command -v "$1" >/dev/null 2>&1
}

need_cmd() {
  local c="$1"
  if ! have_cmd "$c"; then
    echo "[MISSING] command: $c"
    missing=1
  else
    echo "[OK] command: $c"
  fi
}

need_file() {
  local f="$1"
  if [ ! -f "$f" ]; then
    echo "[MISSING] file: $f"
    missing=1
  else
    echo "[OK] file: $f"
  fi
}

echo "Checking required commands..."
need_cmd g++

echo "Checking required repository files..."
need_file "$ROOT_DIR/main/runtime/license/vendor.txt"
need_file "$ROOT_DIR/main/runtime/license/target.dat"
need_file "$ROOT_DIR/furuno/lib/libNAVNETDLL.so"
need_file "$ROOT_DIR/furuno/lib/libNAVNETDLL.so.01.95.0000"

echo "Checking Boost compile/link availability..."
if ! cat <<'CPP' | g++ -x c++ - -o /tmp/navnet_boost_check -lboost_system -lboost_thread >/tmp/navnet_boost_check.log 2>&1
#include <boost/asio.hpp>
#include <boost/thread.hpp>
int main() { return 0; }
CPP
then
  echo "[MISSING] Boost development libs/headers (need libboost-all-dev)"
  tail -n 10 /tmp/navnet_boost_check.log || true
  missing=1
else
  echo "[OK] Boost headers and libs"
fi
rm -f /tmp/navnet_boost_check /tmp/navnet_boost_check.log

echo "Checking optional YOLO dependencies..."
if have_cmd python3; then
  echo "[OK] command: python3"
  python3 - <<'PY'
mods = ["numpy", "PIL", "onnxruntime"]
for m in mods:
    try:
        __import__(m)
        print(f"[OK] python module: {m}")
    except Exception:
        print(f"[OPTIONAL-MISSING] python module: {m}")
PY
else
  echo "[OPTIONAL-MISSING] command: python3"
fi

if [ "$missing" -ne 0 ]; then
  echo
  echo "Prerequisite check failed."
  echo "Install required packages (Debian/Ubuntu):"
  echo "  sudo apt-get update"
  echo "  sudo apt-get install -y g++ libboost-all-dev"
  exit 1
fi

echo
echo "All required prerequisites are present."

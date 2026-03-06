#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MAIN_DIR="$ROOT_DIR/main"
BUILD_DIR="$MAIN_DIR/build"
BIN_DIR="$MAIN_DIR/bin"

mkdir -p "$BUILD_DIR" "$BIN_DIR"

COMMON_FLAGS=( -O2 -Wall -I"$ROOT_DIR/furuno/include" -I"$MAIN_DIR/src" )

# Compile
/usr/bin/g++ "${COMMON_FLAGS[@]}" -c "$MAIN_DIR/src/stb_image_write.cpp" -o "$BUILD_DIR/stb_image_write.o"
/usr/bin/g++ "${COMMON_FLAGS[@]}" -DFLM_COMPILE_NEW_LICENSE=1 -DUSE_BOOST_WEB_INTERFACE=1 -c "$MAIN_DIR/src/main.cpp" -o "$BUILD_DIR/main.o"
/usr/bin/g++ "${COMMON_FLAGS[@]}" -DUSE_BOOST_WEB_INTERFACE=1 -c "$MAIN_DIR/src/web_server.cpp" -o "$BUILD_DIR/web_server.o"
/usr/bin/g++ "${COMMON_FLAGS[@]}" -c "$MAIN_DIR/src/pattern_control.cpp" -o "$BUILD_DIR/pattern_control.o"

# Link
/usr/bin/g++ -o "$BIN_DIR/navnet_main" \
  "$BUILD_DIR/main.o" "$BUILD_DIR/web_server.o" "$BUILD_DIR/stb_image_write.o" "$BUILD_DIR/pattern_control.o" \
  -Wall -L"$ROOT_DIR/furuno/lib" -lNAVNETDLL -lboost_system -lboost_thread -lpthread -lm -ldl

echo "Build complete: $BIN_DIR/navnet_main"

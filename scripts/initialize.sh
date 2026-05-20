#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
PROJECT_DIR=$(cd "$SCRIPT_DIR/.." && pwd)
SIM_DIR="$PROJECT_DIR/simulate-ofdm"
BUILD_DIR="$SIM_DIR/build"

echo "Configuring and building simulator..."
cmake -S "$SIM_DIR" -B "$BUILD_DIR"
cmake --build "$BUILD_DIR" --target simulate-ofdm

echo "Build complete: $BUILD_DIR/bin/simulate-ofdm"

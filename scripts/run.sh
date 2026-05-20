#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
PROJECT_DIR=$(cd "$SCRIPT_DIR/.." && pwd)
SIM_DIR="$PROJECT_DIR/simulate-ofdm"
PLOT_DIR="$PROJECT_DIR/plot-ofdm"
BUILD_DIR="$SIM_DIR/build"
BINARY="$BUILD_DIR/bin/simulate-ofdm"
SIMULATION_RESULT_DIR="$PROJECT_DIR/results/simulation"
PLOT_RESULT_DIR="$PROJECT_DIR/results/plot"

if [[ ! -x "$BINARY" ]]; then
	echo "Simulator binary not found: $BINARY" >&2
	echo "Run ./scripts/initialize.sh first." >&2
	exit 1
fi

mkdir -p "$SIMULATION_RESULT_DIR"

echo "Running simulator..."
"$BINARY" \
	--n=64 \
	--cp=16 \
	--ch-len=5 \
	--channel-taps=1,0,0.5,0,0.3 \
	--eq-eps=1e-12 \
	--snr-db=10 \
	--seed=42 \
	--output-dir="$SIMULATION_RESULT_DIR"

echo "Generating plot..."
if command -v uv >/dev/null 2>&1; then
	(cd "$PLOT_DIR" && uv run python main.py --input-dir "$SIMULATION_RESULT_DIR" --output-dir "$PLOT_RESULT_DIR" --output "$PLOT_RESULT_DIR/ofdm_blocks.png")
else
	(cd "$PLOT_DIR" && python3 main.py --input-dir "$SIMULATION_RESULT_DIR" --output-dir "$PLOT_RESULT_DIR" --output "$PLOT_RESULT_DIR/ofdm_blocks.png")
fi

echo "Done: $PLOT_RESULT_DIR/ofdm_blocks.png"

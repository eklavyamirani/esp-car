#!/usr/bin/env bash
# Build, flash, and verify the motor demo on the ESP32.
# Usage: ./test.sh [serial_port]
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PORT="${1:-/dev/ttyUSB0}"

# shellcheck disable=SC1091  # export.sh only exists after installing ESP-IDF
source /opt/esp-idf/export.sh 2>/dev/null

cd "$SCRIPT_DIR"

echo "=== Building ==="
idf.py build

echo ""
echo "=== Flashing to $PORT ==="
idf.py flash -p "$PORT"

echo ""
echo "=== Verifying serial output ==="
python3 "$SCRIPT_DIR/tools/serial_check.py" "$PORT"

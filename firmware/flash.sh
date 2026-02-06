#!/usr/bin/env bash
# Build, flash, and open serial monitor.
# Usage: ./flash.sh [serial_port]
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PORT="${1:-/dev/ttyUSB0}"

source /opt/esp-idf/export.sh 2>/dev/null

cd "$SCRIPT_DIR"

echo "=== Building ==="
idf.py build

echo ""
echo "=== Flashing to $PORT ==="
idf.py flash -p "$PORT"

echo ""
echo "=== Starting monitor (Ctrl+] to exit) ==="
idf.py monitor -p "$PORT"

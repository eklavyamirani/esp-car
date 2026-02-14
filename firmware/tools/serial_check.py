#!/usr/bin/env python3
"""Read serial output from ESP32 and verify the expected servo demo log sequence."""

import sys
import time
import serial

PORT = "/dev/ttyUSB0"
BAUD = 115200
TIMEOUT_S = 20

EXPECTED_SEQUENCE = [
    "SERVO: PAN=0 TILT=0",
    "SERVO: PAN=90 TILT=90",
    "SERVO: PAN=180 TILT=180",
    "SERVO: PAN=90 TILT=90",
]

def main():
    port = sys.argv[1] if len(sys.argv) > 1 else PORT

    print(f"Opening {port} at {BAUD} baud...")
    ser = serial.Serial(port, BAUD, timeout=1)

    # Wait for the board to boot after flash (don't toggle DTR/RTS)
    time.sleep(1)
    ser.reset_input_buffer()

    # Trigger a reset by toggling RTS (boot into app, not download mode)
    ser.dtr = False
    ser.rts = True
    time.sleep(0.1)
    ser.rts = False
    time.sleep(0.5)
    ser.reset_input_buffer()

    print(f"Waiting up to {TIMEOUT_S}s for log sequence...")
    captured = []
    seq_idx = 0
    deadline = time.time() + TIMEOUT_S

    while time.time() < deadline and seq_idx < len(EXPECTED_SEQUENCE):
        raw = ser.readline()
        if not raw:
            continue
        try:
            line = raw.decode("utf-8", errors="replace").strip()
        except Exception:
            continue
        if line:
            captured.append(line)
            print(f"  > {line}")
            # Check if this line contains the next expected token
            expected = EXPECTED_SEQUENCE[seq_idx]
            if f"MAIN: {expected}" in line:
                seq_idx += 1

    ser.close()

    print()
    if seq_idx >= len(EXPECTED_SEQUENCE):
        print("✅ PASS — Full servo demo sequence detected")
        return 0
    else:
        matched = EXPECTED_SEQUENCE[:seq_idx]
        missing = EXPECTED_SEQUENCE[seq_idx:]
        print(f"❌ FAIL — Matched {seq_idx}/{len(EXPECTED_SEQUENCE)} steps")
        print(f"   Matched: {matched}")
        print(f"   Missing: {missing}")
        print()
        print("--- Captured output ---")
        for l in captured:
            print(l)
        return 1

if __name__ == "__main__":
    sys.exit(main())

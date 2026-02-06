# ESP32 Robot Car (ESP-IDF)

ESP-IDF firmware for the [Freenove 4WD Car Kit for ESP32](http://www.freenove.com). Replaces the manufacturer's Arduino-based code with a modular, FreeRTOS-based implementation built on ESP-IDF v5.4+.

## Hardware

| Component | Detail |
|-----------|--------|
| MCU | ESP32-WROVER-B |
| Motor driver | PCA9685 (16-ch I2C PWM), address `0x5F` |
| I2C bus | SDA = GPIO 13, SCL = GPIO 14, 100 kHz |
| Motors | 4× DC motors via H-bridge, PCA9685 channels 8–15 |
| Servos | Pan (ch 0), Tilt (ch 1) — *not yet implemented* |
| Camera | OV2640 — *not yet implemented* |
| Sensors | Ultrasonic, photosensitive, IR, line tracking — *not yet implemented* |

### Motor Channel Mapping

| Motor | Forward (IN1) | Reverse (IN2) |
|-------|---------------|---------------|
| M1 (front-left) | ch 15 | ch 14 |
| M2 (front-right) | ch 9 | ch 8 |
| M3 (rear-left) | ch 12 | ch 13 |
| M4 (rear-right) | ch 10 | ch 11 |

Speed range: `−4095` to `+4095` (sign = direction, magnitude = PWM duty).

## Project Structure

```
firmware/
├── CMakeLists.txt                # ESP-IDF project file
├── sdkconfig.defaults            # ESP32, 240 MHz, 4 MB flash
├── main/
│   ├── CMakeLists.txt
│   └── main.c                    # app_main — launches motor demo task
├── components/
│   ├── pca9685/                  # PCA9685 I2C PWM driver
│   │   ├── include/pca9685.h
│   │   └── pca9685.c
│   └── motor/                    # 4-motor abstraction layer
│       ├── include/motor.h
│       └── motor.c
├── test.sh                       # Automated: build → flash → verify
├── flash.sh                      # Manual: build → flash → monitor
└── tools/
    └── serial_check.py           # Serial log capture + assertion
```

## Prerequisites

- **ESP-IDF v5.4+** — install from https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/
- **Python 3** with `pyserial` (`pip install pyserial`)
- ESP32 connected via USB (CH340 serial chip, typically `/dev/ttyUSB0`)
- Battery pack connected and turned on (powers the PCA9685 and motors)

## Quick Start

```bash
# Source ESP-IDF environment
source /path/to/esp-idf/export.sh

# Build
cd firmware
idf.py build

# Flash and open serial monitor
./flash.sh

# Or run the automated test pipeline
./test.sh
```

## Scripts

### `test.sh [port]`

Fully automated feedback loop:

1. Builds the firmware (`idf.py build`)
2. Flashes to the ESP32 (`idf.py flash`)
3. Captures serial output for 20 seconds
4. Asserts the expected motor demo log sequence appears in order

Exits `0` on pass, `1` on fail (with full captured output for debugging).

### `flash.sh [port]`

Builds, flashes, and opens the ESP-IDF serial monitor (`Ctrl+]` to exit).

Default port: `/dev/ttyUSB0`.

## Components

### `pca9685`

Low-level I2C driver for the PCA9685 16-channel PWM controller.

**API:**

```c
esp_err_t pca9685_init(const pca9685_config_t *cfg, pca9685_handle_t *handle);
esp_err_t pca9685_set_frequency(pca9685_handle_t *handle, uint16_t freq_hz);
esp_err_t pca9685_set_channel_pwm(pca9685_handle_t *handle, uint8_t channel, uint16_t pwm);
esp_err_t pca9685_set_channel_off(pca9685_handle_t *handle, uint8_t channel);
```

Uses the ESP-IDF legacy I2C driver (`driver/i2c.h`) for compatibility with the Freenove board's GPIO 13/14 pin assignment (these pins are reserved by the SPI flash subsystem at boot; the legacy driver handles this transparently).

### `motor`

High-level 4-motor abstraction built on the `pca9685` component.

**API:**

```c
esp_err_t motor_init(void);                              // Init I2C + PCA9685 + stop all motors
esp_err_t motor_move(int m1, int m2, int m3, int m4);   // Set speed per motor (−4095 to +4095)
esp_err_t motor_stop(void);                              // Stop all motors
```

Direction multipliers (`MOTOR_x_DIR`) can be changed to `-1` in `motor.c` if a motor is wired in reverse.

## Current Behavior (Milestone 1)

On boot, `app_main` initializes the motor subsystem and launches a FreeRTOS task that loops:

```
FORWARD (1s) → STOP (1s) → BACKWARD (1s) → STOP (1s) →
TURN_LEFT (1s) → STOP (1s) → TURN_RIGHT (1s) → STOP (1s) → repeat
```

Each state transition is logged via `ESP_LOGI` with tag `MOTOR` for automated verification.

## Roadmap

| Milestone | Feature | Status |
|-----------|---------|--------|
| 1 | Motor control (4WD demo loop) | ✅ Complete |
| 2 | Servo control (pan/tilt camera mount) | Planned |
| 3 | Ultrasonic sensor + obstacle avoidance | Planned |
| 4 | Wi-Fi + TCP command server | Planned |
| 5 | Camera streaming (OV2640) | Planned |
| 6 | Line tracking / IR remote | Planned |

## Reference

The `reference-code/` directory contains the manufacturer's original Arduino-based implementation (Freenove 4WD Car Kit for ESP32). It is not part of the build and is kept for reference only.

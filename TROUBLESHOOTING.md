# Troubleshooting Guide

Hardware and software issues encountered while developing the ESP-IDF firmware for the Freenove 4WD Car Kit, and how to resolve them.

## I2C Communication Failures

### "GPIO X is not usable, maybe conflict with others"

**Symptom:** Warning on boot:
```
W (272) i2c.common: GPIO 13 is not usable, maybe conflict with others
W (277) i2c.common: GPIO 14 is not usable, maybe conflict with others
```

**Cause:** ESP-IDF's SPI flash subsystem reserves GPIO 12–14 at boot because they are the HSPI (SPI2) IOMUX pins on the ESP32. The I2C driver detects this reservation and logs a warning. On the Freenove board, GPIO 13 (SDA) and GPIO 14 (SCL) are wired to the PCA9685, so this warning is expected.

**Resolution:** This warning is **non-fatal** — the I2C driver proceeds and configures the pins correctly. No action needed. The warning can be suppressed by setting the log level for `i2c.common` to `ERROR`, but it's harmless to leave it.

### "I2C software timeout" / "I2C hardware timeout detected"

**Symptom:** I2C transactions fail with `ESP_ERR_TIMEOUT`:
```
E (383) i2c.master: I2C software timeout
E (383) PCA9685: Failed to write MODE1 (sleep): ESP_ERR_INVALID_STATE
```
Or:
```
E (289) i2c.master: I2C hardware timeout detected
E (293) i2c.master: probe device timeout. Please check if xfer_timeout_ms and pull-ups are correctly set up
```

**Cause:** The I2C bus is physically unable to communicate. Every address on the bus times out, not just the PCA9685.

**Things to check:**
1. **Battery pack** — The PCA9685 is powered by the battery, not USB. If the battery is disconnected, dead, or switched off, the PCA9685 won't respond on I2C.
2. **Shield board seating** — The PCA9685 sits on the motor driver shield. If the shield isn't firmly plugged into the ESP32 board, the I2C lines won't make contact.
3. **Loose wires** — Vibration from the motors can loosen connections over time.

**Diagnostic approach:** Flash the reference Arduino sketch (`Sketches/01.1_Car_Move_and_Turn`) through the Arduino IDE to verify the hardware is functional. If it also fails, the problem is hardware, not software.

### New I2C driver vs legacy I2C driver

**Symptom:** The new ESP-IDF `i2c_master` API (`driver/i2c_master.h`) fails with hardware timeouts on GPIO 13/14, but the legacy API (`driver/i2c.h`) works.

**Cause:** The new I2C master driver introduced in ESP-IDF v5.x has stricter GPIO reservation checks. On the ESP32-WROVER, GPIO 13/14 are pre-reserved by the SPI flash subsystem. Even after calling `esp_gpio_revoke()` and `gpio_reset_pin()`, the new driver's internal pin setup fails to properly reconfigure these pins for I2C.

The legacy driver (`i2c_param_config` + `i2c_driver_install`) handles this case transparently because it configures the GPIO matrix directly without going through the reservation system.

**Resolution:** Use the legacy I2C driver. It produces a deprecation warning (`This driver is an old driver, please migrate your application code to adapt driver/i2c_master.h`) which is safe to ignore. If a future ESP-IDF version removes the legacy driver, revisit by calling `esp_gpio_revoke()` before `i2c_new_master_bus()`.

## PCA9685 Issues

### Motors don't spin but I2C communication succeeds

**Symptom:** Serial logs show the full demo sequence (`FORWARD → STOP → BACKWARD → ...`) with no I2C errors, but the motors don't physically move.

**Cause:** The PCA9685 is in **sleep mode**. When the chip is asleep, it accepts I2C register writes (so no errors are reported) but its PWM outputs are disabled — no signal reaches the motor driver.

This happened due to a bug in the `pca9685_set_frequency()` function. The sequence was:

```c
uint8_t mode1;
pca9685_read_reg(handle, PCA9685_REG_MODE1, &mode1);  // mode1 = 0x30 (SLEEP | AI)
// ... put to sleep, write prescale, wake up ...
pca9685_write_reg(handle, PCA9685_REG_MODE1, mode1 | MODE1_RESTART);
// BUG: mode1 still has MODE1_SLEEP set from the read!
// This writes 0xB0 (RESTART | AI | SLEEP) — putting it back to sleep!
```

**Fix:** Clear the SLEEP bit when writing the restart:
```c
pca9685_write_reg(handle, PCA9685_REG_MODE1, (mode1 & ~MODE1_SLEEP) | MODE1_RESTART);
```

**How to detect:** If the logs show a clean sequence with no errors but motors don't move, read back MODE1 and check bit 4 (SLEEP). If set, the chip is asleep.

### PCA9685 doesn't respond after a cold boot but works after reflashing

**Symptom:** The PCA9685 responds on I2C after flashing the Arduino reference sketch, but fails on the next cold boot with the ESP-IDF firmware.

**Cause:** The Arduino PCA9685 library (`setupSingleDevice`) sends a **General Call Reset** (I2C address `0x00`, data `0x06`) before initializing the device. This software-resets all PCA9685 chips on the bus. The ESP-IDF firmware doesn't do this by default.

If the PCA9685 was in an unknown state from a previous crash or power glitch, it may need this reset to become responsive again.

**Resolution:** If you encounter this, power-cycle the car (turn battery off, wait a few seconds, turn back on). For a more robust fix, add a General Call Reset to the init sequence:

```c
// Send SWRST via General Call (address 0x00)
i2c_cmd_handle_t cmd = i2c_cmd_link_create();
i2c_master_start(cmd);
i2c_master_write_byte(cmd, 0x00, true);  // General Call address
i2c_master_write_byte(cmd, 0x06, true);  // SWRST command
i2c_master_stop(cmd);
i2c_master_cmd_begin(port, cmd, pdMS_TO_TICKS(100));
i2c_cmd_link_delete(cmd);
vTaskDelay(pdMS_TO_TICKS(10));  // Wait for reset
```

## Build Issues

### ESP-IDF environment not found

**Symptom:** `idf.py: command not found`

**Resolution:** Source the ESP-IDF export script before building:
```bash
source /opt/esp-idf/export.sh  # or wherever ESP-IDF is installed
```

### `pdMS_TO_TICKS` implicit declaration

**Symptom:**
```
error: implicit declaration of function 'pdMS_TO_TICKS'
```

**Cause:** Missing FreeRTOS headers.

**Resolution:** Add to your source file:
```c
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
```

## Test Script Issues

### Board enters download mode instead of booting after flash

**Symptom:** `test.sh` flashes successfully but serial output shows:
```
rst:0x10 (RTCWDT_RTC_RESET),boot:0x3 (DOWNLOAD_BOOT)
waiting for download
```

**Cause:** The serial capture script toggles DTR/RTS too aggressively right after `idf.py flash` does a hard reset. The DTR/RTS sequence puts GPIO 0 low during reset, which enters download mode.

**Resolution:** Wait for the board to boot on its own after flash, then reset without holding GPIO 0 low. The `serial_check.py` script handles this by waiting 1 second after connecting, then toggling only RTS (not DTR) for the reset.

### Serial port busy

**Symptom:** `[Errno 16] Device or resource busy: '/dev/ttyUSB0'`

**Cause:** Another process (serial monitor, previous test run) is holding the serial port open.

**Resolution:**
```bash
fuser -k /dev/ttyUSB0  # Kill process holding the port
sleep 1                 # Wait for release
```

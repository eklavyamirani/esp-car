#pragma once

#include "esp_err.h"
#include "pca9685.h"

typedef struct {
    pca9685_handle_t *pca;
    uint8_t channel;
    uint16_t min_pulse_us;  // pulse width at 0°  (default 500)
    uint16_t max_pulse_us;  // pulse width at 180° (default 2500)
} servo_config_t;

typedef struct {
    pca9685_handle_t *pca;
    uint8_t channel;
    uint16_t min_pulse_us;
    uint16_t max_pulse_us;
} servo_handle_t;

/**
 * @brief Initialize a servo on a PCA9685 channel.
 */
esp_err_t servo_init(const servo_config_t *cfg, servo_handle_t *handle);

/**
 * @brief Set servo position by angle (0–180 degrees).
 */
esp_err_t servo_set_angle(servo_handle_t *handle, float angle);

/**
 * @brief Set servo position by raw pulse width in microseconds.
 */
esp_err_t servo_set_pulse_us(servo_handle_t *handle, uint16_t pulse_us);

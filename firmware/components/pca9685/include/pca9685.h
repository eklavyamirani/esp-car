#pragma once

#include "esp_err.h"
#include "driver/i2c.h"

typedef struct {
    i2c_port_t i2c_port;
    int sda_gpio;
    int scl_gpio;
    uint8_t address;
    uint32_t i2c_freq_hz;
} pca9685_config_t;

typedef struct {
    i2c_port_t port;
    uint8_t address;
} pca9685_handle_t;

/**
 * @brief Initialize the PCA9685 and the underlying I2C bus.
 */
esp_err_t pca9685_init(const pca9685_config_t *cfg, pca9685_handle_t *handle);

/**
 * @brief Set the PWM frequency (applies to all channels).
 */
esp_err_t pca9685_set_frequency(pca9685_handle_t *handle, uint16_t freq_hz);

/**
 * @brief Set a channel's pulse width as a 12-bit value (0–4095).
 */
esp_err_t pca9685_set_channel_pwm(pca9685_handle_t *handle, uint8_t channel, uint16_t pwm);

/**
 * @brief Turn off a channel (LED full-off).
 */
esp_err_t pca9685_set_channel_off(pca9685_handle_t *handle, uint8_t channel);

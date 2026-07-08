#include "pca9685.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>

static const char *TAG = "PCA9685";

// PCA9685 register addresses
#define PCA9685_REG_MODE1 0x00
#define PCA9685_REG_MODE2 0x01
#define PCA9685_REG_LED0_ON_L 0x06
#define PCA9685_REG_PRE_SCALE 0xFE

// MODE1 bits
#define MODE1_RESTART (1 << 7)
#define MODE1_SLEEP (1 << 4)
#define MODE1_AI (1 << 5) // auto-increment

// Each LED channel has 4 registers: ON_L, ON_H, OFF_L, OFF_H
#define LED_REG(channel) (PCA9685_REG_LED0_ON_L + 4 * (channel))

// Internal oscillator frequency
#define PCA9685_OSC_FREQ 25000000.0f

// I2C timeout
#define I2C_TIMEOUT_TICKS pdMS_TO_TICKS(100)

static esp_err_t pca9685_write_reg(pca9685_handle_t *handle, uint8_t reg, uint8_t val)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (handle->address << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_write_byte(cmd, val, true);
    i2c_master_stop(cmd);
    esp_err_t err = i2c_master_cmd_begin(handle->port, cmd, I2C_TIMEOUT_TICKS);
    i2c_cmd_link_delete(cmd);
    return err;
}

static esp_err_t pca9685_read_reg(pca9685_handle_t *handle, uint8_t reg, uint8_t *val)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (handle->address << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (handle->address << 1) | I2C_MASTER_READ, true);
    i2c_master_read_byte(cmd, val, I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    esp_err_t err = i2c_master_cmd_begin(handle->port, cmd, I2C_TIMEOUT_TICKS);
    i2c_cmd_link_delete(cmd);
    return err;
}

static esp_err_t pca9685_write_buf(pca9685_handle_t *handle, const uint8_t *buf, size_t len)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (handle->address << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(cmd, buf, len, true);
    i2c_master_stop(cmd);
    esp_err_t err = i2c_master_cmd_begin(handle->port, cmd, I2C_TIMEOUT_TICKS);
    i2c_cmd_link_delete(cmd);
    return err;
}

esp_err_t pca9685_init(const pca9685_config_t *cfg, pca9685_handle_t *handle)
{
    handle->port = cfg->i2c_port;
    handle->address = cfg->address;

    // Configure I2C bus using legacy driver
    i2c_config_t i2c_cfg = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = cfg->sda_gpio,
        .scl_io_num = cfg->scl_gpio,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = cfg->i2c_freq_hz,
    };

    esp_err_t err = i2c_param_config(cfg->i2c_port, &i2c_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C param config failed: %s", esp_err_to_name(err));
        return err;
    }

    err = i2c_driver_install(cfg->i2c_port, I2C_MODE_MASTER, 0, 0, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C driver install failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "I2C bus initialized on port %d (SDA=%d, SCL=%d)", cfg->i2c_port, cfg->sda_gpio,
             cfg->scl_gpio);

    // Reset: write MODE1 to sleep
    err = pca9685_write_reg(handle, PCA9685_REG_MODE1, MODE1_SLEEP);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write MODE1 (sleep): %s", esp_err_to_name(err));
        return err;
    }

    // Enable auto-increment
    err = pca9685_write_reg(handle, PCA9685_REG_MODE1, MODE1_SLEEP | MODE1_AI);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write MODE1 (auto-inc): %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "Initialized at address 0x%02X", cfg->address);
    return ESP_OK;
}

esp_err_t pca9685_set_frequency(pca9685_handle_t *handle, uint16_t freq_hz)
{
    // prescale = round(osc / (4096 * freq)) - 1
    float prescale_val = roundf(PCA9685_OSC_FREQ / (4096.0f * freq_hz)) - 1.0f;
    uint8_t prescale = (uint8_t)prescale_val;

    // Must be in sleep mode to change prescale
    uint8_t mode1;
    esp_err_t err = pca9685_read_reg(handle, PCA9685_REG_MODE1, &mode1);
    if (err != ESP_OK) return err;

    err = pca9685_write_reg(handle, PCA9685_REG_MODE1, (mode1 & ~MODE1_RESTART) | MODE1_SLEEP);
    if (err != ESP_OK) return err;

    err = pca9685_write_reg(handle, PCA9685_REG_PRE_SCALE, prescale);
    if (err != ESP_OK) return err;

    // Wake up
    err = pca9685_write_reg(handle, PCA9685_REG_MODE1, mode1 & ~MODE1_SLEEP);
    if (err != ESP_OK) return err;

    // Wait for oscillator to stabilize, then restart
    vTaskDelay(pdMS_TO_TICKS(5));
    err = pca9685_write_reg(handle, PCA9685_REG_MODE1, (mode1 & ~MODE1_SLEEP) | MODE1_RESTART);

    ESP_LOGI(TAG, "Frequency set to %u Hz (prescale=%u)", freq_hz, prescale);
    return err;
}

esp_err_t pca9685_set_channel_pwm(pca9685_handle_t *handle, uint8_t channel, uint16_t pwm)
{
    if (channel > 15) return ESP_ERR_INVALID_ARG;
    if (pwm > 4095) pwm = 4095;

    uint8_t reg = LED_REG(channel);
    // ON at tick 0, OFF at tick `pwm`
    uint8_t buf[5] = {
        reg,
        0x00,                  // ON_L
        0x00,                  // ON_H
        (uint8_t)(pwm & 0xFF), // OFF_L
        (uint8_t)(pwm >> 8),   // OFF_H
    };
    return pca9685_write_buf(handle, buf, sizeof(buf));
}

esp_err_t pca9685_set_channel_off(pca9685_handle_t *handle, uint8_t channel)
{
    if (channel > 15) return ESP_ERR_INVALID_ARG;

    uint8_t reg = LED_REG(channel);
    // Full OFF: set bit 4 in OFF_H
    uint8_t buf[5] = {
        reg,
        0x00, // ON_L
        0x00, // ON_H
        0x00, // OFF_L
        0x10, // OFF_H (bit 4 = full off)
    };
    return pca9685_write_buf(handle, buf, sizeof(buf));
}

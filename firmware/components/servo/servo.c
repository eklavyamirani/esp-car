#include "servo.h"
#include "esp_log.h"

static const char *TAG = "SERVO_DRV";

// 50 Hz → 20 ms period
#define PWM_PERIOD_US 20000

#define DEFAULT_MIN_PULSE_US 500
#define DEFAULT_MAX_PULSE_US 2500

esp_err_t servo_init(const servo_config_t *cfg, servo_handle_t *handle)
{
    handle->pca = cfg->pca;
    handle->channel = cfg->channel;
    handle->min_pulse_us = cfg->min_pulse_us ? cfg->min_pulse_us : DEFAULT_MIN_PULSE_US;
    handle->max_pulse_us = cfg->max_pulse_us ? cfg->max_pulse_us : DEFAULT_MAX_PULSE_US;

    ESP_LOGI(TAG, "Servo on channel %d (pulse %u–%u us)",
             handle->channel, handle->min_pulse_us, handle->max_pulse_us);
    return ESP_OK;
}

esp_err_t servo_set_pulse_us(servo_handle_t *handle, uint16_t pulse_us)
{
    uint16_t ticks = (uint16_t)((uint32_t)pulse_us * 4096 / PWM_PERIOD_US);
    return pca9685_set_channel_pwm(handle->pca, handle->channel, ticks);
}

esp_err_t servo_set_angle(servo_handle_t *handle, float angle)
{
    if (angle < 0.0f) angle = 0.0f;
    if (angle > 180.0f) angle = 180.0f;

    uint16_t range = handle->max_pulse_us - handle->min_pulse_us;
    uint16_t pulse_us = handle->min_pulse_us + (uint16_t)(range * angle / 180.0f);
    return servo_set_pulse_us(handle, pulse_us);
}

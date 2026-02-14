#include "motor.h"
#include "pca9685.h"
#include "esp_log.h"

static const char *TAG = "MOTOR_DRV";

// PCA9685 channels for each motor (IN1 = forward, IN2 = reverse)
#define M1_IN1  15
#define M1_IN2  14
#define M2_IN1  9
#define M2_IN2  8
#define M3_IN1  12
#define M3_IN2  13
#define M4_IN1  10
#define M4_IN2  11

#define MOTOR_SPEED_MIN -4095
#define MOTOR_SPEED_MAX  4095

// Direction multipliers (change to -1 if a motor is wired backwards)
#define MOTOR_1_DIR  1
#define MOTOR_2_DIR  1
#define MOTOR_3_DIR  1
#define MOTOR_4_DIR  1

static pca9685_handle_t *s_pca;

static int clamp(int val, int lo, int hi)
{
    if (val < lo) return lo;
    if (val > hi) return hi;
    return val;
}

static esp_err_t set_motor(uint8_t ch_fwd, uint8_t ch_rev, int speed)
{
    esp_err_t err;
    if (speed >= 0) {
        err = pca9685_set_channel_pwm(s_pca, ch_fwd, (uint16_t)speed);
        if (err != ESP_OK) return err;
        return pca9685_set_channel_pwm(s_pca, ch_rev, 0);
    } else {
        speed = -speed;
        err = pca9685_set_channel_pwm(s_pca, ch_fwd, 0);
        if (err != ESP_OK) return err;
        return pca9685_set_channel_pwm(s_pca, ch_rev, (uint16_t)speed);
    }
}

esp_err_t motor_init(pca9685_handle_t *pca)
{
    s_pca = pca;
    ESP_LOGI(TAG, "Motor driver attached to PCA9685");
    return motor_stop();
}

esp_err_t motor_move(int m1, int m2, int m3, int m4)
{
    m1 = MOTOR_1_DIR * clamp(m1, MOTOR_SPEED_MIN, MOTOR_SPEED_MAX);
    m2 = MOTOR_2_DIR * clamp(m2, MOTOR_SPEED_MIN, MOTOR_SPEED_MAX);
    m3 = MOTOR_3_DIR * clamp(m3, MOTOR_SPEED_MIN, MOTOR_SPEED_MAX);
    m4 = MOTOR_4_DIR * clamp(m4, MOTOR_SPEED_MIN, MOTOR_SPEED_MAX);

    esp_err_t err;
    err = set_motor(M1_IN1, M1_IN2, m1); if (err != ESP_OK) return err;
    err = set_motor(M2_IN1, M2_IN2, m2); if (err != ESP_OK) return err;
    err = set_motor(M3_IN1, M3_IN2, m3); if (err != ESP_OK) return err;
    err = set_motor(M4_IN1, M4_IN2, m4); if (err != ESP_OK) return err;
    return ESP_OK;
}

esp_err_t motor_stop(void)
{
    return motor_move(0, 0, 0, 0);
}

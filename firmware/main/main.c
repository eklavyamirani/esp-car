#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "pca9685.h"
#include "servo.h"
#include "motor.h"

static const char *TAG = "MAIN";

// I2C / PCA9685 configuration (shared by motors and servos)
#define I2C_PORT        I2C_NUM_0
#define SDA_GPIO        13
#define SCL_GPIO        14
#define PCA9685_ADDR    0x5F
#define I2C_FREQ_HZ     100000
#define PWM_FREQ_HZ     50

// Servo PCA9685 channels
#define SERVO_CH_TILT   0
#define SERVO_CH_PAN    1

#define SERVO_STEP_DELAY_MS 500

static pca9685_handle_t s_pca;
static servo_handle_t s_servo_tilt;
static servo_handle_t s_servo_pan;

static void servo_demo_task(void *arg)
{
    const int angles[] = {0, 90, 180, 90};
    const int n = sizeof(angles) / sizeof(angles[0]);
    int i = 0;

    while (1) {
        int angle = angles[i % n];
        ESP_LOGI(TAG, "SERVO: PAN=%d TILT=%d", angle, angle);
        servo_set_angle(&s_servo_pan, (float)angle);
        servo_set_angle(&s_servo_tilt, (float)angle);
        vTaskDelay(pdMS_TO_TICKS(SERVO_STEP_DELAY_MS));
        i++;
    }
}

#if CONFIG_MOTORS_ENABLED

#define DEMO_SPEED 2000
#define STEP_DELAY_MS 1000

static void motor_demo_task(void *arg)
{
    while (1) {
        ESP_LOGI(TAG, "MOTOR: FORWARD");
        motor_move(DEMO_SPEED, DEMO_SPEED, DEMO_SPEED, DEMO_SPEED);
        vTaskDelay(pdMS_TO_TICKS(STEP_DELAY_MS));

        ESP_LOGI(TAG, "MOTOR: STOP");
        motor_stop();
        vTaskDelay(pdMS_TO_TICKS(STEP_DELAY_MS));

        ESP_LOGI(TAG, "MOTOR: BACKWARD");
        motor_move(-DEMO_SPEED, -DEMO_SPEED, -DEMO_SPEED, -DEMO_SPEED);
        vTaskDelay(pdMS_TO_TICKS(STEP_DELAY_MS));

        ESP_LOGI(TAG, "MOTOR: STOP");
        motor_stop();
        vTaskDelay(pdMS_TO_TICKS(STEP_DELAY_MS));

        ESP_LOGI(TAG, "MOTOR: TURN_LEFT");
        motor_move(-DEMO_SPEED, -DEMO_SPEED, DEMO_SPEED, DEMO_SPEED);
        vTaskDelay(pdMS_TO_TICKS(STEP_DELAY_MS));

        ESP_LOGI(TAG, "MOTOR: STOP");
        motor_stop();
        vTaskDelay(pdMS_TO_TICKS(STEP_DELAY_MS));

        ESP_LOGI(TAG, "MOTOR: TURN_RIGHT");
        motor_move(DEMO_SPEED, DEMO_SPEED, -DEMO_SPEED, -DEMO_SPEED);
        vTaskDelay(pdMS_TO_TICKS(STEP_DELAY_MS));

        ESP_LOGI(TAG, "MOTOR: STOP");
        motor_stop();
        vTaskDelay(pdMS_TO_TICKS(STEP_DELAY_MS));
    }
}

#endif /* CONFIG_MOTORS_ENABLED */

void app_main(void)
{
    // Initialize PCA9685 (shared by motors and servos)
    pca9685_config_t pca_cfg = {
        .i2c_port = I2C_PORT,
        .sda_gpio = SDA_GPIO,
        .scl_gpio = SCL_GPIO,
        .address = PCA9685_ADDR,
        .i2c_freq_hz = I2C_FREQ_HZ,
    };

    esp_err_t err = pca9685_init(&pca_cfg, &s_pca);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "PCA9685 init failed: %s", esp_err_to_name(err));
        return;
    }

    err = pca9685_set_frequency(&s_pca, PWM_FREQ_HZ);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "PCA9685 set frequency failed: %s", esp_err_to_name(err));
        return;
    }

    // Initialize servos
    servo_config_t tilt_cfg = { .pca = &s_pca, .channel = SERVO_CH_TILT };
    servo_init(&tilt_cfg, &s_servo_tilt);

    servo_config_t pan_cfg = { .pca = &s_pca, .channel = SERVO_CH_PAN };
    servo_init(&pan_cfg, &s_servo_pan);

    // Center servos on startup
    servo_set_angle(&s_servo_pan, 90);
    servo_set_angle(&s_servo_tilt, 90);
    ESP_LOGI(TAG, "Servos initialized and centered");

    xTaskCreate(servo_demo_task, "servo_demo", 4096, NULL, 5, NULL);

#if CONFIG_MOTORS_ENABLED
    err = motor_init(&s_pca);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Motor init failed: %s", esp_err_to_name(err));
        return;
    }
    ESP_LOGI(TAG, "Motor control initialized");
    xTaskCreate(motor_demo_task, "motor_demo", 4096, NULL, 5, NULL);
#else
    ESP_LOGI(TAG, "Motors disabled (enable via menuconfig -> ESP Car)");
#endif
}

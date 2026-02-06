#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "motor.h"

static const char *TAG = "MOTOR";

#define DEMO_SPEED 2000
#define STEP_DELAY_MS 1000

static void motor_demo_task(void *arg)
{
    while (1) {
        ESP_LOGI(TAG, "FORWARD");
        motor_move(DEMO_SPEED, DEMO_SPEED, DEMO_SPEED, DEMO_SPEED);
        vTaskDelay(pdMS_TO_TICKS(STEP_DELAY_MS));

        ESP_LOGI(TAG, "STOP");
        motor_stop();
        vTaskDelay(pdMS_TO_TICKS(STEP_DELAY_MS));

        ESP_LOGI(TAG, "BACKWARD");
        motor_move(-DEMO_SPEED, -DEMO_SPEED, -DEMO_SPEED, -DEMO_SPEED);
        vTaskDelay(pdMS_TO_TICKS(STEP_DELAY_MS));

        ESP_LOGI(TAG, "STOP");
        motor_stop();
        vTaskDelay(pdMS_TO_TICKS(STEP_DELAY_MS));

        ESP_LOGI(TAG, "TURN_LEFT");
        motor_move(-DEMO_SPEED, -DEMO_SPEED, DEMO_SPEED, DEMO_SPEED);
        vTaskDelay(pdMS_TO_TICKS(STEP_DELAY_MS));

        ESP_LOGI(TAG, "STOP");
        motor_stop();
        vTaskDelay(pdMS_TO_TICKS(STEP_DELAY_MS));

        ESP_LOGI(TAG, "TURN_RIGHT");
        motor_move(DEMO_SPEED, DEMO_SPEED, -DEMO_SPEED, -DEMO_SPEED);
        vTaskDelay(pdMS_TO_TICKS(STEP_DELAY_MS));

        ESP_LOGI(TAG, "STOP");
        motor_stop();
        vTaskDelay(pdMS_TO_TICKS(STEP_DELAY_MS));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Initializing motor control...");
    esp_err_t err = motor_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Motor init failed: %s", esp_err_to_name(err));
        return;
    }
    ESP_LOGI(TAG, "Motor control initialized");

    xTaskCreate(motor_demo_task, "motor_demo", 4096, NULL, 5, NULL);
}

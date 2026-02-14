#pragma once

#include "esp_err.h"
#include "pca9685.h"

/**
 * @brief Initialize the motor subsystem using an already-initialized PCA9685.
 */
esp_err_t motor_init(pca9685_handle_t *pca);

/**
 * @brief Set speed for all 4 motors individually.
 * @param m1-m4 Speed values in range [-4095, 4095]. Sign controls direction.
 */
esp_err_t motor_move(int m1, int m2, int m3, int m4);

/**
 * @brief Stop all motors.
 */
esp_err_t motor_stop(void);

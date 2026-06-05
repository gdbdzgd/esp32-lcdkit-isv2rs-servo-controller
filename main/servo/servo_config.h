#ifndef SERVO_CONFIG_H
#define SERVO_CONFIG_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#define SERVO_NVS_NAMESPACE "servo_cfg"

#define SERVO_DEFAULT_SPEED_STEP      100
#define SERVO_DEFAULT_DIRECTION       0
#define SERVO_DEFAULT_TARGET_SPEED    500
#define SERVO_DEFAULT_MODBUS_ADDR     16
#define SERVO_DEFAULT_BAUDRATE        9600
#define SERVO_DEFAULT_PULSE_RATIO     60
#define SERVO_DEFAULT_MODE            0
#define SERVO_DEFAULT_VOLUME          80
#define SERVO_DEFAULT_POS_SPEED       100

typedef struct {
    uint16_t speed_step;
    uint8_t direction;
    uint16_t target_speed;
    uint8_t modbus_addr;
    uint32_t baudrate;
    uint8_t mode;
    uint16_t pulse_ratio;
    uint8_t volume;
    uint16_t position_speed;
} servo_config_t;

esp_err_t servo_config_init(void);
esp_err_t servo_config_load(servo_config_t *config);
esp_err_t servo_config_save(const servo_config_t *config);
esp_err_t servo_config_reset(void);

#endif

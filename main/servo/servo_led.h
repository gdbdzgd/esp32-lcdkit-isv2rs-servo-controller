#ifndef SERVO_LED_H
#define SERVO_LED_H

#include <stdint.h>
#include "esp_err.h"

typedef enum {
    LED_STATE_STOPPED = 0,
    LED_STATE_RUNNING,
    LED_STATE_ALARM,
    LED_STATE_MENU,
} led_state_e;

esp_err_t servo_led_init(void);
void servo_led_set_state(led_state_e state);
void servo_led_set_color(uint8_t r, uint8_t g, uint8_t b);
void servo_led_off(void);

#endif

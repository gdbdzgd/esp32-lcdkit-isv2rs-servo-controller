#ifndef UI_SERVO_H
#define UI_SERVO_H

#include "lvgl.h"
#include "lv_example_pub.h"
#include "isv2rs6040.h"

typedef enum {
    SERVO_STATE_STOPPED = 0,
    SERVO_STATE_RUNNING,
    SERVO_STATE_ALARM,
} servo_state_e;

typedef enum {
    SERVO_DIR_FORWARD = 0,
    SERVO_DIR_BACKWARD,
} servo_dir_e;

typedef enum {
    SERVO_MODE_SPEED = 0,
    SERVO_MODE_POSITION,
    SERVO_MODE_PR,
} servo_mode_e;

typedef struct {
    uint16_t target_speed;
    uint16_t speed_step;
    servo_dir_e direction;
    servo_state_e state;
    servo_mode_e mode;
    int16_t actual_speed;
    int16_t actual_position;
    uint16_t alarm_code;
    bool enabled;
    uint8_t modbus_addr;
    uint32_t baudrate;
    uint16_t pulse_ratio;
    int8_t volume;
    uint16_t position_speed;
} servo_ui_data_t;

extern lv_layer_t servo_main_layer;
extern lv_layer_t servo_menu_layer;
extern servo_ui_data_t servo_data;
extern int32_t position_target;
extern bool ui_on_main_screen;

void ui_servo_main_init(lv_obj_t *parent);
void ui_servo_menu_init(lv_obj_t *parent);
void ui_servo_update_speed(int16_t speed);
void ui_servo_update_state(servo_state_e state);
void ui_servo_update_direction(servo_dir_e dir);
void ui_servo_update_alarm(uint16_t alarm_code);
void ui_servo_update_position(int16_t position);
void ui_servo_update_position_display(int32_t position);
void ui_servo_update_mode(void);
void ui_servo_play_click_sound(void);
void ui_servo_play_long_sound(void);
void ui_servo_play_rotate_sound(void);
void ui_servo_set_volume(int8_t volume);
int8_t ui_servo_get_volume(void);

void servo_reset_alarm(void);
void servo_menu_request_save(void);
void servo_position_move(int16_t delta);
void servo_handwheel_trigger(bool forward);
void servo_handwheel_stop(void);
void servo_set_position_increment(int32_t inc);
void servo_request_reinit(void);

#endif
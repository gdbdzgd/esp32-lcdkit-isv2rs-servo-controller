#include "lvgl.h"
#include <stdio.h>
#include "esp_log.h"
#include "lv_example_pub.h"
#include "lv_example_image.h"
#include "ui_servo.h"
#include "servo_audio.h"
#include "servo_led.h"
#include "bsp/esp-bsp.h"

#define UI_DEBUG 0
#if UI_DEBUG
#define UI_LOGD(fmt, ...) ESP_LOGD(TAG, fmt, ##__VA_ARGS__)
#define UI_LOGI(fmt, ...) ESP_LOGI(TAG, fmt, ##__VA_ARGS__)
#else
#define UI_LOGD(fmt, ...)
#define UI_LOGI(fmt, ...)
#endif

static const char *TAG = "ui_servo";

bool ui_on_main_screen = false;
static bool ui_main_initialized = false;

servo_ui_data_t servo_data = {
    .target_speed = 500,
    .speed_step = 100,
    .direction = SERVO_DIR_BACKWARD,
    .state = SERVO_STATE_STOPPED,
    .actual_speed = 0,
    .alarm_code = 0,
    .enabled = false,
    .modbus_addr = 16,
    .baudrate = 9600,
    .volume = 80,
};

static lv_obj_t *lbl_speed_value;
static lv_obj_t *lbl_actual_speed;
static lv_obj_t *lbl_state;
static lv_obj_t *lbl_direction;
static lv_obj_t *lbl_alarm;
static lv_obj_t *lbl_unit;
static lv_obj_t *arc_speed;

static time_out_count time_200ms;
static time_out_count time_key;
static int32_t last_encoder_time = 0;
static bool handwheel_running = false;

static bool servo_main_enter_cb(void *layer);
static bool servo_main_exit_cb(void *layer);
static void servo_main_timer_cb(lv_timer_t *tmr);

lv_layer_t servo_main_layer = {
    .lv_obj_name    = "servo_main_Layer",
    .lv_obj_parent  = NULL,
    .lv_obj_layer   = NULL,
    .lv_show_layer  = NULL,
    .enter_cb       = servo_main_enter_cb,
    .exit_cb        = servo_main_exit_cb,
    .timer_cb       = servo_main_timer_cb,
};

void ui_servo_play_click_sound(void)
{
    servo_audio_play_click();
}

void ui_servo_play_long_sound(void)
{
    servo_audio_play_long();
}

void ui_servo_play_rotate_sound(void)
{
    servo_audio_play_rotate();
}

void ui_servo_set_volume(int8_t volume)
{
    servo_data.volume = volume;
    servo_audio_set_volume(volume);
}

int8_t ui_servo_get_volume(void)
{
    return servo_data.volume;
}

static void servo_main_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (LV_EVENT_FOCUSED == code) {
        lv_group_set_editing(lv_group_get_default(), true);
    } else if (LV_EVENT_KEY == code) {
        uint32_t key = lv_event_get_key(e);
        if (LV_KEY_LEFT == key || LV_KEY_RIGHT == key) {
            if (servo_data.mode == SERVO_MODE_POSITION || servo_data.mode == SERVO_MODE_PR) {
                if (servo_data.state == SERVO_STATE_RUNNING) {
                    bool forward = (LV_KEY_RIGHT == key);
                    int32_t inc = (forward ? 1 : -1) * servo_data.speed_step * servo_data.pulse_ratio;
                    if (servo_data.mode == SERVO_MODE_PR) {
                        servo_set_position_increment(inc);
                    } else {
                        servo_handwheel_trigger(forward);
                    }
                    handwheel_running = true;
                    last_encoder_time = lv_tick_get();
                    position_target += inc;
                    char buf[32];
                    snprintf(buf, sizeof(buf), "%ld", (long)position_target);
                    lv_label_set_text(lbl_speed_value, buf);
                } else if (servo_data.state == SERVO_STATE_STOPPED) {
                    if (LV_KEY_RIGHT == key) {
                        servo_data.target_speed += servo_data.speed_step;
                        if (servo_data.target_speed > 3000) servo_data.target_speed = 3000;
                    } else {
                        if (servo_data.target_speed >= servo_data.speed_step) {
                            servo_data.target_speed -= servo_data.speed_step;
                        } else {
                            servo_data.target_speed = 0;
                        }
                    }
                    char buf[16];
                    snprintf(buf, sizeof(buf), "%d", servo_data.target_speed);
                    lv_label_set_text(lbl_speed_value, buf);
                    lv_arc_set_value(arc_speed, servo_data.target_speed);
                }
            } else {
                if (servo_data.state == SERVO_STATE_STOPPED || servo_data.state == SERVO_STATE_RUNNING) {
                    if (LV_KEY_RIGHT == key) {
                        servo_data.target_speed += servo_data.speed_step;
                        if (servo_data.target_speed > 3000) servo_data.target_speed = 3000;
                    } else {
                        if (servo_data.target_speed >= servo_data.speed_step) {
                            servo_data.target_speed -= servo_data.speed_step;
                        } else {
                            servo_data.target_speed = 0;
                        }
                    }
                    char buf[16];
                    snprintf(buf, sizeof(buf), "%d", servo_data.target_speed);
                    lv_label_set_text(lbl_speed_value, buf);
                    lv_arc_set_value(arc_speed, servo_data.target_speed);
                }
            }
            ui_servo_play_rotate_sound();
        }
        feed_clock_time();
    } else if (LV_EVENT_CLICKED == code) {
        ui_servo_play_click_sound();
        if (servo_data.state == SERVO_STATE_ALARM) {
            servo_reset_alarm();
        } else if (servo_data.state == SERVO_STATE_STOPPED) {
            servo_data.state = SERVO_STATE_RUNNING;
            servo_data.enabled = true;
            servo_led_set_state(LED_STATE_RUNNING);
        } else if (servo_data.state == SERVO_STATE_RUNNING) {
            servo_data.state = SERVO_STATE_STOPPED;
            servo_data.enabled = false;
            servo_led_set_state(LED_STATE_STOPPED);
        }
        ui_servo_update_state(servo_data.state);
        feed_clock_time();
    } else if (LV_EVENT_LONG_PRESSED == code) {
        ui_servo_play_long_sound();
        lv_indev_wait_release(lv_indev_get_next(NULL));
        if (servo_data.state == SERVO_STATE_RUNNING) {
            servo_data.state = SERVO_STATE_STOPPED;
            servo_data.enabled = false;
            ui_servo_update_state(servo_data.state);
            servo_led_set_state(LED_STATE_STOPPED);
        }
        servo_led_set_state(LED_STATE_MENU);
        ui_on_main_screen = false;
        ui_remove_all_objs_from_encoder_group();
        lv_func_goto_layer(&servo_menu_layer);
        feed_clock_time();
    }
}

static void anim_speed_ready_cb(lv_anim_t *a)
{
    lv_obj_t *obj = (lv_obj_t *)a->var;
    if (obj) {
        lv_obj_del(obj);
    }
}

static void anim_opa_cb(void *var, int32_t v)
{
    lv_obj_set_style_opa((lv_obj_t *)var, v, 0);
}

void ui_servo_update_speed(int16_t speed)
{
    servo_data.actual_speed = speed;
    UI_LOGI("update_speed: lbl=%p arc=%p", lbl_speed_value, arc_speed);
    if (!lbl_speed_value || !arc_speed) {
        UI_LOGI("update_speed: NULL ptr");
        return;
    }
    if (!lv_obj_is_valid(lbl_speed_value) || !lv_obj_is_valid(arc_speed)) {
        UI_LOGI("update_speed: invalid obj");
        return;
    }
    
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", abs(speed));
    lv_label_set_text(lbl_speed_value, buf);
    lv_arc_set_value(arc_speed, abs(speed));
}

void ui_servo_update_position_display(int32_t position)
{
    if (!lbl_speed_value || !arc_speed) {
        return;
    }
    if (!lv_obj_is_valid(lbl_speed_value) || !lv_obj_is_valid(arc_speed)) {
        return;
    }
    
    char buf[32];
    snprintf(buf, sizeof(buf), "%ld", (long)position);
    lv_label_set_text(lbl_speed_value, buf);
}

void ui_servo_update_state(servo_state_e state)
{
    servo_data.state = state;
    UI_LOGI("update_state: lbl=%p", lbl_state);
    if (!lbl_state) {
        UI_LOGI("update_state: NULL ptr");
        return;
    }
    if (!lv_obj_is_valid(lbl_state)) {
        UI_LOGI("update_state: invalid obj");
        return;
    }

    switch (state) {
    case SERVO_STATE_STOPPED:
        lv_label_set_text(lbl_state, "停止");
        lv_obj_set_style_text_font(lbl_state, &font_servo_cn_22, 0);
        lv_obj_set_style_text_color(lbl_state, lv_color_hex(0x888888), 0);
        break;
    case SERVO_STATE_RUNNING:
        lv_label_set_text(lbl_state, "运行");
        lv_obj_set_style_text_font(lbl_state, &font_servo_cn_22, 0);
        lv_obj_set_style_text_color(lbl_state, lv_color_hex(0x00FF00), 0);
        break;
    case SERVO_STATE_ALARM:
        lv_label_set_text(lbl_state, "报警");
        lv_obj_set_style_text_font(lbl_state, &font_servo_cn_22, 0);
        lv_obj_set_style_text_color(lbl_state, lv_color_hex(0xFF0000), 0);
        break;
    }
}

void ui_servo_update_direction(servo_dir_e dir)
{
    UI_LOGI("update_dir: lbl=%p", lbl_direction);
    if (!lbl_direction) {
        UI_LOGI("update_dir: NULL ptr");
        return;
    }
    if (!lv_obj_is_valid(lbl_direction)) {
        UI_LOGI("update_dir: invalid obj");
        return;
    }
    lv_label_set_text(lbl_direction, dir == SERVO_DIR_FORWARD ? "正转" : "反转");
    lv_obj_set_style_text_font(lbl_direction, &font_servo_cn_22, 0);
    lv_obj_set_style_text_color(lbl_direction,
        dir == SERVO_DIR_FORWARD ? lv_color_hex(0x00CCFF) : lv_color_hex(0xFF8800), 0);
}

void ui_servo_update_alarm(uint16_t alarm_code)
{
    servo_data.alarm_code = alarm_code;
    UI_LOGI("update_alarm: lbl=%p code=%04X", lbl_alarm, alarm_code);
    if (!lbl_alarm) {
        UI_LOGI("update_alarm: NULL ptr");
        return;
    }
    if (!lv_obj_is_valid(lbl_alarm)) {
        UI_LOGI("update_alarm: invalid obj");
        return;
    }
    if (alarm_code == 0) {
        lv_obj_add_flag(lbl_alarm, LV_OBJ_FLAG_HIDDEN);
    } else {
        char buf[32];
        snprintf(buf, sizeof(buf), "报警:%04X", alarm_code);
        lv_label_set_text(lbl_alarm, buf);
        lv_obj_clear_flag(lbl_alarm, LV_OBJ_FLAG_HIDDEN);
    }
}

void ui_servo_update_mode(void)
{
    if (!lbl_unit || !lbl_speed_value || !arc_speed) return;
    
    if (servo_data.mode == SERVO_MODE_SPEED) {
        lv_label_set_text(lbl_unit, "RPM");
        lv_obj_set_style_text_font(lbl_unit, &HelveticaNeue_Regular_24, 0);
        lv_arc_set_range(arc_speed, 0, 3000);
        lv_arc_set_value(arc_speed, servo_data.target_speed);
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", servo_data.target_speed);
        lv_label_set_text(lbl_speed_value, buf);
    } else if (servo_data.mode == SERVO_MODE_POSITION) {
        lv_label_set_text(lbl_unit, "位置");
        lv_obj_set_style_text_font(lbl_unit, &font_servo_cn_22, 0);
        lv_arc_set_range(arc_speed, 0, 1);
        lv_arc_set_value(arc_speed, 1);
        char buf[16];
        snprintf(buf, sizeof(buf), "%ld", (long)position_target);
        lv_label_set_text(lbl_speed_value, buf);
    } else {
        lv_label_set_text(lbl_unit, "PR");
        lv_obj_set_style_text_font(lbl_unit, &font_servo_cn_22, 0);
        lv_arc_set_range(arc_speed, 0, 1);
        lv_arc_set_value(arc_speed, 1);
        char buf[16];
        snprintf(buf, sizeof(buf), "%ld", (long)position_target);
        lv_label_set_text(lbl_speed_value, buf);
    }
}

void ui_servo_main_init(lv_obj_t *parent)
{
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(parent, lv_color_hex(0x1A1A2E), 0);

    arc_speed = lv_arc_create(parent);
    lv_obj_set_size(arc_speed, 240, 240);
    lv_obj_center(arc_speed);
    lv_arc_set_range(arc_speed, 0, 3000);
    lv_arc_set_value(arc_speed, servo_data.target_speed);
    lv_arc_set_rotation(arc_speed, 135);
    lv_arc_set_bg_angles(arc_speed, 0, 270);
    lv_obj_set_style_arc_width(arc_speed, 12, LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc_speed, lv_color_hex(0x0A0A1A), LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc_speed, 12, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc_speed, lv_color_hex(0x00AAFF), LV_PART_INDICATOR);
    lv_obj_remove_style(arc_speed, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(arc_speed, LV_OBJ_FLAG_CLICKABLE);

    lbl_speed_value = lv_label_create(arc_speed);
    lv_label_set_text_fmt(lbl_speed_value, "%d", servo_data.target_speed);
    lv_obj_set_style_text_font(lbl_speed_value, &HelveticaNeue_Regular_48, 0);
    lv_obj_set_style_text_color(lbl_speed_value, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(lbl_speed_value, LV_ALIGN_CENTER, 0, -20);

    lbl_unit = lv_label_create(arc_speed);
    if (servo_data.mode == SERVO_MODE_SPEED) {
        lv_label_set_text(lbl_unit, "RPM");
        lv_obj_set_style_text_font(lbl_unit, &HelveticaNeue_Regular_24, 0);
    } else if (servo_data.mode == SERVO_MODE_POSITION) {
        lv_label_set_text(lbl_unit, "位置");
        lv_obj_set_style_text_font(lbl_unit, &font_servo_cn_22, 0);
    } else {
        lv_label_set_text(lbl_unit, "PR");
        lv_obj_set_style_text_font(lbl_unit, &font_servo_cn_22, 0);
    }
    lv_obj_set_style_text_color(lbl_unit, lv_color_hex(0x888888), 0);
    lv_obj_align(lbl_unit, LV_ALIGN_CENTER, 0, 18);

    lbl_state = lv_label_create(arc_speed);
    lv_label_set_text(lbl_state, "停止");
    lv_obj_set_style_text_font(lbl_state, &font_servo_cn_22, 0);
    lv_obj_set_style_text_color(lbl_state, lv_color_hex(0x888888), 0);
    lv_obj_align(lbl_state, LV_ALIGN_TOP_MID, 0, 20);

    lbl_direction = lv_label_create(arc_speed);
    lv_label_set_text(lbl_direction, servo_data.direction == SERVO_DIR_FORWARD ? "正转" : "反转");
    lv_obj_set_style_text_font(lbl_direction, &font_servo_cn_22, 0);
    lv_obj_set_style_text_color(lbl_direction, lv_color_hex(0x00CCFF), 0);
    lv_obj_align(lbl_direction, LV_ALIGN_BOTTOM_MID, 0, -35);

    lbl_alarm = lv_label_create(arc_speed);
    lv_label_set_text(lbl_alarm, "");
    lv_obj_set_style_text_font(lbl_alarm, &font_servo_cn_20, 0);
    lv_obj_set_style_text_color(lbl_alarm, lv_color_hex(0xFF4444), 0);
    lv_obj_align(lbl_alarm, LV_ALIGN_BOTTOM_MID, 0, -55);
    lv_obj_add_flag(lbl_alarm, LV_OBJ_FLAG_HIDDEN);

    lv_obj_add_event_cb(parent, servo_main_event_cb, LV_EVENT_FOCUSED, NULL);
    lv_obj_add_event_cb(parent, servo_main_event_cb, LV_EVENT_KEY, NULL);
    lv_obj_add_event_cb(parent, servo_main_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(parent, servo_main_event_cb, LV_EVENT_LONG_PRESSED, NULL);
    ui_add_obj_to_encoder_group(parent);
}

static bool servo_main_enter_cb(void *layer)
{
    bool ret = false;
    lv_layer_t *create_layer = layer;

    UI_LOGI("main_enter: layer=%p obj=%p", layer, create_layer->lv_obj_layer);

    if (NULL == create_layer->lv_obj_layer) {
        ret = true;
        create_layer->lv_obj_layer = lv_obj_create(lv_scr_act());
        lv_obj_remove_style_all(create_layer->lv_obj_layer);
        lv_obj_set_size(create_layer->lv_obj_layer, LV_HOR_RES, LV_VER_RES);

        ui_servo_main_init(create_layer->lv_obj_layer);
    }
    ui_main_initialized = true;
    ui_on_main_screen = true;
    set_time_out(&time_200ms, 200);
    feed_clock_time();

    UI_LOGI("main_enter: done arc=%p lbl_speed=%p", arc_speed, lbl_speed_value);

    return ret;
}

static bool servo_main_exit_cb(void *layer)
{
    UI_LOGI("main_exit: clearing ptrs arc=%p lbl_speed=%p lbl_state=%p",
            arc_speed, lbl_speed_value, lbl_state);
    ui_main_initialized = false;
    ui_on_main_screen = false;
    arc_speed = NULL;
    lbl_speed_value = NULL;
    lbl_state = NULL;
    lbl_direction = NULL;
    lbl_alarm = NULL;
    lbl_unit = NULL;
    return true;
}

static void servo_main_timer_cb(lv_timer_t *tmr)
{
    feed_clock_time();
    
    if (servo_data.mode == SERVO_MODE_POSITION && servo_data.state == SERVO_STATE_RUNNING) {
        int32_t now = lv_tick_get();
        if (handwheel_running && (now - last_encoder_time) > 100) {
            handwheel_running = false;
            servo_handwheel_stop();
        }
    }
}

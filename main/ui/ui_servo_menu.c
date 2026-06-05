#include "lvgl.h"
#include <stdio.h>
#include "esp_log.h"
#include "lv_example_pub.h"
#include "lv_example_image.h"
#include "ui_servo.h"
#include "servo_config.h"
#include "servo_led.h"

static const char *TAG = "ui_servo_menu";

static const uint16_t speed_steps[] = {10, 50, 100, 200, 500};
#define SPEED_STEP_COUNT (sizeof(speed_steps) / sizeof(speed_steps[0]))

static const uint16_t modbus_addrs[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20};
#define MODBUS_ADDR_COUNT (sizeof(modbus_addrs) / sizeof(modbus_addrs[0]))

static const uint32_t baudrates[] = {1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200};
#define BAUDRATE_COUNT (sizeof(baudrates) / sizeof(baudrates[0]))

static const uint16_t pulse_ratios[] = {1, 2, 3, 4, 6, 7, 8, 10, 12, 16, 20, 30, 32, 40, 50, 60, 80, 100};
#define PULSE_RATIO_COUNT (sizeof(pulse_ratios) / sizeof(pulse_ratios[0]))

#define MENU_ITEM_COUNT 10
#define VISIBLE_ITEMS 5

static const char *menu_names[] = {"速度步进", "旋转方向", "默认转速", "伺服地址", "波特率", "控制模式", "脉冲比例", "位置速度", "音量", "恢复出厂"};

static lv_obj_t *menu_containers[VISIBLE_ITEMS];
static lv_obj_t *menu_labels[VISIBLE_ITEMS];
static lv_obj_t *sep_top;
static lv_obj_t *sep_bottom;
static int8_t menu_index = 0;
static bool editing = false;
static bool animating = false;
static bool factory_reset_confirm = false;

static time_out_count time_200ms;

static bool servo_menu_enter_cb(void *layer);
static bool servo_menu_exit_cb(void *layer);
static void servo_menu_timer_cb(lv_timer_t *tmr);

lv_layer_t servo_menu_layer = {
    .lv_obj_name    = "servo_menu_Layer",
    .lv_obj_parent  = NULL,
    .lv_obj_layer   = NULL,
    .lv_show_layer  = NULL,
    .enter_cb       = servo_menu_enter_cb,
    .exit_cb        = servo_menu_exit_cb,
    .timer_cb       = servo_menu_timer_cb,
};

static int get_step_index(uint16_t step)
{
    for (int i = 0; i < SPEED_STEP_COUNT; i++) {
        if (speed_steps[i] == step) return i;
    }
    return 2;
}

static int get_addr_index(uint8_t addr)
{
    for (int i = 0; i < MODBUS_ADDR_COUNT; i++) {
        if (modbus_addrs[i] == addr) return i;
    }
    return 15;
}

static int get_baudrate_index(uint32_t baud)
{
    for (int i = 0; i < BAUDRATE_COUNT; i++) {
        if (baudrates[i] == baud) return i;
    }
    return 3;
}

static int get_ratio_index(uint16_t ratio)
{
    for (int i = 0; i < PULSE_RATIO_COUNT; i++) {
        if (pulse_ratios[i] == ratio) return i;
    }
    return 0;
}

static void get_menu_text(int idx, char *buf, size_t len)
{
    char val_buf[16];
    switch (idx) {
        case 0:
            snprintf(val_buf, sizeof(val_buf), "%d", speed_steps[get_step_index(servo_data.speed_step)]);
            snprintf(buf, len, "%d.\t速度步进\t%s", idx + 1, val_buf);
            break;
        case 1:
            snprintf(buf, len, "%d.\t旋转方向\t%s", idx + 1, servo_data.direction == SERVO_DIR_FORWARD ? "正转" : "反转");
            break;
        case 2:
            snprintf(val_buf, sizeof(val_buf), "%d", servo_data.target_speed);
            snprintf(buf, len, "%d.\t默认转速\t%s", idx + 1, val_buf);
            break;
        case 3:
            snprintf(val_buf, sizeof(val_buf), "%d", servo_data.modbus_addr);
            snprintf(buf, len, "%d.\t伺服地址\t%s", idx + 1, val_buf);
            break;
        case 4:
            snprintf(val_buf, sizeof(val_buf), "%lu", (unsigned long)servo_data.baudrate);
            snprintf(buf, len, "%d.\t波特率  \t%s", idx + 1, val_buf);
            break;
        case 5:
            snprintf(buf, len, "%d.\t控制模式\t%s", idx + 1, 
                servo_data.mode == SERVO_MODE_SPEED ? "速度" : 
                (servo_data.mode == SERVO_MODE_POSITION ? "位置" : "PR模式"));
            break;
        case 6:
            snprintf(val_buf, sizeof(val_buf), "1:%d", servo_data.pulse_ratio);
            snprintf(buf, len, "%d.\t脉冲比例\t%s", idx + 1, val_buf);
            break;
        case 7:
            snprintf(val_buf, sizeof(val_buf), "%d", servo_data.position_speed);
            snprintf(buf, len, "%d.\t位置速度\t%s", idx + 1, val_buf);
            break;
        case 8:
            snprintf(val_buf, sizeof(val_buf), "%d%%", servo_data.volume);
            snprintf(buf, len, "%d.\t音量    \t%s", idx + 1, val_buf);
            break;
        case 9:
            if (factory_reset_confirm) {
                snprintf(buf, len, "%d.\t确认?\t", idx + 1);
            } else {
                snprintf(buf, len, "%d.\t恢复出厂\t", idx + 1);
            }
            break;
        default:
            snprintf(buf, len, "");
            break;
    }
}

static void anim_y_cb(void *var, int32_t v)
{
    lv_obj_set_y(var, v);
}

static void anim_ready_cb(lv_anim_t *a)
{
    animating = false;
}

static void menu_update_display(void)
{
    char buf[48];
    
    for (int i = 0; i < VISIBLE_ITEMS; i++) {
        int item_idx = menu_index - 2 + i;
        if (item_idx < 0) item_idx += MENU_ITEM_COUNT;
        if (item_idx >= MENU_ITEM_COUNT) item_idx -= MENU_ITEM_COUNT;
        
        get_menu_text(item_idx, buf, sizeof(buf));
        lv_label_set_text(menu_labels[i], buf);
        lv_obj_set_style_text_font(menu_labels[i], &font_servo_cn_22, 0);
        
        if (i == 2) {
            if (editing) {
                lv_obj_set_style_bg_color(menu_containers[i], lv_color_hex(0x005500), 0);
                lv_obj_set_style_border_color(menu_containers[i], lv_color_hex(0x00FF00), 0);
                lv_obj_set_style_text_color(menu_labels[i], lv_color_hex(0x00FF00), 0);
            } else {
                lv_obj_set_style_bg_color(menu_containers[i], lv_color_hex(0x003366), 0);
                lv_obj_set_style_border_color(menu_containers[i], lv_color_hex(0x00AAFF), 0);
                lv_obj_set_style_text_color(menu_labels[i], lv_color_hex(0x00AAFF), 0);
            }
            lv_obj_set_style_bg_opa(menu_containers[i], LV_OPA_COVER, 0);
            lv_obj_set_style_border_width(menu_containers[i], 2, 0);
        } else {
            lv_obj_set_style_bg_opa(menu_containers[i], LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_width(menu_containers[i], 0, 0);
            lv_obj_set_style_text_color(menu_labels[i], lv_color_hex(0x555555), 0);
        }
    }
}

static void menu_slide_animation(int direction)
{
    if (animating) return;
    animating = true;
    
    int item_h = 40;
    int base_y = 10;
    
    for (int i = 0; i < VISIBLE_ITEMS; i++) {
        int target_y = (i - 2) * item_h + base_y;
        int start_y = target_y + direction * item_h;
        
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, menu_containers[i]);
        lv_anim_set_exec_cb(&a, anim_y_cb);
        lv_anim_set_values(&a, start_y, target_y);
        lv_anim_set_time(&a, 150);
        lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
        lv_anim_set_ready_cb(&a, anim_ready_cb);
        lv_anim_start(&a);
    }
}

static void servo_menu_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (LV_EVENT_FOCUSED == code) {
        lv_group_set_editing(lv_group_get_default(), true);
    } else if (LV_EVENT_KEY == code) {
        uint32_t key = lv_event_get_key(e);

        if (!editing) {
            int old_index = menu_index;
            if (LV_KEY_LEFT == key) {
                menu_index = (menu_index - 1 + MENU_ITEM_COUNT) % MENU_ITEM_COUNT;
            } else if (LV_KEY_RIGHT == key) {
                menu_index = (menu_index + 1) % MENU_ITEM_COUNT;
            }
            if (old_index != menu_index) {
                factory_reset_confirm = false;
                int direction = (menu_index > old_index || (old_index == MENU_ITEM_COUNT - 1 && menu_index == 0)) ? 1 : -1;
                menu_slide_animation(direction);
            }
            menu_update_display();
        } else {
            if (menu_index == 0) {
                int idx = get_step_index(servo_data.speed_step);
                if (LV_KEY_RIGHT == key) {
                    if (idx < SPEED_STEP_COUNT - 1) idx++;
                } else if (LV_KEY_LEFT == key) {
                    if (idx > 0) idx--;
                }
                servo_data.speed_step = speed_steps[idx];
            } else if (menu_index == 1) {
                if (LV_KEY_LEFT == key || LV_KEY_RIGHT == key) {
                    servo_dir_e old_dir = servo_data.direction;
                    servo_data.direction = (servo_data.direction == SERVO_DIR_FORWARD)
                                          ? SERVO_DIR_BACKWARD : SERVO_DIR_FORWARD;
                    ESP_LOGI("MENU", "Direction changed: %d -> %d (%s)", 
                             old_dir, servo_data.direction,
                             servo_data.direction == SERVO_DIR_FORWARD ? "Forward" : "Backward");
                }
            } else if (menu_index == 2) {
                if (LV_KEY_RIGHT == key) {
                    servo_data.target_speed += servo_data.speed_step;
                    if (servo_data.target_speed > 3000) servo_data.target_speed = 3000;
                } else if (LV_KEY_LEFT == key) {
                    if (servo_data.target_speed >= servo_data.speed_step) {
                        servo_data.target_speed -= servo_data.speed_step;
                    } else {
                        servo_data.target_speed = 0;
                    }
                }
            } else if (menu_index == 3) {
                int idx = get_addr_index(servo_data.modbus_addr);
                if (LV_KEY_RIGHT == key) {
                    if (idx < MODBUS_ADDR_COUNT - 1) idx++;
                } else if (LV_KEY_LEFT == key) {
                    if (idx > 0) idx--;
                }
                servo_data.modbus_addr = modbus_addrs[idx];
                servo_request_reinit();
            } else if (menu_index == 4) {
                int idx = get_baudrate_index(servo_data.baudrate);
                if (LV_KEY_RIGHT == key) {
                    if (idx < BAUDRATE_COUNT - 1) idx++;
                } else if (LV_KEY_LEFT == key) {
                    if (idx > 0) idx--;
                }
                servo_data.baudrate = baudrates[idx];
                servo_request_reinit();
            } else if (menu_index == 5) {
                if (LV_KEY_LEFT == key || LV_KEY_RIGHT == key) {
                    servo_mode_e old_mode = servo_data.mode;
                    servo_data.mode = (servo_data.mode + 1) % 3;
                    ESP_LOGI("MENU", "Control mode changed: %d -> %d (%s)", 
                             old_mode, servo_data.mode,
                             servo_data.mode == SERVO_MODE_SPEED ? "SPEED" :
                             servo_data.mode == SERVO_MODE_POSITION ? "POSITION" : "PR");
                }
            } else if (menu_index == 6) {
                int idx = get_ratio_index(servo_data.pulse_ratio);
                if (LV_KEY_RIGHT == key) {
                    if (idx < PULSE_RATIO_COUNT - 1) idx++;
                } else if (LV_KEY_LEFT == key) {
                    if (idx > 0) idx--;
                }
                if (servo_data.pulse_ratio != pulse_ratios[idx]) {
                    ESP_LOGI("MENU", "Pulse ratio changed: %d -> %d", servo_data.pulse_ratio, pulse_ratios[idx]);
                    servo_data.pulse_ratio = pulse_ratios[idx];
                }
            } else if (menu_index == 7) {
                int old_speed = servo_data.position_speed;
                if (LV_KEY_RIGHT == key) {
                    servo_data.position_speed += 10;
                    if (servo_data.position_speed > 1000) servo_data.position_speed = 1000;
                } else if (LV_KEY_LEFT == key) {
                    if (servo_data.position_speed >= 10) {
                        servo_data.position_speed -= 10;
                    } else {
                        servo_data.position_speed = 0;
                    }
                }
                if (old_speed != servo_data.position_speed) {
                    ESP_LOGI("MENU", "Position speed changed: %d -> %d RPM", old_speed, servo_data.position_speed);
                }
            } else if (menu_index == 8) {
                if (LV_KEY_RIGHT == key) {
                    servo_data.volume += 5;
                    if (servo_data.volume > 100) servo_data.volume = 100;
                } else if (LV_KEY_LEFT == key) {
                    servo_data.volume -= 5;
                    if (servo_data.volume < 0) servo_data.volume = 0;
                }
                ui_servo_set_volume(servo_data.volume);
            } else if (menu_index == 9) {
            }
            menu_update_display();
        }
        ui_servo_play_click_sound();
        feed_clock_time();
    } else if (LV_EVENT_CLICKED == code) {
        ui_servo_play_click_sound();
        if (menu_index == 8) {
            if (!factory_reset_confirm) {
                factory_reset_confirm = true;
                menu_update_display();
                feed_clock_time();
                return;
            }
            factory_reset_confirm = false;
            servo_config_reset();
            servo_data.speed_step = SERVO_DEFAULT_SPEED_STEP;
            servo_data.direction = SERVO_DEFAULT_DIRECTION;
            servo_data.target_speed = SERVO_DEFAULT_TARGET_SPEED;
            servo_data.modbus_addr = SERVO_DEFAULT_MODBUS_ADDR;
            servo_data.baudrate = SERVO_DEFAULT_BAUDRATE;
            servo_data.mode = SERVO_MODE_SPEED;
            servo_data.pulse_ratio = SERVO_DEFAULT_PULSE_RATIO;
            servo_data.volume = SERVO_DEFAULT_VOLUME;
            servo_data.position_speed = SERVO_DEFAULT_POS_SPEED;
            ui_servo_set_volume(servo_data.volume);
            servo_request_reinit();
        }
        if (!editing) {
            editing = true;
        } else {
            editing = false;
        }
        menu_update_display();
        feed_clock_time();
    } else if (LV_EVENT_LONG_PRESSED == code) {
        ui_servo_play_long_sound();
        lv_indev_wait_release(lv_indev_get_next(NULL));
        editing = false;
        lv_group_set_editing(lv_group_get_default(), false);
        ui_remove_all_objs_from_encoder_group();
        servo_menu_request_save();
        ui_on_main_screen = true;
        servo_led_set_state(servo_data.state == SERVO_STATE_RUNNING ? LED_STATE_RUNNING : LED_STATE_STOPPED);
        lv_func_goto_layer(&servo_main_layer);
        feed_clock_time();
    }
}

void ui_servo_menu_init(lv_obj_t *parent)
{
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(parent, lv_color_hex(0x1A1A2E), 0);

    lv_obj_t *title = lv_label_create(parent);
    lv_label_set_text(title, "设置");
    lv_obj_set_style_text_font(title, &font_servo_cn_22, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xCCCCCC), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 5);

    int item_h = 40;

    int first_item_top = 120 + (0 - 2) * item_h + 10 - item_h / 2;
    int last_item_bottom = 120 + (VISIBLE_ITEMS - 1 - 2) * item_h + 10 + item_h / 2;

    sep_top = lv_obj_create(parent);
    lv_obj_set_size(sep_top, 220, 2);
    lv_obj_set_style_bg_color(sep_top, lv_color_hex(0x00AAFF), 0);
    lv_obj_set_style_bg_opa(sep_top, LV_OPA_COVER, 0);
    lv_obj_set_pos(sep_top, 10, first_item_top);
    lv_obj_move_background(sep_top);

    for (int i = 0; i < VISIBLE_ITEMS; i++) {
        menu_containers[i] = lv_obj_create(parent);
        lv_obj_set_size(menu_containers[i], 220, item_h);
        lv_obj_align(menu_containers[i], LV_ALIGN_CENTER, 0, (i - 2) * item_h + 10);
        lv_obj_set_style_bg_color(menu_containers[i], lv_color_hex(0x1A1A2E), 0);
        lv_obj_set_style_bg_opa(menu_containers[i], LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_color(menu_containers[i], lv_color_hex(0x00AAFF), 0);
        lv_obj_set_style_border_width(menu_containers[i], 0, 0);
        lv_obj_set_style_radius(menu_containers[i], 6, 0);
        lv_obj_set_style_pad_all(menu_containers[i], 4, 0);
        lv_obj_clear_flag(menu_containers[i], LV_OBJ_FLAG_SCROLLABLE);
        
        menu_labels[i] = lv_label_create(menu_containers[i]);
        lv_obj_set_style_text_font(menu_labels[i], &font_servo_cn_22, 0);
        lv_obj_set_style_text_color(menu_labels[i], lv_color_hex(0x666666), 0);
        lv_obj_align(menu_labels[i], LV_ALIGN_LEFT_MID, 0, 0);
        lv_label_set_text(menu_labels[i], "");
    }

    sep_bottom = lv_obj_create(parent);
    lv_obj_set_size(sep_bottom, 220, 2);
    lv_obj_set_style_bg_color(sep_bottom, lv_color_hex(0x00AAFF), 0);
    lv_obj_set_style_bg_opa(sep_bottom, LV_OPA_COVER, 0);
    lv_obj_set_pos(sep_bottom, 10, last_item_bottom - 2);
    lv_obj_move_background(sep_bottom);

    menu_update_display();

    lv_obj_add_event_cb(parent, servo_menu_event_cb, LV_EVENT_FOCUSED, NULL);
    lv_obj_add_event_cb(parent, servo_menu_event_cb, LV_EVENT_KEY, NULL);
    lv_obj_add_event_cb(parent, servo_menu_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(parent, servo_menu_event_cb, LV_EVENT_LONG_PRESSED, NULL);
    ui_add_obj_to_encoder_group(parent);
}

static bool servo_menu_enter_cb(void *layer)
{
    bool ret = false;
    lv_layer_t *create_layer = layer;

    if (NULL == create_layer->lv_obj_layer) {
        ret = true;
        create_layer->lv_obj_layer = lv_obj_create(lv_scr_act());
        lv_obj_remove_style_all(create_layer->lv_obj_layer);
        lv_obj_set_size(create_layer->lv_obj_layer, LV_HOR_RES, LV_VER_RES);

        ui_servo_menu_init(create_layer->lv_obj_layer);
    }
    menu_index = 0;
    editing = false;
    set_time_out(&time_200ms, 200);
    feed_clock_time();

    return ret;
}

static bool servo_menu_exit_cb(void *layer)
{
    return true;
}

static void servo_menu_timer_cb(lv_timer_t *tmr)
{
}
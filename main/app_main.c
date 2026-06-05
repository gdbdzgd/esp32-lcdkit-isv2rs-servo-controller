/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_log.h"

#include "lv_example_pub.h"
#include "bsp/esp-bsp.h"
#include "esp_lvgl_port.h"
#include "isv2rs6040.h"
#include "servo_config.h"
#include "ui_servo.h"
#include "servo_led.h"
#include "servo_audio.h"

static const char *TAG = "main";

#define SERVO_UART_NUM      UART_NUM_1

#define EVT_SAVE_CONFIG    (1 << 0)
#define EVT_RESET_ALARM    (1 << 1)
#define EVT_POSITION_MOVE  (1 << 2)
#define EVT_REINIT_SERVO   (1 << 3)

static isv2_handle_t servo_handle;
static bool servo_initialized = false;
static servo_config_t saved_config;
static EventGroupHandle_t servo_evt_group = NULL;
static SemaphoreHandle_t servo_mutex = NULL;
static bool handwheel_active = false;

int32_t position_target = 0;
static int32_t position_increment = 0;

void servo_handwheel_trigger(bool forward)
{
    handwheel_active = true;
    if (servo_initialized) {
        uint16_t jog_speed = servo_data.position_speed > 0 ? servo_data.position_speed : 100;
        isv2_set_jog_speed(&servo_handle, jog_speed);
        vTaskDelay(pdMS_TO_TICKS(5));
        if (forward) {
            isv2_jog_forward(&servo_handle);
        } else {
            isv2_jog_backward(&servo_handle);
        }
    }
}

void servo_set_position_increment(int32_t inc)
{
    position_increment = inc;
    ESP_LOGI(TAG, "Position increment set: %ld", (long)inc);
}

void servo_reset_alarm(void)
{
    if (servo_evt_group) {
        xEventGroupSetBits(servo_evt_group, EVT_RESET_ALARM);
    }
}

void servo_position_move(int16_t delta)
{
    if (servo_data.mode == SERVO_MODE_POSITION && servo_data.state == SERVO_STATE_RUNNING) {
        handwheel_active = true;
        if (servo_mutex) xSemaphoreTake(servo_mutex, portMAX_DELAY);
        position_target += delta * servo_data.pulse_ratio;
        if (servo_mutex) xSemaphoreGive(servo_mutex);
        
        if (servo_evt_group) {
            xEventGroupSetBits(servo_evt_group, EVT_POSITION_MOVE);
        }
    }
}

void servo_handwheel_stop(void)
{
    handwheel_active = false;
    if (servo_initialized) {
        isv2_jog_stop(&servo_handle);
    }
}

static void servo_reinit(void)
{
    if (servo_initialized) {
        isv2_deinit(&servo_handle);
        servo_initialized = false;
    }

    isv2_config_t config = {
        .slave_id = servo_data.modbus_addr,
        .baudrate = servo_data.baudrate,
        .uart_num = SERVO_UART_NUM,
        .tx_pin = ISV2_TX_PIN,
        .rx_pin = ISV2_RX_PIN,
    };

    esp_err_t ret = isv2_init(&servo_handle, &config);
    if (ret == ESP_OK) {
        servo_initialized = true;
        ESP_LOGI(TAG, "Servo reinitialized: addr=%d, baud=%lu",
                 servo_data.modbus_addr, (unsigned long)servo_data.baudrate);
    } else {
        ESP_LOGE(TAG, "Servo reinit failed: %s", esp_err_to_name(ret));
    }
}

void servo_request_reinit(void)
{
    if (servo_evt_group) {
        xEventGroupSetBits(servo_evt_group, EVT_REINIT_SERVO);
    }
}

void servo_menu_request_save(void)
{
    if (servo_evt_group) {
        xEventGroupSetBits(servo_evt_group, EVT_SAVE_CONFIG);
    }
}

esp_err_t bsp_board_init(void)
{
    ESP_ERROR_CHECK(bsp_led_init());
    return ESP_OK;
}

static esp_err_t servo_init(void)
{
    isv2_config_t config = {
        .slave_id = saved_config.modbus_addr,
        .baudrate = saved_config.baudrate,
        .uart_num = SERVO_UART_NUM,
        .tx_pin = ISV2_TX_PIN,
        .rx_pin = ISV2_RX_PIN,
    };

    esp_err_t ret = isv2_init(&servo_handle, &config);
    if (ret == ESP_OK) {
        servo_initialized = true;
        ESP_LOGI(TAG, "Servo initialized: addr=%d, baud=%lu",
                 saved_config.modbus_addr, (unsigned long)saved_config.baudrate);
    } else {
        ESP_LOGE(TAG, "Servo init failed: %s", esp_err_to_name(ret));
    }
    return ret;
}

static void servo_control_task(void *arg)
{
    ESP_LOGI(TAG, "Servo control task started");

    bool last_enabled = false;
    servo_mode_e last_mode = servo_data.mode;

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(50));

        if (!servo_initialized) {
            continue;
        }

        if (servo_data.mode != last_mode) {
            ESP_LOGI(TAG, "Mode changed: %d -> %d, resetting last_enabled", last_mode, servo_data.mode);
            last_enabled = false;
            last_mode = servo_data.mode;
        }

        if (servo_data.enabled && servo_data.state == SERVO_STATE_RUNNING) {
            if (!last_enabled) {
                ESP_LOGI(TAG, "=== Motor ENABLED ===");
                isv2_write_reg16(&servo_handle, 0x1801, 0x1111);
                vTaskDelay(pdMS_TO_TICKS(300));
                ESP_LOGI("MODBUS", ">> Servo enable (Pr4.02=0)");
                isv2_write_reg16(&servo_handle, 0x0405, 0);
                vTaskDelay(pdMS_TO_TICKS(200));
                
                if (servo_data.mode == SERVO_MODE_POSITION) {
                    ESP_LOGI(TAG, "Configuring position mode...");
                    isv2_set_position_mode(&servo_handle);
                    vTaskDelay(pdMS_TO_TICKS(100));
                    ESP_LOGI(TAG, "Rebooting servo for position mode...");
                    isv2_reboot(&servo_handle);
                    vTaskDelay(pdMS_TO_TICKS(500));
                    isv2_wait_ready(&servo_handle, 3000);
                    vTaskDelay(pdMS_TO_TICKS(200));
                    ESP_LOGI("MODBUS", ">> Re-enable after reboot (Pr4.02=0)");
                    isv2_write_reg16(&servo_handle, 0x0405, 0);
                    vTaskDelay(pdMS_TO_TICKS(100));
                    uint16_t ctrl_mode = 0xFFFF;
                    isv2_read_reg16(&servo_handle, ISV2_ADDR_CTRL_MODE, &ctrl_mode);
                    ESP_LOGI(TAG, "After reboot: Pr0.01=%u (expect 0)", ctrl_mode);
                }
                
                if (servo_data.mode == SERVO_MODE_PR) {
                    ESP_LOGI(TAG, "Configuring PR mode...");
                    isv2_set_control_mode(&servo_handle, ISV2_CTRL_MODE_PR);
                    vTaskDelay(pdMS_TO_TICKS(100));
                    ESP_LOGI(TAG, "PR mode: writing Pr0.01=0, then reboot...");
                    isv2_reboot(&servo_handle);
                    vTaskDelay(pdMS_TO_TICKS(500));
                    isv2_wait_ready(&servo_handle, 3000);
                    vTaskDelay(pdMS_TO_TICKS(200));
                    ESP_LOGI("MODBUS", ">> Re-enable after reboot (Pr4.02=0)");
                    isv2_write_reg16(&servo_handle, 0x0405, 0);
                    vTaskDelay(pdMS_TO_TICKS(200));
                    ESP_LOGI("MODBUS", ">> SON enable (0x1801=0x0001)");
                    isv2_write_reg16(&servo_handle, 0x1801, 0x0001);
                    vTaskDelay(pdMS_TO_TICKS(100));
                    uint16_t ctrl_mode = 0xFFFF;
                    isv2_read_reg16(&servo_handle, ISV2_ADDR_CTRL_MODE, &ctrl_mode);
                    ESP_LOGI(TAG, "After reboot: Pr0.01=%u (expect 0)", ctrl_mode);
                }
                
                last_enabled = true;
            }

            if (servo_data.mode == SERVO_MODE_SPEED) {
                isv2_set_jog_speed(&servo_handle, servo_data.target_speed);
                vTaskDelay(pdMS_TO_TICKS(10));

                if (servo_data.direction == SERVO_DIR_FORWARD) {
                    ESP_LOGI(TAG, "JOG forward (direction=%d)", servo_data.direction);
                    isv2_jog_forward(&servo_handle);
                } else {
                    ESP_LOGI(TAG, "JOG backward (direction=%d)", servo_data.direction);
                    isv2_jog_backward(&servo_handle);
                }
            } else if (servo_data.mode == SERVO_MODE_POSITION) {
                if (servo_mutex) xSemaphoreTake(servo_mutex, portMAX_DELAY);
                int32_t inc = position_increment;
                if (inc != 0) {
                    ESP_LOGI(TAG, "Handwheel: move increment %ld", (long)inc);
                    isv2_move_increment(&servo_handle, inc);
                    position_increment = 0;
                }
                if (servo_mutex) xSemaphoreGive(servo_mutex);
            } else if (servo_data.mode == SERVO_MODE_PR) {
                if (servo_mutex) xSemaphoreTake(servo_mutex, portMAX_DELAY);
                int32_t inc = position_increment;
                if (inc != 0) {
                    ESP_LOGI(TAG, "PR mode: relative move %ld pulses", (long)inc);
                    
                    uint16_t jog_speed = servo_data.position_speed > 0 ? servo_data.position_speed : 100;
                    
                    isv2_write_reg16(&servo_handle, ISV2_ADDR_PR1_MODE, 0x0041);
                    vTaskDelay(pdMS_TO_TICKS(5));
                    
                    isv2_write_reg32(&servo_handle, ISV2_ADDR_PR1_POS_H, ISV2_ADDR_PR1_POS_L, inc);
                    vTaskDelay(pdMS_TO_TICKS(5));
                    
                    isv2_write_reg16(&servo_handle, ISV2_ADDR_PR1_SPEED, jog_speed);
                    vTaskDelay(pdMS_TO_TICKS(5));
                    
                    isv2_write_reg16(&servo_handle, ISV2_ADDR_PR1_ACCEL, 100);
                    vTaskDelay(pdMS_TO_TICKS(5));
                    
                    isv2_write_reg16(&servo_handle, ISV2_ADDR_PR1_DECEL, 100);
                    vTaskDelay(pdMS_TO_TICKS(5));
                    
                    ESP_LOGI(TAG, "PR mode: trigger PR0, pos=%ld, speed=%d", (long)inc, jog_speed);
                    isv2_write_reg16(&servo_handle, ISV2_ADDR_CTRL_OP, 0x0010);
                    
                    position_increment = 0;
                }
                if (servo_mutex) xSemaphoreGive(servo_mutex);
            }
        } else {
            if (last_enabled) {
                ESP_LOGI(TAG, "=== Motor DISABLED ===");
                isv2_jog_stop(&servo_handle);
                last_enabled = false;
                servo_data.alarm_code = 0;
                if (lvgl_port_lock(0)) {
                    ui_servo_update_alarm(0);
                    lvgl_port_unlock();
                }
            }
        }
    }
}

static void servo_status_task(void *arg)
{
    ESP_LOGI(TAG, "Servo status task started");

    while (1) {
        EventBits_t bits = xEventGroupWaitBits(servo_evt_group,
            EVT_SAVE_CONFIG | EVT_RESET_ALARM | EVT_POSITION_MOVE | EVT_REINIT_SERVO,
            pdTRUE, pdFALSE, pdMS_TO_TICKS(200));

        if (bits & EVT_SAVE_CONFIG) {
            saved_config.speed_step = servo_data.speed_step;
            saved_config.direction = servo_data.direction;
            saved_config.target_speed = servo_data.target_speed;
            saved_config.modbus_addr = servo_data.modbus_addr;
            saved_config.baudrate = servo_data.baudrate;
            saved_config.mode = servo_data.mode;
            saved_config.pulse_ratio = servo_data.pulse_ratio;
            saved_config.volume = servo_data.volume;
            saved_config.position_speed = servo_data.position_speed;
            servo_config_save(&saved_config);
            ESP_LOGI(TAG, "Config saved");
        }

        if (bits & EVT_RESET_ALARM) {
            if (servo_initialized) {
                isv2_reset_alarm(&servo_handle);
                servo_data.state = SERVO_STATE_STOPPED;
                servo_data.alarm_code = 0;
                if (lvgl_port_lock(0)) {
                    ui_servo_update_state(SERVO_STATE_STOPPED);
                    ui_servo_update_alarm(0);
                    lvgl_port_unlock();
                }
                servo_led_set_state(LED_STATE_STOPPED);
                ESP_LOGI(TAG, "Alarm reset");
            }
        }

        if (bits & EVT_POSITION_MOVE) {
            if (lvgl_port_lock(0)) {
                ui_servo_update_position_display(position_target);
                lvgl_port_unlock();
            }
        }

        if (bits & EVT_REINIT_SERVO) {
            servo_reinit();
        }

        if (!servo_initialized) {
            continue;
        }

        int16_t speed = 0;
        if (isv2_read_speed(&servo_handle, &speed) == ESP_OK) {
            servo_data.actual_speed = speed;
        }

        uint16_t alarm = 0;
        if (isv2_read_alarm(&servo_handle, &alarm) == ESP_OK) {
            if (alarm != 0) {
                isv2_reset_alarm(&servo_handle);
                vTaskDelay(pdMS_TO_TICKS(100));
                
                uint16_t alarm_after = 0;
                if (isv2_read_alarm(&servo_handle, &alarm_after) == ESP_OK) {
                    if (alarm_after != 0) {
                        servo_data.state = SERVO_STATE_ALARM;
                        servo_data.enabled = false;
                        servo_data.alarm_code = alarm_after;
                        if (lvgl_port_lock(0)) {
                            ui_servo_update_state(SERVO_STATE_ALARM);
                            ui_servo_update_alarm(alarm_after);
                            lvgl_port_unlock();
                        }
                        servo_led_set_state(LED_STATE_ALARM);
                    } else {
                        servo_data.alarm_code = 0;
                        servo_data.enabled = true;
                        if (lvgl_port_lock(0)) {
                            ui_servo_update_alarm(0);
                            lvgl_port_unlock();
                        }
                    }
                }
            } else if (servo_data.state == SERVO_STATE_ALARM && alarm == 0) {
                servo_data.state = SERVO_STATE_STOPPED;
                if (lvgl_port_lock(0)) {
                    ui_servo_update_state(SERVO_STATE_STOPPED);
                    lvgl_port_unlock();
                }
                servo_led_set_state(LED_STATE_STOPPED);
            }
        }

        if (servo_data.state == SERVO_STATE_RUNNING) {
            if (lvgl_port_lock(0)) {
                ui_servo_update_direction(servo_data.direction);
                lvgl_port_unlock();
            }
        }
        
        if (servo_data.mode == SERVO_MODE_SPEED) {
            if (lvgl_port_lock(0)) {
                ui_servo_update_speed(servo_data.target_speed);
                lvgl_port_unlock();
            }
        }
        
        if (servo_data.mode == SERVO_MODE_PR) {
            if (lvgl_port_lock(0)) {
                ui_servo_update_speed(servo_data.target_speed);
                lvgl_port_unlock();
            }
        }
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "=== iSV2-RS6040 伺服控制器 ===");
    ESP_LOGI(TAG, "编译时间: %s %s", __DATE__, __TIME__);

    servo_evt_group = xEventGroupCreate();
    servo_mutex = xSemaphoreCreateMutex();

    ESP_ERROR_CHECK(servo_config_init());
    ESP_ERROR_CHECK(servo_config_load(&saved_config));

    servo_data.speed_step = saved_config.speed_step;
    servo_data.direction = saved_config.direction;
    servo_data.target_speed = saved_config.target_speed;
    servo_data.modbus_addr = saved_config.modbus_addr;
    servo_data.baudrate = saved_config.baudrate;
    servo_data.mode = saved_config.mode;
    servo_data.pulse_ratio = saved_config.pulse_ratio;
    servo_data.volume = saved_config.volume;
    servo_data.position_speed = saved_config.position_speed;

    ui_servo_set_volume(servo_data.volume);

    bsp_display_cfg_t cfg = {
        .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
        .buffer_size = 240 * 10,
        .double_buffer = false,
        .flags.buff_dma = true,
    };
    bsp_display_start_with_config(&cfg);

    ESP_LOGI(TAG, "显示伺服控制界面");
    ui_obj_to_encoder_init();
    lv_create_home(&servo_main_layer);
    bsp_display_unlock();

    vTaskDelay(pdMS_TO_TICKS(500));
    bsp_display_backlight_on();

    bsp_board_init();

    ESP_LOGI(TAG, "初始化音频...");
    servo_audio_init();

    ESP_LOGI(TAG, "初始化LED...");
    servo_led_init();
    servo_led_set_state(LED_STATE_STOPPED);

    ESP_LOGI(TAG, "初始化伺服通信...");
    servo_init();

    xTaskCreate(servo_control_task, "servo_ctrl", 4096, NULL, 5, NULL);
    xTaskCreate(servo_status_task, "servo_status", 4096, NULL, 4, NULL);

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "=== 系统就绪 ===");
    ESP_LOGI(TAG, "编码器旋转: 调节目标转速");
    ESP_LOGI(TAG, "短按按键: 启动/停止");
    ESP_LOGI(TAG, "长按按键: 进入设置菜单");
    ESP_LOGI(TAG, "Modbus: TX=GPIO%d, RX=GPIO%d", ISV2_TX_PIN, ISV2_RX_PIN);
}
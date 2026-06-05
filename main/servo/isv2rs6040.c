#include "isv2rs6040.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "mbcontroller.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "MODBUS";

static void print_modbus_request(const char *desc, uint8_t slave, uint8_t func, uint16_t addr, uint16_t value_or_count)
{
    ESP_LOGI(TAG, "TX [%s]: %02X %02X %04X %04X", 
             desc, slave, func, addr, value_or_count);
}

static void print_modbus_response(uint8_t func, uint16_t value)
{
    ESP_LOGI(TAG, "RX [0x%02X]: %04X", func, value);
}

static void *mb_handle = NULL;

esp_err_t isv2_init(isv2_handle_t *handle, const isv2_config_t *config)
{
    if (handle == NULL || config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    handle->config = *config;
    
    mb_communication_info_t comm_info = {
        .port = config->uart_num,
        .mode = MB_MODE_RTU,
        .baudrate = config->baudrate,
        .parity = UART_PARITY_DISABLE
    };
    
    esp_err_t ret = mbc_master_init(MB_PORT_SERIAL_MASTER, &mb_handle);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Modbus init failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = mbc_master_setup((void *)&comm_info);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Modbus setup failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    uart_config_t uart_config = {
        .baud_rate = config->baudrate,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    uart_param_config(config->uart_num, &uart_config);
    uart_set_pin(config->uart_num, config->tx_pin, config->rx_pin, 
                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    
    ret = mbc_master_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Modbus start failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    handle->initialized = true;
    ESP_LOGI(TAG, "ISV2RS6040 initialized: baud=%lu, slave=%d, tx=%d, rx=%d",
             config->baudrate, config->slave_id, config->tx_pin, config->rx_pin);
    
    return ESP_OK;
}

esp_err_t isv2_deinit(isv2_handle_t *handle)
{
    if (handle == NULL || !handle->initialized) {
        return ESP_ERR_INVALID_ARG;
    }
    
    mbc_master_destroy();
    mb_handle = NULL;
    handle->initialized = false;
    return ESP_OK;
}

esp_err_t isv2_write_reg16(isv2_handle_t *handle, uint16_t addr, uint16_t value)
{
    if (handle == NULL || !handle->initialized) {
        return ESP_ERR_INVALID_ARG;
    }
    
    print_modbus_request("Write", handle->config.slave_id, 0x06, addr, value);
    
    uint8_t data[4];
    mb_param_request_t request = {
        .slave_addr = handle->config.slave_id,
        .command = 6,
        .reg_start = addr,
        .reg_size = 1
    };
    data[0] = value & 0xFF;
    data[1] = (value >> 8) & 0xFF;
    
    esp_err_t err = mbc_master_send_request(&request, data);
    if (err == ESP_OK) {
        print_modbus_response(0x06, value);
    } else {
        ESP_LOGE(TAG, "RX ERROR: %s", esp_err_to_name(err));
    }
    return err;
}

esp_err_t isv2_read_reg16(isv2_handle_t *handle, uint16_t addr, uint16_t *value)
{
    if (handle == NULL || !handle->initialized || value == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    print_modbus_request("Read", handle->config.slave_id, 0x03, addr, 1);
    
    uint8_t data[4] = {0};
    mb_param_request_t request = {
        .slave_addr = handle->config.slave_id,
        .command = 3,
        .reg_start = addr,
        .reg_size = 1
    };
    
    esp_err_t err = mbc_master_send_request(&request, data);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "RX ERROR: %s", esp_err_to_name(err));
        return err;
    }
    
    *value = (data[0] << 8) | data[1];
    print_modbus_response(0x03, *value);
    return ESP_OK;
}

esp_err_t isv2_write_reg32(isv2_handle_t *handle, uint16_t addr_h, uint16_t addr_l, int32_t value)
{
    esp_err_t ret;
    ret = isv2_write_reg16(handle, addr_h, (value >> 16) & 0xFFFF);
    if (ret != ESP_OK) return ret;
    vTaskDelay(pdMS_TO_TICKS(10));
    return isv2_write_reg16(handle, addr_l, value & 0xFFFF);
}

esp_err_t isv2_read_reg32(isv2_handle_t *handle, uint16_t addr_h, uint16_t addr_l, int32_t *value)
{
    uint16_t h, l;
    esp_err_t ret;
    
    ret = isv2_read_reg16(handle, addr_h, &h);
    if (ret != ESP_OK) return ret;
    
    ret = isv2_read_reg16(handle, addr_l, &l);
    if (ret != ESP_OK) return ret;
    
    *value = ((int32_t)h << 16) | l;
    return ESP_OK;
}

esp_err_t isv2_jog_forward(isv2_handle_t *handle)
{
    ESP_LOGI(TAG, ">> JOG forward");
    return isv2_write_reg16(handle, ISV2_ADDR_CTRL_WORD, ISV2_AUX_JOG_N);
}

esp_err_t isv2_jog_backward(isv2_handle_t *handle)
{
    ESP_LOGI(TAG, ">> JOG reverse");
    return isv2_write_reg16(handle, ISV2_ADDR_CTRL_WORD, ISV2_AUX_JOG_P);
}

esp_err_t isv2_jog_stop(isv2_handle_t *handle)
{
    ESP_LOGI(TAG, ">> JOG stop");
    return isv2_write_reg16(handle, ISV2_ADDR_CTRL_WORD, 0);
}

esp_err_t isv2_set_jog_speed(isv2_handle_t *handle, uint16_t speed_rpm)
{
    ESP_LOGI(TAG, ">> Set JOG speed: %d rpm", speed_rpm);
    return isv2_write_reg16(handle, 0x0609, speed_rpm);
}

esp_err_t isv2_set_jog_accel(isv2_handle_t *handle, uint16_t accel_ms)
{
    return isv2_write_reg16(handle, ISV2_ADDR_JOG_ACCEL, accel_ms);
}

esp_err_t isv2_set_jog_decel(isv2_handle_t *handle, uint16_t decel_ms)
{
    return isv2_write_reg16(handle, ISV2_ADDR_JOG_DECEL, decel_ms);
}

esp_err_t isv2_set_position(isv2_handle_t *handle, int32_t position)
{
    return isv2_write_reg32(handle, ISV2_ADDR_CMD_POS_H, ISV2_ADDR_CMD_POS_L, position);
}

esp_err_t isv2_get_position(isv2_handle_t *handle, int32_t *position)
{
    return isv2_read_reg32(handle, ISV2_ADDR_MOTOR_POS_H, ISV2_ADDR_MOTOR_POS_L, position);
}

esp_err_t isv2_set_pr1_path(isv2_handle_t *handle, int32_t position, uint16_t speed,
                            uint16_t accel, uint16_t decel)
{
    esp_err_t ret;
    
    ret = isv2_write_reg16(handle, ISV2_ADDR_PR1_MODE, 1);
    if (ret != ESP_OK) return ret;
    vTaskDelay(pdMS_TO_TICKS(10));
    
    ret = isv2_write_reg32(handle, ISV2_ADDR_PR1_POS_H, ISV2_ADDR_PR1_POS_L, position);
    if (ret != ESP_OK) return ret;
    vTaskDelay(pdMS_TO_TICKS(10));
    
    ret = isv2_write_reg16(handle, ISV2_ADDR_PR1_SPEED, speed);
    if (ret != ESP_OK) return ret;
    vTaskDelay(pdMS_TO_TICKS(10));
    
    ret = isv2_write_reg16(handle, ISV2_ADDR_PR1_ACCEL, accel);
    if (ret != ESP_OK) return ret;
    vTaskDelay(pdMS_TO_TICKS(10));
    
    return isv2_write_reg16(handle, ISV2_ADDR_PR1_DECEL, decel);
}

esp_err_t isv2_start_pr(isv2_handle_t *handle)
{
    return isv2_write_reg16(handle, ISV2_ADDR_CTRL_OP, 1);
}

esp_err_t isv2_stop_pr(isv2_handle_t *handle)
{
    return isv2_write_reg16(handle, ISV2_ADDR_CTRL_OP, 0);
}

esp_err_t isv2_save_params(isv2_handle_t *handle)
{
    esp_err_t ret = isv2_write_reg16(handle, ISV2_ADDR_CTRL_WORD, ISV2_AUX_SAVE_EE);
    if (ret != ESP_OK) return ret;
    
    vTaskDelay(pdMS_TO_TICKS(100));
    
    uint16_t status;
    ret = isv2_read_reg16(handle, ISV2_ADDR_STATUS_WORD, &status);
    if (ret != ESP_OK) return ret;
    
    if (status == ISV2_STATUS_SAVE_OK) {
        ESP_LOGI(TAG, "Parameters saved successfully");
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "Save failed");
        return ESP_FAIL;
    }
}

esp_err_t isv2_reset_alarm(isv2_handle_t *handle)
{
    ESP_LOGI(TAG, ">> Clear alarm");
    return isv2_write_reg16(handle, ISV2_ADDR_CTRL_WORD, ISV2_AUX_RESET_ALARM);
}

esp_err_t isv2_read_alarm(isv2_handle_t *handle, uint16_t *alarm_code)
{
    return isv2_read_reg16(handle, ISV2_ADDR_ALARM, alarm_code);
}

esp_err_t isv2_set_control_mode(isv2_handle_t *handle, uint16_t mode)
{
    return isv2_write_reg16(handle, ISV2_ADDR_CTRL_MODE, mode);
}

esp_err_t isv2_set_target_speed(isv2_handle_t *handle, int16_t speed_rpm)
{
    return isv2_write_reg16(handle, ISV2_ADDR_TARGET_SPEED, (uint16_t)speed_rpm);
}

esp_err_t isv2_run_speed(isv2_handle_t *handle)
{
    return isv2_write_reg16(handle, ISV2_ADDR_CTRL_WORD, 0x0001);
}

esp_err_t isv2_stop(isv2_handle_t *handle)
{
    return isv2_write_reg16(handle, ISV2_ADDR_CTRL_WORD, 0x0000);
}

esp_err_t isv2_read_status_word(isv2_handle_t *handle, uint16_t *status)
{
    return isv2_read_reg16(handle, ISV2_ADDR_STATUS_WORD, status);
}

esp_err_t isv2_read_speed(isv2_handle_t *handle, int16_t *speed_rpm)
{
    uint16_t value;
    esp_err_t ret = isv2_read_reg16(handle, ISV2_ADDR_MOTOR_SPEED, &value);
    if (ret == ESP_OK) {
        *speed_rpm = (int16_t)value;
    }
    return ret;
}

esp_err_t isv2_read_torque(isv2_handle_t *handle, int16_t *torque_percent)
{
    uint16_t value;
    esp_err_t ret = isv2_read_reg16(handle, ISV2_ADDR_MOTOR_TORQUE, &value);
    if (ret == ESP_OK) {
        *torque_percent = (int16_t)value;
    }
    return ret;
}

esp_err_t isv2_read_status(isv2_handle_t *handle, isv2_status_t *status)
{
    if (handle == NULL || !handle->initialized || status == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    memset(status, 0, sizeof(isv2_status_t));
    
    esp_err_t ret = isv2_read_status_word(handle, &status->status_word);
    if (ret != ESP_OK) return ret;
    
    status->ready = (status->status_word & ISV2_STATUS_READY) != 0;
    status->enabled = (status->status_word & ISV2_STATUS_ENABLED) != 0;
    status->running = (status->status_word & ISV2_STATUS_RUNNING) != 0;
    status->forward = (status->status_word & ISV2_STATUS_FORWARD) != 0;
    status->has_alarm = (status->status_word & ISV2_STATUS_ALARM) != 0;
    status->motion_complete = (status->status_word & ISV2_STATUS_COMPLETE) != 0;
    
    isv2_read_alarm(handle, &status->alarm_code);
    isv2_read_speed(handle, &status->speed_rpm);
    isv2_read_torque(handle, &status->torque_percent);
    isv2_get_position(handle, &status->position);
    
    return ESP_OK;
}

const char* isv2_alarm_str(uint16_t alarm_code)
{
    switch (alarm_code) {
        case 0:     return "No alarm";
        case 1:     return "Over voltage";
        case 2:     return "Under voltage";
        case 3:     return "Over current";
        case 4:     return "Overload";
        case 5:     return "Overheat";
        case 6:     return "Encoder error";
        case 7:     return "Communication error";
        case 8:     return "Position limit";
        case 9:     return "Speed limit";
        case 10:    return "Torque limit";
        case 11:    return "Motor error";
        case 12:    return "Driver error";
        case 13:    return "Sensor error";
        case 14:    return "Memory error";
        case 15:    return "System error";
        default:    return "Unknown alarm";
    }
}

esp_err_t isv2_set_position_mode(isv2_handle_t *handle)
{
    ESP_LOGI(TAG, ">> Set position mode (Pr0.01=0)");
    esp_err_t ret = isv2_write_reg16(handle, ISV2_ADDR_CTRL_MODE, 0);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Position mode set (requires power cycle)");
    }
    return ret;
}

esp_err_t isv2_reboot(isv2_handle_t *handle)
{
    ESP_LOGI(TAG, ">> Servo reboot (0x100A=0x0001)");
    return isv2_write_reg16(handle, ISV2_ADDR_REBOOT, 0x0001);
}

esp_err_t isv2_wait_ready(isv2_handle_t *handle, uint32_t timeout_ms)
{
    uint32_t elapsed = 0;
    uint16_t status = 0;
    
    while (elapsed < timeout_ms) {
        esp_err_t ret = isv2_read_reg16(handle, ISV2_ADDR_STATUS_WORD, &status);
        if (ret == ESP_OK && status != 0xFFFF) {
            ESP_LOGI(TAG, "Servo ready after %lu ms", (unsigned long)elapsed);
            return ESP_OK;
        }
        vTaskDelay(pdMS_TO_TICKS(50));
        elapsed += 50;
    }
    
    ESP_LOGE(TAG, "Servo not ready after %lu ms timeout", (unsigned long)timeout_ms);
    return ESP_ERR_TIMEOUT;
}

esp_err_t isv2_move_increment(isv2_handle_t *handle, int32_t increment)
{
    esp_err_t ret;
    
    ret = isv2_write_reg16(handle, ISV2_ADDR_PR1_MODE, 0x0041);
    if (ret != ESP_OK) return ret;
    vTaskDelay(pdMS_TO_TICKS(5));
    
    ret = isv2_write_reg32(handle, ISV2_ADDR_PR1_POS_H, ISV2_ADDR_PR1_POS_L, increment);
    if (ret != ESP_OK) return ret;
    vTaskDelay(pdMS_TO_TICKS(5));
    
    ret = isv2_write_reg16(handle, ISV2_ADDR_PR1_SPEED, 1000);
    if (ret != ESP_OK) return ret;
    vTaskDelay(pdMS_TO_TICKS(5));
    
    ret = isv2_write_reg16(handle, ISV2_ADDR_PR1_ACCEL, 100);
    if (ret != ESP_OK) return ret;
    vTaskDelay(pdMS_TO_TICKS(5));
    
    ret = isv2_write_reg16(handle, ISV2_ADDR_PR1_DECEL, 100);
    if (ret != ESP_OK) return ret;
    vTaskDelay(pdMS_TO_TICKS(5));
    
    ret = isv2_write_reg16(handle, ISV2_ADDR_PR1_DWELL, 0);
    if (ret != ESP_OK) return ret;
    vTaskDelay(pdMS_TO_TICKS(5));
    
    ret = isv2_write_reg16(handle, 0x6207, 0x0010);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, ">> PR0 triggered, increment: %ld", (long)increment);
    }
    return ret;
}
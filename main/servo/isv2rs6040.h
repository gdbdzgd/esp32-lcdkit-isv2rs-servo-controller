#ifndef ISV2RS6040_H
#define ISV2RS6040_H

#include <stdint.h>
#include <stdbool.h>
#include "driver/uart.h"

#define ISV2_SLAVE_ID_DEFAULT   1
#define ISV2_BAUDRATE_DEFAULT   9600

#define ISV2_TX_PIN             21
#define ISV2_RX_PIN             20

#define ISV2_ADDR_CTRL_MODE     0x0003
#define ISV2_ADDR_AUX_FUNC      0x0033
#define ISV2_ADDR_TARGET_SPEED  0x0005
#define ISV2_ADDR_SPEED_LIMIT   0x0006
#define ISV2_ADDR_TORQUE_LIMIT  0x0007
#define ISV2_ADDR_MOTOR_SPEED   0x1001
#define ISV2_ADDR_MOTOR_TORQUE  0x1002
#define ISV2_ADDR_JOG_SPEED     0x0609
#define ISV2_ADDR_JOG_ACCEL     0x6028
#define ISV2_ADDR_JOG_DECEL     0x6029
#define ISV2_ADDR_CMD_POS_H     0x602A
#define ISV2_ADDR_CMD_POS_L     0x602B
#define ISV2_ADDR_MOTOR_POS_H   0x602C
#define ISV2_ADDR_MOTOR_POS_L   0x602D
#define ISV2_ADDR_CTRL_WORD     0x1801
#define ISV2_ADDR_STATUS_WORD   0x1901
#define ISV2_ADDR_ALARM         0x2203

#define ISV2_ADDR_REBOOT        0x100A

#define ISV2_ADDR_PR_CTRL_SET   0x6000
#define ISV2_ADDR_PATH_NUM      0x6001
#define ISV2_ADDR_CTRL_OP       0x6002
#define ISV2_ADDR_PR1_MODE      0x6200
#define ISV2_ADDR_PR1_POS_H     0x6201
#define ISV2_ADDR_PR1_POS_L     0x6202
#define ISV2_ADDR_PR1_SPEED     0x6203
#define ISV2_ADDR_PR1_ACCEL     0x6204
#define ISV2_ADDR_PR1_DECEL     0x6205
#define ISV2_ADDR_PR1_DWELL     0x6206

#define ISV2_AUX_JOG_P          0x4001
#define ISV2_AUX_JOG_N          0x4002
#define ISV2_AUX_SAVE_EE        0x2211
#define ISV2_AUX_RESET_ALARM    0x1111
#define ISV2_AUX_INIT_PARAM     0x2222

#define ISV2_CTRL_MODE_PR       0
#define ISV2_CTRL_MODE_SPEED    1
#define ISV2_CTRL_MODE_TORQUE   2

#define ISV2_STATUS_READY      (1 << 0)
#define ISV2_STATUS_ENABLED    (1 << 1)
#define ISV2_STATUS_RUNNING    (1 << 2)
#define ISV2_STATUS_FORWARD    (1 << 3)
#define ISV2_STATUS_ALARM      (1 << 4)
#define ISV2_STATUS_COMPLETE   (1 << 5)

#define ISV2_STATUS_SAVE_OK     0x5555
#define ISV2_STATUS_SAVE_FAIL   0xAAAA

typedef struct {
    uint8_t slave_id;
    uint32_t baudrate;
    uart_port_t uart_num;
    int tx_pin;
    int rx_pin;
} isv2_config_t;

typedef struct {
    isv2_config_t config;
    bool initialized;
} isv2_handle_t;

typedef struct {
    uint16_t status_word;
    uint16_t alarm_code;
    int32_t position;
    int16_t speed_rpm;
    int16_t torque_percent;
    bool ready;
    bool enabled;
    bool running;
    bool forward;
    bool has_alarm;
    bool motion_complete;
} isv2_status_t;

esp_err_t isv2_init(isv2_handle_t *handle, const isv2_config_t *config);
esp_err_t isv2_deinit(isv2_handle_t *handle);

esp_err_t isv2_write_reg16(isv2_handle_t *handle, uint16_t addr, uint16_t value);
esp_err_t isv2_write_reg32(isv2_handle_t *handle, uint16_t addr_h, uint16_t addr_l, int32_t value);
esp_err_t isv2_read_reg16(isv2_handle_t *handle, uint16_t addr, uint16_t *value);
esp_err_t isv2_read_reg32(isv2_handle_t *handle, uint16_t addr_h, uint16_t addr_l, int32_t *value);

esp_err_t isv2_jog_forward(isv2_handle_t *handle);
esp_err_t isv2_jog_backward(isv2_handle_t *handle);
esp_err_t isv2_jog_stop(isv2_handle_t *handle);

esp_err_t isv2_set_jog_speed(isv2_handle_t *handle, uint16_t speed_rpm);
esp_err_t isv2_set_jog_accel(isv2_handle_t *handle, uint16_t accel_ms);
esp_err_t isv2_set_jog_decel(isv2_handle_t *handle, uint16_t decel_ms);

esp_err_t isv2_set_position(isv2_handle_t *handle, int32_t position);
esp_err_t isv2_get_position(isv2_handle_t *handle, int32_t *position);

esp_err_t isv2_set_pr1_path(isv2_handle_t *handle, int32_t position, uint16_t speed, 
                            uint16_t accel, uint16_t decel);
esp_err_t isv2_start_pr(isv2_handle_t *handle);
esp_err_t isv2_stop_pr(isv2_handle_t *handle);

esp_err_t isv2_save_params(isv2_handle_t *handle);
esp_err_t isv2_reset_alarm(isv2_handle_t *handle);

esp_err_t isv2_read_alarm(isv2_handle_t *handle, uint16_t *alarm_code);

esp_err_t isv2_set_control_mode(isv2_handle_t *handle, uint16_t mode);
esp_err_t isv2_set_target_speed(isv2_handle_t *handle, int16_t speed_rpm);
esp_err_t isv2_run_speed(isv2_handle_t *handle);
esp_err_t isv2_stop(isv2_handle_t *handle);

esp_err_t isv2_read_status(isv2_handle_t *handle, isv2_status_t *status);
esp_err_t isv2_read_status_word(isv2_handle_t *handle, uint16_t *status);
esp_err_t isv2_read_speed(isv2_handle_t *handle, int16_t *speed_rpm);
esp_err_t isv2_read_torque(isv2_handle_t *handle, int16_t *torque_percent);

esp_err_t isv2_reboot(isv2_handle_t *handle);
esp_err_t isv2_wait_ready(isv2_handle_t *handle, uint32_t timeout_ms);

esp_err_t isv2_set_position_mode(isv2_handle_t *handle);
esp_err_t isv2_move_increment(isv2_handle_t *handle, int32_t increment);

const char* isv2_alarm_str(uint16_t alarm_code);

#endif
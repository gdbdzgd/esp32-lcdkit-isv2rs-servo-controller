#include "servo_config.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"

static const char *TAG = "servo_cfg";

esp_err_t servo_config_init(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition truncated, erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    return ret;
}

esp_err_t servo_config_load(servo_config_t *config)
{
    if (config == NULL) return ESP_ERR_INVALID_ARG;

    config->speed_step = SERVO_DEFAULT_SPEED_STEP;
    config->direction = SERVO_DEFAULT_DIRECTION;
    config->target_speed = SERVO_DEFAULT_TARGET_SPEED;
    config->modbus_addr = SERVO_DEFAULT_MODBUS_ADDR;
    config->baudrate = SERVO_DEFAULT_BAUDRATE;
    config->mode = SERVO_DEFAULT_MODE;
    config->pulse_ratio = SERVO_DEFAULT_PULSE_RATIO;
    config->volume = SERVO_DEFAULT_VOLUME;
    config->position_speed = SERVO_DEFAULT_POS_SPEED;

    nvs_handle_t handle;
    esp_err_t err = nvs_open(SERVO_NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "No saved config, using defaults");
        return ESP_OK;
    }

    nvs_get_u16(handle, "speed_step", &config->speed_step);
    nvs_get_u8(handle, "direction", &config->direction);
    nvs_get_u16(handle, "target_spd", &config->target_speed);
    nvs_get_u8(handle, "mbus_addr", &config->modbus_addr);
    nvs_get_u32(handle, "baudrate", &config->baudrate);
    nvs_get_u8(handle, "mode", &config->mode);
    nvs_get_u16(handle, "pulse_ratio", &config->pulse_ratio);
    nvs_get_u8(handle, "volume", &config->volume);
    nvs_get_u16(handle, "pos_speed", &config->position_speed);

    nvs_close(handle);
    ESP_LOGI(TAG, "Config loaded: vol=%d", config->volume);
    return ESP_OK;
}

esp_err_t servo_config_save(const servo_config_t *config)
{
    if (config == NULL) return ESP_ERR_INVALID_ARG;

    nvs_handle_t handle;
    esp_err_t err = nvs_open(SERVO_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) return err;

    nvs_set_u16(handle, "speed_step", config->speed_step);
    nvs_set_u8(handle, "direction", config->direction);
    nvs_set_u16(handle, "target_spd", config->target_speed);
    nvs_set_u8(handle, "mbus_addr", config->modbus_addr);
    nvs_set_u32(handle, "baudrate", config->baudrate);
    nvs_set_u8(handle, "mode", config->mode);
    nvs_set_u16(handle, "pulse_ratio", config->pulse_ratio);
    nvs_set_u8(handle, "volume", config->volume);
    nvs_set_u16(handle, "pos_speed", config->position_speed);

    err = nvs_commit(handle);
    nvs_close(handle);
    ESP_LOGI(TAG, "Config saved");
    return err;
}

esp_err_t servo_config_reset(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(SERVO_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) return ESP_OK;

    nvs_erase_all(handle);
    nvs_commit(handle);
    nvs_close(handle);
    ESP_LOGI(TAG, "Config reset to factory defaults");
    return ESP_OK;
}
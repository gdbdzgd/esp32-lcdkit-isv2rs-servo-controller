#include "servo_led.h"
#include "esp_log.h"
#include "led_strip.h"
#include "driver/gpio.h"

static const char *TAG = "servo_led";
static led_strip_handle_t led_strip = NULL;
static led_state_e current_state = LED_STATE_STOPPED;

#define WS2812_GPIO 8

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} led_color_t;

static const led_color_t color_stopped = {50, 0, 0};
static const led_color_t color_running = {0, 50, 0};
static const led_color_t color_alarm = {50, 0, 0};
static const led_color_t color_menu = {0, 0, 50};

esp_err_t servo_led_init(void)
{
    led_strip_config_t strip_config = {
        .strip_gpio_num = WS2812_GPIO,
        .max_leds = 1,
        .led_pixel_format = LED_PIXEL_FORMAT_GRB,
        .led_model = LED_MODEL_WS2812,
        .flags.invert_out = false,
    };

    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,
    };

    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    ESP_LOGI(TAG, "LED initialized on GPIO%d", WS2812_GPIO);
    
    led_strip_clear(led_strip);
    return ESP_OK;
}

void servo_led_set_state(led_state_e state)
{
    current_state = state;
    led_color_t color;
    
    switch (state) {
        case LED_STATE_RUNNING:
            color = color_running;
            break;
        case LED_STATE_ALARM:
            color = color_alarm;
            break;
        case LED_STATE_MENU:
            color = color_menu;
            break;
        default:
            color = color_stopped;
            break;
    }
    
    if (led_strip) {
        led_strip_set_pixel(led_strip, 0, color.r, color.g, color.b);
        led_strip_refresh(led_strip);
    }
}

void servo_led_set_color(uint8_t r, uint8_t g, uint8_t b)
{
    if (led_strip) {
        led_strip_set_pixel(led_strip, 0, r, g, b);
        led_strip_refresh(led_strip);
    }
}

void servo_led_off(void)
{
    if (led_strip) {
        led_strip_clear(led_strip);
    }
}
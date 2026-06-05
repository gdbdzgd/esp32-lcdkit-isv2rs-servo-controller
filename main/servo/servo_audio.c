#include "servo_audio.h"
#include "esp_log.h"
#include "bsp/esp-bsp.h"
#include "esp_codec_dev.h"
#include "driver/i2s_pdm.h"
#include <string.h>

static const char *TAG = "servo_audio";
static esp_codec_dev_handle_t spk_codec = NULL;
static bool audio_initialized = false;
static int8_t audio_volume = 80;

#define BEEP_SAMPLE_RATE     16000
#define BEEP_DURATION_MS     60
#define BEEP_SAMPLES         (BEEP_SAMPLE_RATE * BEEP_DURATION_MS / 1000)
#define ROTATE_FREQ          1200
#define CLICK_FREQ           1200
#define LONG_FREQ            1000
#define BEEP_AMPLITUDE       16000

static int16_t beep_buffer_click[BEEP_SAMPLES];
static int16_t beep_buffer_long[BEEP_SAMPLES * 2];
static int16_t beep_buffer_rotate[BEEP_SAMPLES / 2];
static int16_t output_buffer[BEEP_SAMPLES * 2];

static void generate_beep(int16_t *buffer, int count, int freq, int sample_rate, int amplitude)
{
    int step = freq * 65536 / sample_rate;
    int phase = 0;
    for (int i = 0; i < count; i++) {
        buffer[i] = (phase < 32768) ? amplitude : -amplitude;
        phase += step;
        if (phase >= 65536) phase -= 65536;
    }
}

static void play_with_volume(const int16_t *src, int count)
{
    if (audio_volume <= 0) return;
    
    int scale = audio_volume;
    for (int i = 0; i < count; i++) {
        output_buffer[i] = (int16_t)((int32_t)src[i] * scale / 100);
    }
    esp_codec_dev_write(spk_codec, (uint8_t *)output_buffer, count * sizeof(int16_t));
}

esp_err_t servo_audio_init(void)
{
    if (audio_initialized) {
        return ESP_OK;
    }

    spk_codec = bsp_audio_codec_speaker_init();
    if (spk_codec == NULL) {
        ESP_LOGE(TAG, "Speaker init failed");
        return ESP_FAIL;
    }

    generate_beep(beep_buffer_click, BEEP_SAMPLES, CLICK_FREQ, BEEP_SAMPLE_RATE, BEEP_AMPLITUDE);
    generate_beep(beep_buffer_long, BEEP_SAMPLES, LONG_FREQ, BEEP_SAMPLE_RATE, BEEP_AMPLITUDE);
    generate_beep(beep_buffer_long + BEEP_SAMPLES, BEEP_SAMPLES, LONG_FREQ, BEEP_SAMPLE_RATE, BEEP_AMPLITUDE);
    generate_beep(beep_buffer_rotate, BEEP_SAMPLES / 2, ROTATE_FREQ, BEEP_SAMPLE_RATE, BEEP_AMPLITUDE);

    esp_codec_dev_set_out_vol(spk_codec, audio_volume);

    esp_codec_dev_sample_info_t fs = {
        .sample_rate = BEEP_SAMPLE_RATE,
        .channel = 1,
        .bits_per_sample = 16,
    };
    esp_codec_dev_open(spk_codec, &fs);

    audio_initialized = true;
    ESP_LOGI(TAG, "Audio initialized, volume: %d", audio_volume);
    return ESP_OK;
}

void servo_audio_set_volume(int8_t volume)
{
    if (volume < 0) volume = 0;
    if (volume > 100) volume = 100;
    audio_volume = volume;
    if (spk_codec) {
        esp_codec_dev_set_out_vol(spk_codec, audio_volume);
    }
}

int8_t servo_audio_get_volume(void)
{
    return audio_volume;
}

void servo_audio_play_click(void)
{
    if (!audio_initialized) {
        servo_audio_init();
    }
    if (!spk_codec) return;

    play_with_volume(beep_buffer_click, BEEP_SAMPLES);
}

void servo_audio_play_long(void)
{
    if (!audio_initialized) {
        servo_audio_init();
    }
    if (!spk_codec) return;

    play_with_volume(beep_buffer_long, BEEP_SAMPLES * 2);
}

void servo_audio_play_rotate(void)
{
    if (!audio_initialized) {
        servo_audio_init();
    }
    if (!spk_codec) return;

    play_with_volume(beep_buffer_rotate, BEEP_SAMPLES / 2);
}

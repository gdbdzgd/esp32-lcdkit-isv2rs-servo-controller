#ifndef SERVO_AUDIO_H
#define SERVO_AUDIO_H

#include "esp_err.h"
#include <stdint.h>

esp_err_t servo_audio_init(void);
void servo_audio_set_volume(int8_t volume);
int8_t servo_audio_get_volume(void);
void servo_audio_play_click(void);
void servo_audio_play_long(void);
void servo_audio_play_rotate(void);

#endif
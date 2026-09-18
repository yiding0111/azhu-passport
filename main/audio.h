#pragma once
#include <stdbool.h>

// 音效编号
enum {
    SFX_UP = 0,
    SFX_DOWN,
    SFX_OK,
    SFX_HAPPY,     // 好感度升档时来一串上扬音
};

void audio_init(void);

void audio_set_muted(bool m);   // 存 NVS，断电不丢
bool audio_muted(void);
void audio_toggle_mute(void);

void audio_cycle_volume(void);   // 低/中/高三档循环
int  audio_volume_level(void);   // 0=低 1=中 2=高

void audio_sfx(int which);      // 插播一个短音效（静音时自动忽略）

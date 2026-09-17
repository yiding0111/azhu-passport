#pragma once
#include <stdint.h>

void display_init(void);
void display_set_backlight(int pct);          // 0..100

// 用调色板把一帧 DISP_W*DISP_H 的 8bpp 索引图推到屏（内部行块 DMA）
void display_draw_indexed(const uint8_t *idx, const uint16_t *palette);

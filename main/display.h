#pragma once
#include <stdint.h>

void display_init(void);
void display_set_backlight(int pct);          // 0..100

// 用调色板把一帧 DISP_W*DISP_H 的 8bpp 索引图推到屏（内部行块 DMA）
void display_draw_indexed(const uint8_t *idx, const uint16_t *palette);

// 把一块已按屏字节序(big-endian RGB565)准备好的像素贴到 (x,y)。
// pix 必须在内部 RAM（DMA 可达）。内部会等这块传完再返回。
void display_blit(int x, int y, int w, int h, const uint16_t *pix);

#include "ui.h"
#include "display.h"
#include "board_config.h"

#include <string.h>
#include <stdint.h>

// 状态条像素缓冲（内部 RAM，DMA 可达）
static uint16_t s_bar[DISP_W * UI_STATUS_H];

// 主机 RGB565 -> 屏字节序
#define RGB(r, g, b) ((uint16_t)((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3)))
#define BE(c)        ((uint16_t)((((c) & 0xFF) << 8) | ((c) >> 8)))
#define C(r, g, b)   BE(RGB(r, g, b))

#define COL_BG     C(253, 246, 243)   // 和阿猪背景同色，衔接无缝
#define COL_FG     C(70,  60,  62)
#define COL_HEART  C(226, 106, 128)
#define COL_LOW    C(214, 74,  74)

// 5x7 点阵：0-9 和 '%'，每行低 5 位有效
static const uint8_t FONT[11][7] = {
    {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E}, // 0
    {0x04,0x0C,0x04,0x04,0x04,0x04,0x0E}, // 1
    {0x0E,0x11,0x01,0x02,0x04,0x08,0x1F}, // 2
    {0x1F,0x02,0x04,0x02,0x01,0x11,0x0E}, // 3
    {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02}, // 4
    {0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E}, // 5
    {0x06,0x08,0x10,0x1E,0x11,0x11,0x0E}, // 6
    {0x1F,0x01,0x02,0x04,0x08,0x08,0x08}, // 7
    {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E}, // 8
    {0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C}, // 9
    {0x19,0x1A,0x02,0x04,0x08,0x0B,0x13}, // %
};

// 8 宽的小图标，每行低 8 位
static const uint8_t ICON_HEART[7] = {0x66,0xFF,0xFF,0xFF,0x7E,0x3C,0x18};
static const uint8_t ICON_PAW[7]   = {0x6C,0xFE,0xFE,0x7C,0x38,0x00,0x00};
static const uint8_t ICON_SPK[7]   = {0x18,0x38,0x78,0xF8,0x78,0x38,0x18};

static inline void px(int x, int y, uint16_t c) {
    if (x < 0 || y < 0 || x >= DISP_W || y >= UI_STATUS_H) return;
    s_bar[y * DISP_W + x] = c;
}

static void fill_rect(int x, int y, int w, int h, uint16_t c) {
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++) px(x + i, y + j, c);
}

// 画点阵：bits 每行低 width 位，scale 倍放大
static void blit_bits(int x, int y, const uint8_t *bits, int width, int rows,
                      int scale, uint16_t c) {
    for (int r = 0; r < rows; r++) {
        for (int b = 0; b < width; b++) {
            if (bits[r] & (1 << (width - 1 - b))) {
                fill_rect(x + b * scale, y + r * scale, scale, scale, c);
            }
        }
    }
}

// 画整数，返回占用宽度
static int draw_num(int x, int y, int v, int scale, uint16_t c) {
    char buf[8];
    int n = 0;
    if (v < 0) v = 0;
    if (v == 0) { buf[n++] = 0; }
    while (v > 0 && n < 6) { buf[n++] = (char)(v % 10); v /= 10; }
    int w = 0;
    for (int i = n - 1; i >= 0; i--) {
        blit_bits(x + w, y, FONT[(int)buf[i]], 5, 7, scale, c);
        w += 5 * scale + scale;
    }
    return w;
}

static void draw_battery(int x, int y, int pct) {
    uint16_t c = (pct >= 0 && pct <= 20) ? COL_LOW : COL_FG;
    // 外壳 20x11 + 正极小凸起
    fill_rect(x, y, 20, 1, c);
    fill_rect(x, y + 10, 20, 1, c);
    fill_rect(x, y, 1, 11, c);
    fill_rect(x + 19, y, 1, 11, c);
    fill_rect(x + 20, y + 3, 2, 5, c);
    // 内部电量条
    int fillw = (pct < 0) ? 0 : (16 * pct) / 100;
    if (fillw > 0) fill_rect(x + 2, y + 2, fillw, 7, c);
}

void ui_init(void) {
    for (int i = 0; i < DISP_W * UI_STATUS_H; i++) s_bar[i] = COL_BG;
}

void ui_draw_status(int battery_pct, int roam, int affinity, bool muted) {
    for (int i = 0; i < DISP_W * UI_STATUS_H; i++) s_bar[i] = COL_BG;

    // 电量：图标 + 百分比
    draw_battery(4, 8, battery_pct);
    int x = 32;
    if (battery_pct >= 0) {
        x += draw_num(x, 7, battery_pct, 2, COL_FG);
        blit_bits(x, 7, FONT[10], 5, 7, 2, COL_FG);   // '%'
    }

    // 溜达值：爪印 + 数字
    blit_bits(84, 8, ICON_PAW, 8, 7, 2, COL_FG);
    draw_num(104, 7, roam, 2, COL_FG);

    // 好感度：心 + 数字
    blit_bits(150, 8, ICON_HEART, 8, 7, 2, COL_HEART);
    draw_num(170, 7, affinity, 2, COL_HEART);

    // 静音状态：喇叭，静音时打一条斜杠
    blit_bits(216, 8, ICON_SPK, 8, 7, 2, COL_FG);
    if (muted) {
        for (int i = 0; i < 18; i++) px(214 + i, 6 + i, COL_LOW);
        for (int i = 0; i < 18; i++) px(215 + i, 6 + i, COL_LOW);
    }

    display_blit(0, 0, DISP_W, UI_STATUS_H, s_bar);
}

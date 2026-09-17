#pragma once

typedef enum {
    BTN_NONE = 0,
    BTN_UP,
    BTN_DOWN,
    BTN_OK,
} btn_t;

typedef struct {
    btn_t click;       // 短按（松手时产生）
    btn_t longpress;   // 长按（按住约 1 秒时产生一次）
} btn_event_t;

void        buttons_init(void);
btn_event_t buttons_poll(void);

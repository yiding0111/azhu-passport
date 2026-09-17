#pragma once

typedef enum {
    BTN_NONE = 0,
    BTN_UP,
    BTN_DOWN,
    BTN_OK,
} btn_t;

void  buttons_init(void);
// 返回本次「新按下」的键（按下沿），无则 BTN_NONE
btn_t buttons_read_event(void);

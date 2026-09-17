#pragma once
#include <stdbool.h>

#define UI_STATUS_H 28   // 顶部状态条高度（阿猪本体在下面，不受影响）

void ui_init(void);
// 画顶部状态条：电量% / 溜达值 / 好感度 / 静音图标
void ui_draw_status(int battery_pct, int roam, int affinity, bool muted);

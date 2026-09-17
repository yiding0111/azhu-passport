#pragma once

void battery_init(void);
// 返回 0..100 的电量百分比；读失败返回 -1
int  battery_percent(void);

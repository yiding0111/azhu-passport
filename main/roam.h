#pragma once
#include <stdbool.h>

// 溜达值：靠 WiFi 扫描周围热点集合的变化来推断"你带着它挪窝了"。
// 只扫描，不连接任何网络、不需要密码、不发数据。开关可关（省电）。
void roam_init(void);
int  roam_value(void);
void roam_set_enabled(bool en);
bool roam_enabled(void);

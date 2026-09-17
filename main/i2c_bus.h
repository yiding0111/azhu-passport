#pragma once
#include "driver/i2c_master.h"

// ES8311 音频 codec (0x18) 和 CW2017 电量计 (0x63) 挂在同一对引脚上，
// 必须共用一条总线，谁都不能自己 i2c_new_master_bus。
i2c_master_bus_handle_t i2c_bus_get(void);

#include "battery.h"
#include "i2c_bus.h"
#include "board_config.h"

#include "esp_log.h"

#define TAG "battery"

// CW2017 电量计寄存器
#define CW2017_REG_VERSION 0x00
#define CW2017_REG_SOC_INT 0x04   // 电量整数百分比

static i2c_master_dev_handle_t s_dev;

void battery_init(void) {
    i2c_device_config_t dc = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = CW2017_I2C_ADDR,
        .scl_speed_hz    = 100000,
    };
    if (i2c_master_bus_add_device(i2c_bus_get(), &dc, &s_dev) != ESP_OK) {
        ESP_LOGW(TAG, "CW2017 挂载失败");
        s_dev = NULL;
        return;
    }
    uint8_t reg = CW2017_REG_VERSION, ver = 0;
    if (i2c_master_transmit_receive(s_dev, &reg, 1, &ver, 1, 100) == ESP_OK) {
        ESP_LOGI(TAG, "CW2017 就绪 VERSION=0x%02X", ver);
    } else {
        ESP_LOGW(TAG, "CW2017 无应答");
    }
}

int battery_percent(void) {
    if (s_dev == NULL) return -1;
    uint8_t reg = CW2017_REG_SOC_INT, soc = 0;
    if (i2c_master_transmit_receive(s_dev, &reg, 1, &soc, 1, 100) != ESP_OK) return -1;
    if (soc > 100) soc = 100;
    return soc;
}

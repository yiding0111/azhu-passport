#include "i2c_bus.h"
#include "board_config.h"
#include "esp_err.h"

static i2c_master_bus_handle_t s_bus;

i2c_master_bus_handle_t i2c_bus_get(void) {
    if (s_bus == NULL) {
        i2c_master_bus_config_t cfg = {
            .i2c_port          = I2C_NUM_0,
            .sda_io_num        = I2C_SDA_PIN,
            .scl_io_num        = I2C_SCL_PIN,
            .clk_source        = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority     = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&cfg, &s_bus));
    }
    return s_bus;
}

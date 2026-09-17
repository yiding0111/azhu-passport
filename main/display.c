#include "display.h"
#include "board_config.h"

#include <stdbool.h>
#include <string.h>

#include "driver/spi_common.h"
#include "driver/ledc.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_heap_caps.h"
#include "esp_attr.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

static esp_lcd_panel_handle_t    s_panel;
static esp_lcd_panel_io_handle_t s_io;
static SemaphoreHandle_t         s_trans_done;

// 一次推 BLOCK_ROWS 行，缓冲 240*40*2 = 19KB，远小于可用堆
#define BLOCK_ROWS 40
static uint16_t *s_block;

// ST7789P3 厂商专属初始化序列（power/porch/gamma）。
// 原样取自 FoloToy 原厂 badge 固件——通用 ST7789 驱动点不亮这块屏。
typedef struct {
    uint8_t  cmd;
    uint8_t  data[16];
    uint8_t  len;
    uint16_t delay_ms;
} st_init_cmd_t;

static const st_init_cmd_t ST_INIT[] = {
    {0xB2, {0x05, 0x05, 0x00, 0x33, 0x33}, 5, 0},
    {0xB7, {0x35}, 1, 0},
    {0xBB, {0x21}, 1, 0},
    {0xC0, {0x2C}, 1, 0},
    {0xC2, {0x01}, 1, 0},
    {0xC3, {0x0B}, 1, 0},
    {0xC4, {0x20}, 1, 0},
    {0xC6, {0x0F}, 1, 0},
    {0xD0, {0xA7, 0xA1}, 2, 0},
    {0xD0, {0xA4, 0xA1}, 2, 0},
    {0xD6, {0xA1}, 1, 0},
    {0xE0, {0xD0, 0x04, 0x08, 0x0A, 0x09, 0x05, 0x2D, 0x43, 0x49, 0x09, 0x16, 0x15, 0x26, 0x2B}, 14, 0},
    {0xE1, {0xD0, 0x03, 0x09, 0x0A, 0x0A, 0x06, 0x2E, 0x44, 0x40, 0x3A, 0x15, 0x15, 0x26, 0x2A}, 14, 10},
};

// draw_bitmap 是异步排队的：这一块真正传完才发信号，否则复用 s_block 会把
// 还没传出去的数据覆写掉，屏上就是错位、残影和斜纹。
static bool IRAM_ATTR on_color_trans_done(esp_lcd_panel_io_handle_t io,
                                          esp_lcd_panel_io_event_data_t *edata,
                                          void *user_ctx) {
    BaseType_t hp_woken = pdFALSE;
    xSemaphoreGiveFromISR(s_trans_done, &hp_woken);
    return hp_woken == pdTRUE;
}

static void backlight_init(void) {
    ledc_timer_config_t timer = {
        .speed_mode      = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num       = LEDC_TIMER_0,
        .freq_hz         = 5000,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&timer);
    ledc_channel_config_t ch = {
        .gpio_num   = DISP_PIN_BL,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel    = LEDC_CHANNEL_0,
        .timer_sel  = LEDC_TIMER_0,
        .duty       = 0,
        .hpoint     = 0,
    };
    ledc_channel_config(&ch);
}

void display_set_backlight(int pct) {
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    uint32_t duty = (1023u * (uint32_t)pct) / 100u;   // 10-bit
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

void display_init(void) {
    s_trans_done = xSemaphoreCreateBinary();

    spi_bus_config_t bus = {
        .mosi_io_num     = DISP_PIN_MOSI,
        .miso_io_num     = -1,
        .sclk_io_num     = DISP_PIN_SCLK,
        .quadwp_io_num   = -1,
        .quadhd_io_num   = -1,
        .max_transfer_sz = DISP_W * BLOCK_ROWS * (int)sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(DISP_SPI_HOST, &bus, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_spi_config_t io = {
        .cs_gpio_num         = DISP_PIN_CS,
        .dc_gpio_num         = DISP_PIN_DC,
        .spi_mode            = 0,
        .pclk_hz             = DISP_PCLK_HZ,
        .trans_queue_depth   = 10,
        .on_color_trans_done = on_color_trans_done,
        .user_ctx            = NULL,
        .lcd_cmd_bits        = 8,
        .lcd_param_bits      = 8,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(
        (esp_lcd_spi_bus_handle_t)DISP_SPI_HOST, &io, &s_io));

    esp_lcd_panel_dev_config_t pc = {
        .reset_gpio_num = DISP_PIN_RST,
        .rgb_ele_order  = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(s_io, &pc, &s_panel));

    ESP_ERROR_CHECK(esp_lcd_panel_reset(s_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(s_panel));
    for (size_t i = 0; i < sizeof(ST_INIT) / sizeof(ST_INIT[0]); i++) {
        ESP_ERROR_CHECK(esp_lcd_panel_io_tx_param(s_io, ST_INIT[i].cmd,
                                                  ST_INIT[i].data, ST_INIT[i].len));
        if (ST_INIT[i].delay_ms) vTaskDelay(pdMS_TO_TICKS(ST_INIT[i].delay_ms));
    }
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(s_panel, DISP_INVERT_COLOR));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(s_panel, true));

    s_block = heap_caps_malloc(DISP_W * BLOCK_ROWS * sizeof(uint16_t), MALLOC_CAP_DMA);
    backlight_init();
}

void display_draw_indexed(const uint8_t *idx, const uint16_t *palette) {
    for (int y = 0; y < DISP_H; y += BLOCK_ROWS) {
        int rows = (y + BLOCK_ROWS <= DISP_H) ? BLOCK_ROWS : (DISP_H - y);
        const uint8_t *src = idx + (size_t)y * DISP_W;
        int n = rows * DISP_W;
        for (int i = 0; i < n; i++) {
            // 这块屏要 big-endian RGB565，逐像素高低字节互换
            s_block[i] = __builtin_bswap16(palette[src[i]]);
        }
        esp_lcd_panel_draw_bitmap(s_panel, 0, y, DISP_W, y + rows, s_block);
        // 等这一块真的传完，再回头覆写同一个缓冲
        xSemaphoreTake(s_trans_done, portMAX_DELAY);
    }
}

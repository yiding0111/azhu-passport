#pragma once
// AI Passport (ESP32-C3) 板级引脚 —— 取自 FoloToy 官方 ai-passport-c3 板级定义。
// 这些是移植的地基，不要凭猜改。

// ---- ST7789P3 240x320 竖屏 ----
#define DISP_SPI_HOST      SPI2_HOST
#define DISP_PIN_SCLK      8
#define DISP_PIN_MOSI      9
#define DISP_PIN_CS        1
#define DISP_PIN_DC        20
#define DISP_PIN_RST       (-1)     // 无独立复位脚，走软复位
#define DISP_PIN_BL        21       // 背光 LEDC PWM，非反相
#define DISP_W             240
#define DISP_H             320
#define DISP_INVERT_COLOR  1        // 关键：这块屏必须反相，否则满屏怪色
#define DISP_TOP_MARGIN    28       // 顶部留给状态条，阿猪的帧不推这几行
#define DISP_PCLK_HZ       (40 * 1000 * 1000)

// ---- 三键分压：都挂 GPIO0 / ADC1 通道0 ----
// 电压窗口(mV)，原厂验证值：上 / 下 / 确认
#define BTN_ADC_CHANNEL    0
#define BTN_UP_MIN         0
#define BTN_UP_MAX         150
#define BTN_DOWN_MIN       150
#define BTN_DOWN_MAX       447
#define BTN_OK_MIN         447
#define BTN_OK_MAX         1900

// ---- ES8311 音频(暂不用，留档) ----
// I2C SDA=10 SCL=7 ; I2S MCLK=6 BCLK=5 WS=3 DOUT=2 DIN=4 ; addr 0x18 ; 电量计 CW2017 @0x63

// ---- I2C 共享总线（ES8311 + CW2017 都挂这对脚）----
#define I2C_SDA_PIN      10
#define I2C_SCL_PIN      7
#define ES8311_I2C_ADDR  0x18
#define CW2017_I2C_ADDR  0x63

// ---- I2S 到 ES8311 ----
#define I2S_MCLK_PIN     6
#define I2S_BCLK_PIN     5
#define I2S_WS_PIN       3
#define I2S_DOUT_PIN     2
#define I2S_DIN_PIN      4

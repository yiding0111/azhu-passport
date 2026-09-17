#include "audio.h"
#include "board_config.h"
#include "i2c_bus.h"

#include <string.h>
#include <stdint.h>

#include "driver/i2s_std.h"
#include "esp_codec_dev.h"
#include "esp_codec_dev_defaults.h"
#include "esp_log.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG          "audio"
#define NVS_NS       "azhu"
#define KEY_MUTE     "mute"

#define SAMPLE_RATE  16000
#define CHUNK        320                 // 20ms @16kHz
#define AMP_BGM      5200
#define AMP_SFX      7000

static i2s_chan_handle_t     s_tx;
static esp_codec_dev_handle_t s_dev;
static bool s_muted = false;
static bool s_ready = false;

// ---------- 曲谱 ----------
typedef struct { uint16_t hz; uint16_t ms; } note_t;

#define R 0
// 一段欢快的 C 大调琶音循环
static const note_t BGM[] = {
    {523,150},{659,150},{784,150},{659,150},
    {523,150},{659,150},{784,300},
    {440,150},{523,150},{659,150},{523,150},
    {440,150},{523,150},{659,300},
    {349,150},{440,150},{523,150},{440,150},
    {392,150},{494,150},{587,300},
    {523,150},{392,150},{330,150},{392,150},
    {523,300},{R,350},
};
#define BGM_N ((int)(sizeof(BGM)/sizeof(BGM[0])))

// 音效：最多两段
typedef struct { uint16_t hz[2]; uint16_t ms[2]; uint8_t n; } sfx_t;
static const sfx_t SFX[] = {
    [SFX_UP]    = {{1200, 0},    {60, 0},  1},
    [SFX_DOWN]  = {{900, 0},     {60, 0},  1},
    [SFX_OK]    = {{800, 1200},  {40, 40}, 2},
    [SFX_HAPPY] = {{784, 1046},  {70, 90}, 2},
};

// ---------- 合成状态 ----------
static int      s_note_i = 0;
static int      s_note_left = 0;      // 当前音符剩余样本
static uint32_t s_phase_bgm = 0;

static int      s_sfx_seg = -1;       // -1 = 无音效
static int      s_sfx_idx = 0;
static int      s_sfx_left = 0;
static uint32_t s_phase_sfx = 0;
static volatile int s_sfx_req = -1;

static inline int ms_to_samples(int ms) { return (SAMPLE_RATE * ms) / 1000; }

// 方波：相位累加器溢出一半时翻转
static inline int16_t square(uint32_t *phase, uint32_t hz, int amp) {
    if (hz == 0) return 0;
    uint32_t step = (uint32_t)(((uint64_t)hz << 32) / SAMPLE_RATE);
    *phase += step;
    return (*phase & 0x80000000u) ? (int16_t)amp : (int16_t)(-amp);
}

static void fill_chunk(int16_t *buf, int n) {
    for (int i = 0; i < n; i++) {
        int32_t v = 0;

        // BGM
        if (s_note_left <= 0) {
            s_note_i = (s_note_i + 1) % BGM_N;
            s_note_left = ms_to_samples(BGM[s_note_i].ms);
            s_phase_bgm = 0;
        }
        v += square(&s_phase_bgm, BGM[s_note_i].hz, AMP_BGM);
        s_note_left--;

        // 音效（叠加，优先级高）
        if (s_sfx_seg < 0 && s_sfx_req >= 0) {
            s_sfx_idx = s_sfx_req;
            s_sfx_req = -1;
            s_sfx_seg = 0;
            s_sfx_left = ms_to_samples(SFX[s_sfx_idx].ms[0]);
            s_phase_sfx = 0;
        }
        if (s_sfx_seg >= 0) {
            v = v / 3 + square(&s_phase_sfx, SFX[s_sfx_idx].hz[s_sfx_seg], AMP_SFX);
            if (--s_sfx_left <= 0) {
                s_sfx_seg++;
                if (s_sfx_seg >= SFX[s_sfx_idx].n) {
                    s_sfx_seg = -1;
                } else {
                    s_sfx_left = ms_to_samples(SFX[s_sfx_idx].ms[s_sfx_seg]);
                    s_phase_sfx = 0;
                }
            }
        }

        if (v > 32000)  v = 32000;
        if (v < -32000) v = -32000;
        buf[i] = (int16_t)v;
    }
}

static void audio_task(void *arg) {
    (void)arg;
    static int16_t buf[CHUNK];
    while (1) {
        if (s_muted) {
            memset(buf, 0, sizeof(buf));
            s_sfx_req = -1;
        } else {
            fill_chunk(buf, CHUNK);
        }
        esp_codec_dev_write(s_dev, buf, sizeof(buf));
    }
}

static void load_mute(void) {
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) == ESP_OK) {
        int32_t v = 0;
        if (nvs_get_i32(h, KEY_MUTE, &v) == ESP_OK) s_muted = (v != 0);
        nvs_close(h);
    }
}

static void save_mute(void) {
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_i32(h, KEY_MUTE, s_muted ? 1 : 0);
        nvs_commit(h);
        nvs_close(h);
    }
}

void audio_init(void) {
    load_mute();

    // I2S TX（只输出，不要麦克风）
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true;
    if (i2s_new_channel(&chan_cfg, &s_tx, NULL) != ESP_OK) {
        ESP_LOGE(TAG, "i2s_new_channel 失败");
        return;
    }
    i2s_std_config_t std_cfg = {
        .clk_cfg  = {
            .sample_rate_hz = SAMPLE_RATE,
            .clk_src        = I2S_CLK_SRC_DEFAULT,
            .mclk_multiple  = I2S_MCLK_MULTIPLE_256,
        },
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,
                                                        I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_MCLK_PIN,
            .bclk = I2S_BCLK_PIN,
            .ws   = I2S_WS_PIN,
            .dout = I2S_DOUT_PIN,
            .din  = I2S_GPIO_UNUSED,
            .invert_flags = {0},
        },
    };
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(s_tx, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(s_tx));

    // esp_codec_dev：数据口 + 控制口
    audio_codec_i2s_cfg_t i2s_cfg = {
        .port = I2S_NUM_0,
        .rx_handle = NULL,
        .tx_handle = s_tx,
    };
    const audio_codec_data_if_t *data_if = audio_codec_new_i2s_data(&i2s_cfg);

    audio_codec_i2c_cfg_t i2c_cfg = {
        .port = I2C_NUM_0,
        .addr = ES8311_I2C_ADDR,
        .bus_handle = (void *)i2c_bus_get(),
    };
    const audio_codec_ctrl_if_t *ctrl_if = audio_codec_new_i2c_ctrl(&i2c_cfg);
    if (data_if == NULL || ctrl_if == NULL) {
        ESP_LOGE(TAG, "codec 接口创建失败");
        return;
    }

    // 关键：先软复位，数字块要拉住几毫秒，否则状态机起不来
    uint8_t reset_value = 0x1F;
    ctrl_if->write_reg(ctrl_if, 0x00, 1, &reset_value, 1);
    vTaskDelay(pdMS_TO_TICKS(5));

    const audio_codec_gpio_if_t *gpio_if = audio_codec_new_gpio();

    es8311_codec_cfg_t es_cfg = {
        .ctrl_if    = ctrl_if,
        .gpio_if    = gpio_if,
        .codec_mode = ESP_CODEC_DEV_WORK_MODE_DAC,
        .pa_pin     = GPIO_NUM_NC,
        .use_mclk   = true,
        .hw_gain    = { .pa_voltage = 5.0f, .codec_dac_voltage = 3.3f },
    };
    const audio_codec_if_t *codec_if = es8311_codec_new(&es_cfg);
    if (codec_if == NULL) {
        ESP_LOGE(TAG, "ES8311 初始化失败");
        return;
    }

    esp_codec_dev_cfg_t dev_cfg = {
        .dev_type = ESP_CODEC_DEV_TYPE_OUT,
        .codec_if = codec_if,
        .data_if  = data_if,
    };
    s_dev = esp_codec_dev_new(&dev_cfg);
    if (s_dev == NULL) {
        ESP_LOGE(TAG, "esp_codec_dev_new 失败");
        return;
    }

    esp_codec_dev_sample_info_t fs = {
        .bits_per_sample = 16,
        .channel         = 1,
        .channel_mask    = 0,
        .sample_rate     = SAMPLE_RATE,
    };
    esp_codec_dev_open(s_dev, &fs);
    esp_codec_dev_set_out_vol(s_dev, 70);

    s_note_left = 0;
    s_ready = true;
    ESP_LOGI(TAG, "音频就绪（%s）", s_muted ? "静音" : "有声");
    xTaskCreate(audio_task, "audio", 4096, NULL, 4, NULL);
}

void audio_set_muted(bool m) {
    s_muted = m;
    save_mute();
    ESP_LOGI(TAG, "%s", m ? "已静音" : "已取消静音");
}

bool audio_muted(void) { return s_muted; }

void audio_toggle_mute(void) { audio_set_muted(!s_muted); }

void audio_sfx(int which) {
    if (!s_ready || s_muted) return;
    if (which < 0 || which > SFX_HAPPY) return;
    s_sfx_req = which;
}

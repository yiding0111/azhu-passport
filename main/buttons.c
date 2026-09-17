#include "buttons.h"
#include "board_config.h"

#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_timer.h"

#define LONG_PRESS_US (1000LL * 1000)   // 按住 1 秒算长按

static adc_oneshot_unit_handle_t s_adc;
static adc_cali_handle_t          s_cali;
static bool                       s_cali_ok;

static btn_t   s_cur = BTN_NONE;
static int64_t s_down_us = 0;
static bool    s_long_fired = false;

static btn_t classify(int mv) {
    if (mv >= BTN_UP_MIN   && mv < BTN_UP_MAX)   return BTN_UP;
    if (mv >= BTN_DOWN_MIN && mv < BTN_DOWN_MAX) return BTN_DOWN;
    if (mv >= BTN_OK_MIN   && mv < BTN_OK_MAX)   return BTN_OK;
    return BTN_NONE;   // 没按时上拉到约 3300mV，落在所有窗口之外
}

void buttons_init(void) {
    adc_oneshot_unit_init_cfg_t unit = { .unit_id = ADC_UNIT_1 };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit, &s_adc));

    adc_oneshot_chan_cfg_t chan = {
        .atten    = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(s_adc, BTN_ADC_CHANNEL, &chan));

    adc_cali_curve_fitting_config_t cali = {
        .unit_id  = ADC_UNIT_1,
        .chan     = BTN_ADC_CHANNEL,
        .atten    = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    s_cali_ok = (adc_cali_create_scheme_curve_fitting(&cali, &s_cali) == ESP_OK);
}

static int read_mv(void) {
    int raw = 0;
    adc_oneshot_read(s_adc, BTN_ADC_CHANNEL, &raw);
    int mv = raw;
    if (s_cali_ok) adc_cali_raw_to_voltage(s_cali, raw, &mv);
    return mv;
}

btn_event_t buttons_poll(void) {
    btn_event_t ev = { BTN_NONE, BTN_NONE };

    btn_t a = classify(read_mv());
    btn_t b = classify(read_mv());
    if (a != b) return ev;              // 两次不一致，这轮不作数
    btn_t now = a;

    int64_t t = esp_timer_get_time();

    if (now != BTN_NONE && s_cur == BTN_NONE) {
        s_cur = now;
        s_down_us = t;
        s_long_fired = false;
    } else if (now != BTN_NONE && now == s_cur) {
        if (!s_long_fired && (t - s_down_us) >= LONG_PRESS_US) {
            s_long_fired = true;
            ev.longpress = s_cur;
        }
    } else if (now == BTN_NONE && s_cur != BTN_NONE) {
        if (!s_long_fired) ev.click = s_cur;   // 长按过就不再算短按
        s_cur = BTN_NONE;
    }
    return ev;
}

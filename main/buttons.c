#include "buttons.h"
#include "board_config.h"

#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

static adc_oneshot_unit_handle_t s_adc;
static adc_cali_handle_t          s_cali;
static bool                       s_cali_ok;
static btn_t                      s_last = BTN_NONE;

static btn_t classify(int mv) {
    if (mv >= BTN_UP_MIN   && mv < BTN_UP_MAX)   return BTN_UP;
    if (mv >= BTN_DOWN_MIN && mv < BTN_DOWN_MAX) return BTN_DOWN;
    if (mv >= BTN_OK_MIN   && mv < BTN_OK_MAX)   return BTN_OK;
    return BTN_NONE;   // 无按下时上拉到 ~3300mV，落在所有窗口之外
}

void buttons_init(void) {
    adc_oneshot_unit_init_cfg_t unit = { .unit_id = ADC_UNIT_1 };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit, &s_adc));

    adc_oneshot_chan_cfg_t chan = {
        .atten    = ADC_ATTEN_DB_12,          // ~0..3.3V 量程
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

btn_t buttons_read_event(void) {
    btn_t a = classify(read_mv());
    btn_t b = classify(read_mv());
    btn_t cur = (a == b) ? a : BTN_NONE;   // 两次一致才采信
    btn_t ev = BTN_NONE;
    if (cur != BTN_NONE && s_last == BTN_NONE) ev = cur;   // 按下沿
    s_last = cur;
    return ev;
}

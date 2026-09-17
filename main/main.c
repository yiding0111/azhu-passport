#include "display.h"
#include "buttons.h"
#include "azhu.h"
#include "board_config.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_random.h"
#include "esp_log.h"

static const char *TAG = "azhu";

static int find_action(const char *want) {
    int n = azhu_num_actions();
    for (int a = 0; a < n; a++) {
        const char *name = azhu_action_name(a);   // 固定12字节，可能无结尾0
        int k = 0;
        while (k < 12 && want[k] && name[k] == want[k]) k++;
        if (want[k] == 0 && (k == 12 || name[k] == 0)) return a;
    }
    return -1;
}

void app_main(void) {
    azhu_init();
    display_init();
    display_set_backlight(90);

    ESP_LOGI(TAG, "阿猪 on AI Passport：%d 动作 · %d 帧/动作 · %dx%d",
             azhu_num_actions(), azhu_frames_per(), azhu_w(), azhu_h());

    const int   na  = azhu_num_actions();
    const int   fpa = azhu_frames_per();
    const uint16_t *pal = azhu_palette();

    int sleep_idx = find_action("sleep");
    if (sleep_idx < 0) sleep_idx = 0;

    int action = 0;      // 0 = wave，开机打招呼
    int frame = 0;
    int idle_ticks = 0;

    const int FRAME_MS       = 230;
    const int IDLE_TO_SLEEP  = 40;   // 约 40 帧 ≈ 9 秒无操作 → 睡觉

    while (1) {
        display_draw_indexed(azhu_frame(action, frame), pal);
        frame = (frame + 1) % fpa;

        // 一帧时间内多采几次按键，反应更跟手
        btn_t ev = BTN_NONE;
        for (int i = 0; i < 8; i++) {
            btn_t e = buttons_read_event();
            if (e != BTN_NONE) ev = e;
            vTaskDelay(pdMS_TO_TICKS(FRAME_MS / 8));
        }

        if (ev == BTN_UP) {
            action = (action - 1 + na) % na; frame = 0; idle_ticks = 0;
            ESP_LOGI(TAG, "← %.*s", 12, azhu_action_name(action));
        } else if (ev == BTN_DOWN) {
            action = (action + 1) % na; frame = 0; idle_ticks = 0;
            ESP_LOGI(TAG, "→ %.*s", 12, azhu_action_name(action));
        } else if (ev == BTN_OK) {
            action = (int)(esp_random() % (uint32_t)na); frame = 0; idle_ticks = 0;
            ESP_LOGI(TAG, "★ %.*s", 12, azhu_action_name(action));
        } else {
            idle_ticks++;
            if (idle_ticks == IDLE_TO_SLEEP && action != sleep_idx) {
                action = sleep_idx; frame = 0;
                ESP_LOGI(TAG, "…睡觉");
            }
        }
    }
}

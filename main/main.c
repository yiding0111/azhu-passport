#include "display.h"
#include "buttons.h"
#include "azhu.h"
#include "board_config.h"
#include "ui.h"
#include "battery.h"
#include "affinity.h"
#include "roam.h"
#include "audio.h"

#include "nvs_flash.h"
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

// 按好感度档位挑待机动作：生疏就蔫，熟了就活泼
static int idle_pool(int level, int *out, int max) {
    static const char *L0[] = {"sad", "shy", "sleep"};
    static const char *L1[] = {"think", "eat", "drink", "wave"};
    static const char *L2[] = {"celebrate", "laugh", "dance", "wave"};
    const char **src;
    int n;
    if (level == 0)      { src = L0; n = 3; }
    else if (level == 1) { src = L1; n = 4; }
    else                 { src = L2; n = 4; }

    int k = 0;
    for (int i = 0; i < n && k < max; i++) {
        int a = find_action(src[i]);
        if (a >= 0) out[k++] = a;
    }
    return k;
}

void app_main(void) {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    azhu_init();
    display_init();
    display_set_backlight(90);
    ui_init();
    buttons_init();
    battery_init();
    affinity_init();
    audio_init();
    roam_init();

    ESP_LOGI(TAG, "阿猪 on AI Passport：%d 动作 · %d 帧/动作 · %dx%d",
             azhu_num_actions(), azhu_frames_per(), azhu_w(), azhu_h());

    const int na  = azhu_num_actions();
    const int fpa = azhu_frames_per();
    const uint16_t *pal = azhu_palette();

    int sleep_idx = find_action("sleep");
    if (sleep_idx < 0) sleep_idx = 0;
    int sad_idx = find_action("sad");
    if (sad_idx < 0) sad_idx = sleep_idx;

    int action = 0;      // 0 = wave，开机打招呼
    int frame = 0;
    int idle_ticks = 0;
    int last_level = affinity_level();
    int bat = -1;
    int bat_tick = 0;

    const int FRAME_MS      = 230;
    const int IDLE_TO_SLEEP = 40;   // 约 9 秒没人理
    const int BAT_EVERY     = 40;   // 约 9 秒读一次电量

    while (1) {
        display_draw_indexed(azhu_frame(action, frame), pal);
        ui_draw_status(bat, roam_value(), affinity_get(), audio_muted());
        frame = (frame + 1) % fpa;

        // 一帧时间内多采几次按键
        btn_event_t ev = { BTN_NONE, BTN_NONE };
        for (int i = 0; i < 8; i++) {
            btn_event_t e = buttons_poll();
            if (e.longpress != BTN_NONE) ev.longpress = e.longpress;
            if (e.click     != BTN_NONE) ev.click     = e.click;
            vTaskDelay(pdMS_TO_TICKS(FRAME_MS / 8));
        }

        if (ev.longpress == BTN_UP) {
            audio_toggle_mute();
            idle_ticks = 0;
        } else if (ev.click == BTN_UP) {
            action = (action - 1 + na) % na; frame = 0; idle_ticks = 0;
            audio_sfx(SFX_UP);
            affinity_on_interact();
        } else if (ev.click == BTN_DOWN) {
            action = (action + 1) % na; frame = 0; idle_ticks = 0;
            audio_sfx(SFX_DOWN);
            affinity_on_interact();
        } else if (ev.click == BTN_OK) {
            action = (int)(esp_random() % (uint32_t)na); frame = 0; idle_ticks = 0;
            audio_sfx(SFX_OK);
            affinity_on_interact();
        } else {
            idle_ticks++;
            if (idle_ticks >= IDLE_TO_SLEEP) {
                idle_ticks = 0;
                if (bat >= 0 && bat <= 20) {
                    action = sad_idx;          // 没电了就委屈
                } else {
                    int pool[6];
                    int k = idle_pool(affinity_level(), pool, 6);
                    action = (k > 0) ? pool[esp_random() % (uint32_t)k] : sleep_idx;
                }
                frame = 0;
            }
        }

        affinity_tick();

        int lv = affinity_level();
        if (lv > last_level) {
            ESP_LOGI(TAG, "好感度升档：%d", lv);
            audio_sfx(SFX_HAPPY);
        }
        last_level = lv;

        if (++bat_tick >= BAT_EVERY) {
            bat_tick = 0;
            bat = battery_percent();
        }
    }
}

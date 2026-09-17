#include "affinity.h"

#include "nvs.h"
#include "esp_timer.h"
#include "esp_log.h"

#define TAG      "affinity"
#define NVS_NS   "azhu"
#define KEY_AFF  "aff"

#define COOLDOWN_US   (30LL * 1000000)        // 互动加分冷却 30 秒
#define DECAY_US      (30LL * 60 * 1000000)   // 每 30 分钟检查一次衰减

static int     s_aff = 20;          // 初来乍到，给个起步值
static int64_t s_last_interact_us = 0;
static int64_t s_last_decay_us = 0;

static void clamp(void) {
    if (s_aff < 0)   s_aff = 0;
    if (s_aff > 100) s_aff = 100;
}

static void save(void) {
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_i32(h, KEY_AFF, (int32_t)s_aff);
        nvs_commit(h);
        nvs_close(h);
    }
}

void affinity_init(void) {
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) == ESP_OK) {
        int32_t v = 0;
        if (nvs_get_i32(h, KEY_AFF, &v) == ESP_OK) s_aff = (int)v;
        nvs_close(h);
    }
    clamp();
    int64_t now = esp_timer_get_time();
    s_last_decay_us    = now;
    s_last_interact_us = now;
    ESP_LOGI(TAG, "好感度 %d（%s）", s_aff,
             affinity_level() == 0 ? "陌生" : (affinity_level() == 1 ? "熟悉" : "亲密"));
}

void affinity_on_interact(void) {
    int64_t now = esp_timer_get_time();
    if (now - s_last_interact_us < COOLDOWN_US) {
        s_last_interact_us = now;   // 仍然算"被理会了"，只是不加分
        return;
    }
    s_last_interact_us = now;
    s_aff += 1;
    clamp();
    save();
}

void affinity_on_roam(void) {
    s_aff += 2;
    clamp();
    save();
}

void affinity_tick(void) {
    int64_t now = esp_timer_get_time();
    if (now - s_last_decay_us >= DECAY_US) {
        s_last_decay_us = now;
        if (now - s_last_interact_us >= DECAY_US) {
            s_aff -= 1;
            clamp();
            save();
            ESP_LOGI(TAG, "好久没人理，好感度降到 %d", s_aff);
        }
    }
}

int affinity_get(void)   { return s_aff; }
int affinity_level(void) { return s_aff <= 30 ? 0 : (s_aff <= 70 ? 1 : 2); }

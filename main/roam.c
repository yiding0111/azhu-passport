#include "roam.h"
#include "affinity.h"

#include <string.h>

#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG            "roam"
#define NVS_NS         "azhu"
#define KEY_ROAM       "roam"
#define MAX_AP         24
#define SCAN_PERIOD_MS 30000

static uint8_t s_prev[MAX_AP][6];
static int     s_prev_n  = 0;
static int     s_roam    = 0;
static bool    s_enabled = true;
static bool    s_wifi_up = false;

static void save(void) {
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_i32(h, KEY_ROAM, (int32_t)s_roam);
        nvs_commit(h);
        nvs_close(h);
    }
}

static bool contains(uint8_t (*arr)[6], int n, const uint8_t *mac) {
    for (int i = 0; i < n; i++) {
        if (memcmp(arr[i], mac, 6) == 0) return true;
    }
    return false;
}

static void scan_once(void) {
    wifi_scan_config_t sc = {
        .ssid = NULL, .bssid = NULL, .channel = 0,
        .show_hidden = false,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
    };
    if (esp_wifi_scan_start(&sc, true) != ESP_OK) return;

    uint16_t n = MAX_AP;
    static wifi_ap_record_t recs[MAX_AP];
    if (esp_wifi_scan_get_ap_records(&n, recs) != ESP_OK) return;

    int cn = (n > MAX_AP) ? MAX_AP : n;
    static uint8_t cur[MAX_AP][6];
    for (int i = 0; i < cn; i++) memcpy(cur[i], recs[i].bssid, 6);

    if (s_prev_n > 0 && cn > 0) {
        int gone = 0, fresh = 0;
        for (int i = 0; i < s_prev_n; i++) if (!contains(cur, cn, s_prev[i])) gone++;
        for (int i = 0; i < cn; i++)        if (!contains(s_prev, s_prev_n, cur[i])) fresh++;

        int total  = (s_prev_n > cn) ? s_prev_n : cn;
        int thresh = total / 3;
        if (thresh < 3) thresh = 3;

        if (gone + fresh >= thresh) {
            s_roam++;
            save();
            affinity_on_roam();
            ESP_LOGI(TAG, "溜达 +1（走了 %d 个热点、来了 %d 个），累计 %d", gone, fresh, s_roam);
        }
    }

    memcpy(s_prev, cur, (size_t)cn * 6);
    s_prev_n = cn;
}

static void roam_task(void *arg) {
    (void)arg;
    while (1) {
        if (s_enabled && s_wifi_up) scan_once();
        vTaskDelay(pdMS_TO_TICKS(SCAN_PERIOD_MS));
    }
}

void roam_init(void) {
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) == ESP_OK) {
        int32_t v = 0;
        if (nvs_get_i32(h, KEY_ROAM, &v) == ESP_OK) s_roam = (int)v;
        nvs_close(h);
    }

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());   // 只 start，不 connect
    s_wifi_up = true;

    ESP_LOGI(TAG, "溜达值 %d，扫描每 %d 秒一次（只嗅探，不联网）", s_roam, SCAN_PERIOD_MS / 1000);
    xTaskCreate(roam_task, "roam", 4096, NULL, 3, NULL);
}

int  roam_value(void)             { return s_roam; }
void roam_set_enabled(bool en)    { s_enabled = en; }
bool roam_enabled(void)           { return s_enabled; }

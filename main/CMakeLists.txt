idf_component_register(
    SRCS "main.c" "display.c" "buttons.c" "azhu.c" "i2c_bus.c"
         "ui.c" "battery.c" "affinity.c" "roam.c" "audio.c"
    INCLUDE_DIRS "."
    EMBED_FILES "azhu_assets.bin"
    REQUIRES esp_lcd driver esp_adc esp_timer esp_hw_support
             esp_wifi esp_netif esp_event nvs_flash esp_codec_dev
)

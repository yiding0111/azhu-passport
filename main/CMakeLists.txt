idf_component_register(
    SRCS "main.c" "display.c" "buttons.c" "azhu.c"
    INCLUDE_DIRS "."
    EMBED_FILES "azhu_assets.bin"
    REQUIRES esp_lcd driver esp_adc esp_timer esp_hw_support
)

idf_component_register(
    SRCS "main.cpp" "../../../src/gif/gif_creality.c" "../../../src/gif/gif_standby_green.c"
    INCLUDE_DIRS "."
    REQUIRES arduino-esp32 lvgl tft_espi esp_timer esp_system
)
idf_component_register(
    SRCS "main.cpp" "network_spike.cpp" "../../../src/gif/gif_creality.c" "../../../src/gif/gif_standby_green.c"
    INCLUDE_DIRS "."
    REQUIRES arduino-esp32 lvgl esp_driver_gpio esp_driver_spi esp_timer esp_system esp_psram spi_flash espasyncwebserver arduinojson
)
idf_component_register(
        SRCS "main.cpp" "ui/display_hal.cpp" "ui/splash.c"
            "ui/assets/fonts/ui_font_InterSemiBold14.c"
            "ui/assets/fonts/ui_font_InterSemiBold18.c"
            "ui/assets/fonts/ui_font_InterSemiBold48.c"
    INCLUDE_DIRS "." "../include"
    REQUIRES arduino-esp32 lvgl esp_driver_gpio esp_driver_spi esp_timer esp_system esp_psram spi_flash
)
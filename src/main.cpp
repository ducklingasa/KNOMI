#include <Arduino.h>
#include <esp_flash.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_psram.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <lvgl.h>

#include "ui/display_hal.h"

namespace {
void ui_task(void *)
{
    if (!display_hal::initialize()) {
        ESP_LOGE("knomi", "Display initialization failed");
        vTaskDelete(nullptr);
        return;
    }
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_black(), 0);
    lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_COVER, 0);
    lv_obj_clear_flag(lv_scr_act(), LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *caption = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_color(caption, lv_color_white(), 0);
    lv_obj_set_style_text_align(caption, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(caption, "KNOMI Creality\nDisplay-only boot");
    lv_obj_align(caption, LV_ALIGN_TOP_MID, 0, 28);
    const uint32_t colors[] = {0xFF0000, 0x00FF00, 0x0000FF};
    for (unsigned index = 0; index < 3; ++index) {
        lv_obj_t *bar = lv_obj_create(lv_scr_act());
        lv_obj_remove_style_all(bar);
        lv_obj_set_size(bar, 50, 80);
        lv_obj_set_pos(bar, 45 + index * 50, 80);
        lv_obj_set_style_bg_color(bar, lv_color_hex(colors[index]), 0);
        lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    }
    lv_obj_t *uptime = lv_label_create(lv_scr_act());
    lv_obj_set_size(uptime, 170, 20);
    lv_obj_set_style_text_color(uptime, lv_color_white(), 0);
    lv_obj_set_style_text_align(uptime, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(uptime, LV_ALIGN_BOTTOM_MID, 0, -25);
    uint32_t displayed_seconds = UINT32_MAX;
    uint32_t logged_seconds = UINT32_MAX;
    while (true) {
        display_hal::poll();
        const uint32_t seconds = esp_timer_get_time() / 1000000;
        if (seconds != displayed_seconds) {
            lv_label_set_text_fmt(uptime, "%02lu:%02lu", static_cast<unsigned long>(seconds / 60),
                                  static_cast<unsigned long>(seconds % 60));
            displayed_seconds = seconds;
        }
        lv_timer_handler();
        if (seconds / 5 != logged_seconds) {
            ESP_LOGI("knomi", "uptime=%lu internal_free=%u psram_free=%u psram_largest=%u",
                     static_cast<unsigned long>(seconds),
                     static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
                     static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)),
                     static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)));
            logged_seconds = seconds / 5;
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
}

extern "C" void app_main()
{
    initArduino();
    uint32_t flash_size = 0;
    if (esp_flash_get_size(nullptr, &flash_size) != ESP_OK || flash_size != 4 * 1024 * 1024
        || !esp_psram_is_initialized() || esp_psram_get_size() != 2 * 1024 * 1024) {
        ESP_LOGE("knomi", "Expected ESP32-S3-Zero with 4 MiB flash and 2 MiB initialized PSRAM");
        return;
    }
    ESP_LOGI("knomi", "IDF %s, reset=%d; display-only boot; networking and printer controls disabled",
             esp_get_idf_version(), static_cast<int>(esp_reset_reason()));
    if (xTaskCreatePinnedToCore(ui_task, "knomi_ui", 8192, nullptr, 4, nullptr, 1) != pdPASS) {
        ESP_LOGE("knomi", "UI task creation failed");
    }
}
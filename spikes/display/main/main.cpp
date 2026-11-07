#include <Arduino.h>
#include <TFT_eSPI.h>
#include <lvgl.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_system.h>
#include <esp_timer.h>

LV_IMG_DECLARE(gif_creality);
LV_IMG_DECLARE(gif_standby_green);

static_assert(LV_COLOR_DEPTH == 16 && LV_COLOR_16_SWAP == 1, "DMA requires swapped RGB565");
static_assert(LV_MEM_CUSTOM == 1 && LV_USE_GIF == 1, "Spike requires heap allocation and GIF support");

namespace {
constexpr uint32_t heap_caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
constexpr uint32_t buffer_pixels = 240 * 24;
constexpr uint32_t stage_ms = 30000;
const char *tag = "display_spike";
TFT_eSPI display;
lv_disp_draw_buf_t draw_buffers;
lv_disp_drv_t display_driver;
lv_disp_drv_t *pending_flush = nullptr;
uint32_t completed_frames = 0;
lv_obj_t *gif = nullptr;
lv_obj_t *pattern = nullptr;
lv_obj_t *caption = nullptr;
uint32_t current_stage = 0;

void log_heap(const char *stage, float fps = 0) {
    ESP_LOGI(tag, "%s free=%u largest=%u minimum=%u fps=%.2f",
             stage,
             static_cast<unsigned>(heap_caps_get_free_size(heap_caps)),
             static_cast<unsigned>(heap_caps_get_largest_free_block(heap_caps)),
             static_cast<unsigned>(heap_caps_get_minimum_free_size(heap_caps)), fps);
}

void finish_flush() {
    if (pending_flush != nullptr && !display.dmaBusy()) {
        if (lv_disp_flush_is_last(pending_flush)) {
            ++completed_frames;
        }
        display.endWrite();
        lv_disp_drv_t *finished = pending_flush;
        pending_flush = nullptr;
        lv_disp_flush_ready(finished);
    }
}

void flush_display(lv_disp_drv_t *driver, const lv_area_t *area, lv_color_t *pixels) {
    display.startWrite();
    display.pushImageDMA(area->x1, area->y1, area->x2 - area->x1 + 1,
                         area->y2 - area->y1 + 1,
                         reinterpret_cast<uint16_t *>(pixels));
    pending_flush = driver;
}

void wait_flush(lv_disp_drv_t *) {
    display.dmaWait();
    finish_flush();
}

void lv_log(const char *message) {
    ESP_LOGW(tag, "%s", message);
}

void show_stage(uint32_t stage) {
    current_stage = stage;
    if (stage == 0) {
        lv_obj_add_flag(gif, LV_OBJ_FLAG_HIDDEN);
        auto *animation = reinterpret_cast<lv_gif_t *>(gif);
        lv_timer_pause(animation->timer);
        lv_obj_clear_flag(pattern, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(caption, "TOP - RGB\nDMA 40 MHz");
        log_heap("pattern");
        return;
    }

    lv_obj_add_flag(pattern, LV_OBJ_FLAG_HIDDEN);
    const lv_img_dsc_t *asset = stage == 1 ? &gif_creality : &gif_standby_green;
    const char *name = stage == 1 ? "gif_creality" : "gif_standby_green";
    log_heap("before GIF");
    lv_timer_pause(reinterpret_cast<lv_gif_t *>(gif)->timer);
    lv_gif_set_src(gif, asset);
    if (reinterpret_cast<lv_gif_t *>(gif)->gif == nullptr) {
        lv_obj_add_flag(gif, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(caption, "GIF allocation failed");
        ESP_LOGE(tag, "Decoder failed: %s", name);
    } else {
        lv_obj_clear_flag(gif, LV_OBJ_FLAG_HIDDEN);
        lv_obj_center(gif);
        lv_label_set_text(caption, name);
    }
    log_heap(name);
}

void ui_task(void *) {
    log_heap("before display");
    display.begin();
    display.setRotation(0);
    display.setSwapBytes(false);
    if (!display.initDMA()) {
        ESP_LOGE(tag, "DMA initialization failed");
        vTaskDelete(nullptr);
        return;
    }

    auto *first = static_cast<lv_color_t *>(heap_caps_malloc(
        buffer_pixels * sizeof(lv_color_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA));
    auto *second = static_cast<lv_color_t *>(heap_caps_malloc(
        buffer_pixels * sizeof(lv_color_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA));
    if (first == nullptr || second == nullptr) {
        ESP_LOGE(tag, "DMA buffer allocation failed");
        heap_caps_free(first);
        heap_caps_free(second);
        vTaskDelete(nullptr);
        return;
    }
    log_heap("after DMA buffers");
    lv_init();
    lv_log_register_print_cb(lv_log);
    lv_disp_draw_buf_init(&draw_buffers, first, second, buffer_pixels);
    lv_disp_drv_init(&display_driver);
    display_driver.hor_res = 240;
    display_driver.ver_res = 240;
    display_driver.draw_buf = &draw_buffers;
    display_driver.flush_cb = flush_display;
    display_driver.wait_cb = wait_flush;
    if (lv_disp_drv_register(&display_driver) == nullptr) {
        ESP_LOGE(tag, "LVGL display registration failed");
        vTaskDelete(nullptr);
        return;
    }

    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_black(), 0);
    pattern = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(pattern);
    lv_obj_set_size(pattern, 150, 110);
    lv_obj_center(pattern);
    const uint32_t colors[] = {0xFF0000, 0x00FF00, 0x0000FF};
    for (uint32_t index = 0; index < 3; ++index) {
        lv_obj_t *bar = lv_obj_create(pattern);
        lv_obj_remove_style_all(bar);
        lv_obj_set_pos(bar, index * 50, 0);
        lv_obj_set_size(bar, 50, 110);
        lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(bar, lv_color_hex(colors[index]), 0);
    }
    caption = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_color(caption, lv_color_white(), 0);
    lv_obj_set_style_text_align(caption, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(caption, LV_ALIGN_TOP_MID, 0, 28);
    gif = lv_gif_create(lv_scr_act());
    if (gif == nullptr) {
        ESP_LOGE(tag, "GIF object allocation failed");
        vTaskDelete(nullptr);
        return;
    }
    lv_obj_t *stopwatch = lv_label_create(lv_scr_act());
    lv_obj_set_size(stopwatch, 170, 18);
    lv_obj_set_style_text_color(stopwatch, lv_color_white(), 0);
    lv_obj_set_style_text_align(stopwatch, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(stopwatch, "00:00");
    lv_obj_align(stopwatch, LV_ALIGN_BOTTOM_MID, 0, -22);
    lv_obj_add_flag(stopwatch, LV_OBJ_FLAG_HIDDEN);
    show_stage(0);
    uint32_t last_stage = millis();
    uint32_t last_log = last_stage;
    int64_t playback_started_us = 0;
    uint32_t displayed_seconds = UINT32_MAX;
    while (true) {
        finish_flush();
        lv_timer_handler();
        const uint32_t now = millis();
        if (now - last_stage >= stage_ms) {
            show_stage(current_stage == 1 ? 2 : 1);
            last_stage = now;
        }
        if (current_stage != 0) {
            if (playback_started_us == 0) {
                playback_started_us = esp_timer_get_time();
            }
            const uint32_t elapsed_seconds =
                (esp_timer_get_time() - playback_started_us) / 1000000;
            if (elapsed_seconds != displayed_seconds) {
                lv_label_set_text_fmt(stopwatch, "%02lu:%02lu",
                                      static_cast<unsigned long>(elapsed_seconds / 60),
                                      static_cast<unsigned long>(elapsed_seconds % 60));
                lv_obj_clear_flag(stopwatch, LV_OBJ_FLAG_HIDDEN);
                displayed_seconds = elapsed_seconds;
            }
        }
        if (now - last_log >= 5000) {
            log_heap(current_stage == 0 ? "pattern" : current_stage == 1
                     ? "gif_creality" : "gif_standby_green",
                     completed_frames * 1000.0f / (now - last_log));
            completed_frames = 0;
            last_log = now;
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
}

extern "C" void app_main() {
    initArduino();
    ESP_LOGI(tag, "IDF %s; LVGL %d.%d.%d; TFT_eSPI %s; reset=%d",
             esp_get_idf_version(), LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR,
             LVGL_VERSION_PATCH, TFT_ESPI_VERSION, static_cast<int>(esp_reset_reason()));
    ESP_LOGI(tag, "ESP32 4 MB no PSRAM; SCK18 MOSI23 DC26 CS5 RST33; RGB565 swapped; SPI40MHz");
    if (xTaskCreatePinnedToCore(ui_task, "spike_ui", 8192, nullptr, 4, nullptr, 1) != pdPASS) {
        ESP_LOGE(tag, "UI task creation failed");
    }
}
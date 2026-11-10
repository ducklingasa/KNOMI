#include <Arduino.h>
#include <driver/gpio.h>
#include <driver/spi_master.h>
#include <lvgl.h>
#include <esp_chip_info.h>
#include <esp_flash.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_psram.h>
#include <esp_system.h>
#include <esp_timer.h>
#include "tft_setup.h"
#include "network_spike.h"

LV_IMG_DECLARE(gif_creality);
LV_IMG_DECLARE(gif_standby_green);

static_assert(LV_COLOR_DEPTH == 16 && LV_COLOR_16_SWAP == 1, "DMA requires swapped RGB565");
static_assert(LV_MEM_CUSTOM == 1 && LV_USE_GIF == 1, "Spike requires heap allocation and GIF support");

namespace {
constexpr uint32_t heap_caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
constexpr uint32_t buffer_pixels = 240 * 24;
constexpr uint32_t stage_ms = 30000;
const char *tag = "display_spike";
spi_device_handle_t display_spi = nullptr;
spi_transaction_t pixel_transfer{};
lv_disp_draw_buf_t draw_buffers;
lv_disp_drv_t display_driver;
lv_disp_drv_t *pending_flush = nullptr;
uint32_t completed_frames = 0;
lv_obj_t *gif = nullptr;
lv_obj_t *pattern = nullptr;
lv_obj_t *caption = nullptr;
uint32_t current_stage = 0;

void log_heap(const char *stage, float fps = 0) {
    const auto network = heap_network_status();
    ESP_LOGI(tag, "%s free=%u largest=%u minimum=%u fps=%.2f",
             stage,
             static_cast<unsigned>(heap_caps_get_free_size(heap_caps)),
             static_cast<unsigned>(heap_caps_get_largest_free_block(heap_caps)),
             static_cast<unsigned>(heap_caps_get_minimum_free_size(heap_caps)), fps);
    const uint32_t psram_caps = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;
    const uint32_t dma_caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA;
    ESP_LOGI(tag, "%s psram_total=%u psram_free=%u psram_largest=%u psram_minimum=%u dma_free=%u dma_largest=%u",
             stage,
             static_cast<unsigned>(heap_caps_get_total_size(psram_caps)),
             static_cast<unsigned>(heap_caps_get_free_size(psram_caps)),
             static_cast<unsigned>(heap_caps_get_largest_free_block(psram_caps)),
             static_cast<unsigned>(heap_caps_get_minimum_free_size(psram_caps)),
             static_cast<unsigned>(heap_caps_get_free_size(dma_caps)),
             static_cast<unsigned>(heap_caps_get_largest_free_block(dma_caps)));
    ESP_LOGI(tag, "network wifi=%d web=%d ws=%d messages=%lu oversized=%lu reconnects=%lu json_errors=%lu",
             network.wifi_connected, network.web_started, network.ws_connected,
             static_cast<unsigned long>(network.messages),
             static_cast<unsigned long>(network.oversized_messages),
             static_cast<unsigned long>(network.reconnects),
             static_cast<unsigned long>(network.json_errors));
}

void write_selected(bool data_mode, const void *data, size_t length) {
    ESP_ERROR_CHECK(gpio_set_level(static_cast<gpio_num_t>(TFT_DC), data_mode ? 1 : 0));
    spi_transaction_t transaction{};
    transaction.length = length * 8;
    transaction.tx_buffer = data;
    ESP_ERROR_CHECK(spi_device_transmit(display_spi, &transaction));
}

void display_command(uint8_t command, const uint8_t *data = nullptr, size_t length = 0) {
    ESP_ERROR_CHECK(gpio_set_level(static_cast<gpio_num_t>(TFT_CS), 0));
    write_selected(false, &command, sizeof(command));
    if (length != 0) {
        write_selected(true, data, length);
    }
    ESP_ERROR_CHECK(gpio_set_level(static_cast<gpio_num_t>(TFT_CS), 1));
}

void initialize_display() {
    gpio_config_t outputs{};
    outputs.pin_bit_mask = (1ULL << TFT_DC) | (1ULL << TFT_CS) | (1ULL << TFT_RST);
    outputs.mode = GPIO_MODE_OUTPUT;
    ESP_ERROR_CHECK(gpio_config(&outputs));
    ESP_ERROR_CHECK(gpio_set_level(static_cast<gpio_num_t>(TFT_CS), 1));
    ESP_ERROR_CHECK(gpio_set_level(static_cast<gpio_num_t>(TFT_RST), 1));

    spi_bus_config_t bus{};
    bus.mosi_io_num = TFT_MOSI;
    bus.miso_io_num = -1;
    bus.sclk_io_num = TFT_SCLK;
    bus.quadwp_io_num = -1;
    bus.quadhd_io_num = -1;
    bus.data4_io_num = -1;
    bus.data5_io_num = -1;
    bus.data6_io_num = -1;
    bus.data7_io_num = -1;
    bus.max_transfer_sz = buffer_pixels * sizeof(lv_color_t);
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO));
    spi_device_interface_config_t device{};
    device.clock_speed_hz = SPI_FREQUENCY;
    device.mode = 0;
    device.spics_io_num = -1;
    device.queue_size = 1;
    device.flags = SPI_DEVICE_NO_DUMMY;
    ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &device, &display_spi));

    ESP_ERROR_CHECK(gpio_set_level(static_cast<gpio_num_t>(TFT_RST), 0));
    vTaskDelay(pdMS_TO_TICKS(20));
    ESP_ERROR_CHECK(gpio_set_level(static_cast<gpio_num_t>(TFT_RST), 1));
    vTaskDelay(pdMS_TO_TICKS(120));
    display_command(0x01);
    vTaskDelay(pdMS_TO_TICKS(150));

    static const struct {
        uint8_t command;
        uint8_t length;
        uint8_t data[12];
    } initialization[] = {
        {0xEF, 0, {0}},
        {0xEB, 1, {0x14}},
        {0xFE, 0, {0}},
        {0xEF, 0, {0}},
        {0xEB, 1, {0x14}},
        {0x84, 1, {0x40}},
        {0x85, 1, {0xFF}},
        {0x86, 1, {0xFF}},
        {0x87, 1, {0xFF}},
        {0x88, 1, {0x0A}},
        {0x89, 1, {0x21}},
        {0x8A, 1, {0x00}},
        {0x8B, 1, {0x80}},
        {0x8C, 1, {0x01}},
        {0x8D, 1, {0x01}},
        {0x8E, 1, {0xFF}},
        {0x8F, 1, {0xFF}},
        {0xB6, 2, {0x00, 0x00}},
        {0x36, 1, {0x48}},
        {0x3A, 1, {0x55}},
        {0x90, 4, {0x08, 0x08, 0x08, 0x08}},
        {0xBD, 1, {0x06}},
        {0xBC, 1, {0x00}},
        {0xFF, 3, {0x60, 0x01, 0x04}},
        {0xC3, 1, {0x13}},
        {0xC4, 1, {0x13}},
        {0xC9, 1, {0x22}},
        {0xBE, 1, {0x11}},
        {0xE1, 2, {0x10, 0x0E}},
        {0xDF, 3, {0x21, 0x0C, 0x02}},
        {0xF0, 6, {0x45, 0x09, 0x08, 0x08, 0x26, 0x2A}},
        {0xF1, 6, {0x43, 0x70, 0x72, 0x36, 0x37, 0x6F}},
        {0xF2, 6, {0x45, 0x09, 0x08, 0x08, 0x26, 0x2A}},
        {0xF3, 6, {0x43, 0x70, 0x72, 0x36, 0x37, 0x6F}},
        {0xED, 2, {0x1B, 0x0B}},
        {0xAE, 1, {0x77}},
        {0xCD, 1, {0x63}},
        {0x70, 9, {0x07, 0x07, 0x04, 0x0E, 0x0F, 0x09, 0x07, 0x08, 0x03}},
        {0xE8, 1, {0x34}},
        {0x62, 12, {0x18, 0x0D, 0x71, 0xED, 0x70, 0x70, 0x18, 0x0F, 0x71, 0xEF, 0x70, 0x70}},
        {0x63, 12, {0x18, 0x11, 0x71, 0xF1, 0x70, 0x70, 0x18, 0x13, 0x71, 0xF3, 0x70, 0x70}},
        {0x64, 7, {0x28, 0x29, 0xF1, 0x01, 0xF1, 0x00, 0x07}},
        {0x66, 10, {0x3C, 0x00, 0xCD, 0x67, 0x45, 0x45, 0x10, 0x00, 0x00, 0x00}},
        {0x67, 10, {0x00, 0x3C, 0x00, 0x00, 0x00, 0x01, 0x54, 0x10, 0x32, 0x98}},
        {0x74, 7, {0x10, 0x85, 0x80, 0x00, 0x00, 0x4E, 0x00}},
        {0x98, 2, {0x3E, 0x07}},
        {0x35, 0, {0}},
        {0x21, 0, {0}},
    };
    for (const auto &entry : initialization) {
        display_command(entry.command, entry.data, entry.length);
    }
    display_command(0x11);
    vTaskDelay(pdMS_TO_TICKS(120));
    display_command(0x29);
    vTaskDelay(pdMS_TO_TICKS(20));
    ESP_LOGI(tag, "Native IDF SPI2 GC9A01 initialized; net-clock sequence; MADCTL=0x48 RGB565=0x55");
}

void finish_flush(TickType_t timeout = 0) {
    if (pending_flush == nullptr) return;
    spi_transaction_t *finished_transfer = nullptr;
    const esp_err_t result = spi_device_get_trans_result(display_spi, &finished_transfer, timeout);
    if (result == ESP_ERR_TIMEOUT) return;
    ESP_ERROR_CHECK(result);
    ESP_ERROR_CHECK(finished_transfer == &pixel_transfer ? ESP_OK : ESP_FAIL);
    ESP_ERROR_CHECK(gpio_set_level(static_cast<gpio_num_t>(TFT_CS), 1));
    if (lv_disp_flush_is_last(pending_flush)) {
        ++completed_frames;
    }
    lv_disp_drv_t *finished = pending_flush;
    pending_flush = nullptr;
    lv_disp_flush_ready(finished);
}

void flush_display(lv_disp_drv_t *driver, const lv_area_t *area, lv_color_t *pixels) {
    finish_flush(portMAX_DELAY);
    const uint8_t columns[] = {static_cast<uint8_t>(area->x1 >> 8), static_cast<uint8_t>(area->x1),
                               static_cast<uint8_t>(area->x2 >> 8), static_cast<uint8_t>(area->x2)};
    const uint8_t rows[] = {static_cast<uint8_t>(area->y1 >> 8), static_cast<uint8_t>(area->y1),
                            static_cast<uint8_t>(area->y2 >> 8), static_cast<uint8_t>(area->y2)};
    display_command(0x2A, columns, sizeof(columns));
    display_command(0x2B, rows, sizeof(rows));
    ESP_ERROR_CHECK(gpio_set_level(static_cast<gpio_num_t>(TFT_CS), 0));
    const uint8_t memory_write = 0x2C;
    write_selected(false, &memory_write, sizeof(memory_write));
    ESP_ERROR_CHECK(gpio_set_level(static_cast<gpio_num_t>(TFT_DC), 1));
    pixel_transfer = {};
    pixel_transfer.length = (area->x2 - area->x1 + 1) * (area->y2 - area->y1 + 1)
                            * sizeof(lv_color_t) * 8;
    pixel_transfer.tx_buffer = pixels;
    ESP_ERROR_CHECK(spi_device_queue_trans(display_spi, &pixel_transfer, portMAX_DELAY));
    pending_flush = driver;
}

void wait_flush(lv_disp_drv_t *) {
    finish_flush(portMAX_DELAY);
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
    initialize_display();

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
        const auto network = heap_network_status();
        const bool network_ready = network.wifi_connected && network.web_started
                                   && network.ws_connected && network.messages != 0;
        if (now - last_stage >= stage_ms && (current_stage != 0 || network_ready)) {
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
    ESP_LOGI(tag, "IDF %s; LVGL %d.%d.%d; native IDF SPI display; reset=%d",
             esp_get_idf_version(), LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR,
             LVGL_VERSION_PATCH, static_cast<int>(esp_reset_reason()));
    esp_chip_info_t chip{};
    esp_chip_info(&chip);
    uint32_t flash_size = 0;
    if (esp_flash_get_size(nullptr, &flash_size) != ESP_OK
        || flash_size != 4 * 1024 * 1024
        || !esp_psram_is_initialized() || esp_psram_get_size() != 2 * 1024 * 1024) {
        ESP_LOGE(tag, "Hardware capacity check failed; expected 4 MB flash and 2 MB initialized PSRAM");
        return;
    }
    ESP_LOGI(tag, "ESP32-S3-Zero cores=%u revision=%u flash=%lu psram=%u cpu=%lu MHz; USB Serial/JTAG console",
             static_cast<unsigned>(chip.cores), static_cast<unsigned>(chip.revision),
             static_cast<unsigned long>(flash_size), static_cast<unsigned>(esp_psram_get_size()),
             static_cast<unsigned long>(getCpuFrequencyMhz()));
    ESP_LOGI(tag, "SCK%d MOSI%d DC%d CS%d RST%d; LVGL/GIF in PSRAM; DMA internal; RGB565 swapped; SPI40MHz",
             TFT_SCLK, TFT_MOSI, TFT_DC, TFT_CS, TFT_RST);
    log_heap("boot");
    if (xTaskCreatePinnedToCore(ui_task, "spike_ui", 8192, nullptr, 4, nullptr, 1) != pdPASS) {
        ESP_LOGE(tag, "UI task creation failed");
    }
    start_heap_network();
}
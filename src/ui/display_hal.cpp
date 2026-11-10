#include "display_hal.h"
#include "display_rotation.h"
#include "board_pins.h"

#include <driver/gpio.h>
#include <driver/spi_master.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <lvgl.h>

static_assert(LV_COLOR_DEPTH == 16 && LV_COLOR_16_SWAP == 1, "DMA requires swapped RGB565");
static_assert(LV_MEM_CUSTOM == 1, "LVGL must use explicit PSRAM allocation");

namespace {
constexpr unsigned buffer_pixels = board::width * 24;
spi_device_handle_t display_spi = nullptr;
spi_transaction_t pixel_transfer{};
lv_disp_draw_buf_t draw_buffers;
lv_disp_drv_t display_driver;
lv_disp_drv_t *pending_flush = nullptr;
bool initialized = false;
uint8_t current_rotation = 0;

void write_selected(bool data_mode, const void *data, size_t length)
{
    ESP_ERROR_CHECK(gpio_set_level(static_cast<gpio_num_t>(board::display_dc), data_mode ? 1 : 0));
    spi_transaction_t transaction{};
    transaction.length = length * 8;
    transaction.tx_buffer = data;
    ESP_ERROR_CHECK(spi_device_transmit(display_spi, &transaction));
}

void display_command(uint8_t command, const uint8_t *data = nullptr, size_t length = 0)
{
    ESP_ERROR_CHECK(gpio_set_level(static_cast<gpio_num_t>(board::display_cs), 0));
    write_selected(false, &command, sizeof(command));
    if (length != 0) write_selected(true, data, length);
    ESP_ERROR_CHECK(gpio_set_level(static_cast<gpio_num_t>(board::display_cs), 1));
}

void initialize_panel()
{
    gpio_config_t outputs{};
    outputs.pin_bit_mask = (1ULL << board::display_dc) | (1ULL << board::display_cs) | (1ULL << board::display_reset);
    outputs.mode = GPIO_MODE_OUTPUT;
    ESP_ERROR_CHECK(gpio_config(&outputs));
    ESP_ERROR_CHECK(gpio_set_level(static_cast<gpio_num_t>(board::display_cs), 1));
    ESP_ERROR_CHECK(gpio_set_level(static_cast<gpio_num_t>(board::display_reset), 1));

    spi_bus_config_t bus{};
    bus.mosi_io_num = board::display_mosi;
    bus.miso_io_num = -1;
    bus.sclk_io_num = board::display_sck;
    bus.quadwp_io_num = -1;
    bus.quadhd_io_num = -1;
    bus.data4_io_num = -1;
    bus.data5_io_num = -1;
    bus.data6_io_num = -1;
    bus.data7_io_num = -1;
    bus.max_transfer_sz = buffer_pixels * sizeof(lv_color_t);
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO));
    spi_device_interface_config_t device{};
    device.clock_speed_hz = board::display_hz;
    device.mode = 0;
    device.spics_io_num = -1;
    device.queue_size = 1;
    device.flags = SPI_DEVICE_NO_DUMMY;
    ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &device, &display_spi));

    ESP_ERROR_CHECK(gpio_set_level(static_cast<gpio_num_t>(board::display_reset), 0));
    vTaskDelay(pdMS_TO_TICKS(20));
    ESP_ERROR_CHECK(gpio_set_level(static_cast<gpio_num_t>(board::display_reset), 1));
    vTaskDelay(pdMS_TO_TICKS(120));
    display_command(0x01);
    vTaskDelay(pdMS_TO_TICKS(150));

    static const struct {
        uint8_t command;
        uint8_t length;
        uint8_t data[12];
    } initialization[] = {
        {0xEF, 0, {0}}, {0xEB, 1, {0x14}}, {0xFE, 0, {0}}, {0xEF, 0, {0}},
        {0xEB, 1, {0x14}}, {0x84, 1, {0x40}}, {0x85, 1, {0xFF}}, {0x86, 1, {0xFF}},
        {0x87, 1, {0xFF}}, {0x88, 1, {0x0A}}, {0x89, 1, {0x21}}, {0x8A, 1, {0x00}},
        {0x8B, 1, {0x80}}, {0x8C, 1, {0x01}}, {0x8D, 1, {0x01}}, {0x8E, 1, {0xFF}},
        {0x8F, 1, {0xFF}}, {0xB6, 2, {0x00, 0x00}}, {0x36, 1, {0x48}}, {0x3A, 1, {0x55}},
        {0x90, 4, {0x08, 0x08, 0x08, 0x08}}, {0xBD, 1, {0x06}}, {0xBC, 1, {0x00}},
        {0xFF, 3, {0x60, 0x01, 0x04}}, {0xC3, 1, {0x13}}, {0xC4, 1, {0x13}},
        {0xC9, 1, {0x22}}, {0xBE, 1, {0x11}}, {0xE1, 2, {0x10, 0x0E}}, {0xDF, 3, {0x21, 0x0C, 0x02}},
        {0xF0, 6, {0x45, 0x09, 0x08, 0x08, 0x26, 0x2A}},
        {0xF1, 6, {0x43, 0x70, 0x72, 0x36, 0x37, 0x6F}},
        {0xF2, 6, {0x45, 0x09, 0x08, 0x08, 0x26, 0x2A}},
        {0xF3, 6, {0x43, 0x70, 0x72, 0x36, 0x37, 0x6F}},
        {0xED, 2, {0x1B, 0x0B}}, {0xAE, 1, {0x77}}, {0xCD, 1, {0x63}},
        {0x70, 9, {0x07, 0x07, 0x04, 0x0E, 0x0F, 0x09, 0x07, 0x08, 0x03}},
        {0xE8, 1, {0x34}},
        {0x62, 12, {0x18, 0x0D, 0x71, 0xED, 0x70, 0x70, 0x18, 0x0F, 0x71, 0xEF, 0x70, 0x70}},
        {0x63, 12, {0x18, 0x11, 0x71, 0xF1, 0x70, 0x70, 0x18, 0x13, 0x71, 0xF3, 0x70, 0x70}},
        {0x64, 7, {0x28, 0x29, 0xF1, 0x01, 0xF1, 0x00, 0x07}},
        {0x66, 10, {0x3C, 0x00, 0xCD, 0x67, 0x45, 0x45, 0x10, 0x00, 0x00, 0x00}},
        {0x67, 10, {0x00, 0x3C, 0x00, 0x00, 0x00, 0x01, 0x54, 0x10, 0x32, 0x98}},
        {0x74, 7, {0x10, 0x85, 0x80, 0x00, 0x00, 0x4E, 0x00}},
        {0x98, 2, {0x3E, 0x07}}, {0x35, 0, {0}}, {0x21, 0, {0}},
    };
    for (const auto &entry : initialization) display_command(entry.command, entry.data, entry.length);
    display_command(0x11);
    vTaskDelay(pdMS_TO_TICKS(120));
    display_command(0x29);
    vTaskDelay(pdMS_TO_TICKS(20));
}

void finish_flush(TickType_t timeout = 0)
{
    if (pending_flush == nullptr) return;
    spi_transaction_t *finished_transfer = nullptr;
    esp_err_t result = spi_device_get_trans_result(display_spi, &finished_transfer, timeout);
    if (result == ESP_ERR_TIMEOUT) return;
    ESP_ERROR_CHECK(result);
    ESP_ERROR_CHECK(finished_transfer == &pixel_transfer ? ESP_OK : ESP_FAIL);
    ESP_ERROR_CHECK(gpio_set_level(static_cast<gpio_num_t>(board::display_cs), 1));
    lv_disp_drv_t *finished = pending_flush;
    pending_flush = nullptr;
    lv_disp_flush_ready(finished);
}

void flush_display(lv_disp_drv_t *driver, const lv_area_t *area, lv_color_t *pixels)
{
    finish_flush(portMAX_DELAY);
    const uint8_t columns[] = {static_cast<uint8_t>(area->x1 >> 8), static_cast<uint8_t>(area->x1),
                               static_cast<uint8_t>(area->x2 >> 8), static_cast<uint8_t>(area->x2)};
    const uint8_t rows[] = {static_cast<uint8_t>(area->y1 >> 8), static_cast<uint8_t>(area->y1),
                            static_cast<uint8_t>(area->y2 >> 8), static_cast<uint8_t>(area->y2)};
    display_command(0x2A, columns, sizeof(columns));
    display_command(0x2B, rows, sizeof(rows));
    ESP_ERROR_CHECK(gpio_set_level(static_cast<gpio_num_t>(board::display_cs), 0));
    const uint8_t memory_write = 0x2C;
    write_selected(false, &memory_write, sizeof(memory_write));
    ESP_ERROR_CHECK(gpio_set_level(static_cast<gpio_num_t>(board::display_dc), 1));
    pixel_transfer = {};
    pixel_transfer.length = (area->x2 - area->x1 + 1) * (area->y2 - area->y1 + 1) * sizeof(lv_color_t) * 8;
    pixel_transfer.tx_buffer = pixels;
    ESP_ERROR_CHECK(spi_device_queue_trans(display_spi, &pixel_transfer, portMAX_DELAY));
    pending_flush = driver;
}

void wait_flush(lv_disp_drv_t *)
{
    finish_flush(portMAX_DELAY);
}
}

bool display_hal::initialize(uint8_t rotation)
{
    if (!valid_rotation(rotation) || display_spi != nullptr) return false;
    initialize_panel();
    auto *first = static_cast<lv_color_t *>(heap_caps_malloc(
        buffer_pixels * sizeof(lv_color_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA));
    auto *second = static_cast<lv_color_t *>(heap_caps_malloc(
        buffer_pixels * sizeof(lv_color_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA));
    if (first == nullptr || second == nullptr) {
        heap_caps_free(first);
        heap_caps_free(second);
        ESP_LOGE("display", "Internal DMA buffer allocation failed");
        return false;
    }
    lv_init();
    lv_disp_draw_buf_init(&draw_buffers, first, second, buffer_pixels);
    lv_disp_drv_init(&display_driver);
    display_driver.hor_res = board::width;
    display_driver.ver_res = board::height;
    display_driver.draw_buf = &draw_buffers;
    display_driver.flush_cb = flush_display;
    display_driver.wait_cb = wait_flush;
    if (lv_disp_drv_register(&display_driver) == nullptr) {
        heap_caps_free(first);
        heap_caps_free(second);
        return false;
    }
    initialized = true;
    ESP_LOGI("display", "Native SPI2 GC9A01, GPIO1-5, 40 MHz, two internal 240x24 DMA strips");
    return set_rotation(rotation);
}

bool display_hal::set_rotation(uint8_t rotation)
{
    if (!initialized || !valid_rotation(rotation)) return false;
    if (rotation == current_rotation) return true;
    finish_flush(portMAX_DELAY);
    const uint8_t madctl = rotation_madctl(rotation);
    display_command(0x36, &madctl, sizeof(madctl));
    current_rotation = rotation;
    lv_obj_invalidate(lv_scr_act());
    return true;
}

void display_hal::poll()
{
    finish_flush();
}
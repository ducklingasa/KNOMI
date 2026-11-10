#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ui.h"

static lv_color_t draw_buffer[240 * 24];
static lv_color_t framebuffer[240 * 240];

LV_IMG_DECLARE(gif_welcome);
LV_IMG_DECLARE(gif_voron);

static void flush(lv_disp_drv_t *driver, const lv_area_t *area, lv_color_t *pixels)
{
    for (int row = area->y1; row <= area->y2; ++row) {
        for (int column = area->x1; column <= area->x2; ++column) {
            if (row >= 0 && row < 240 && column >= 0 && column < 240) {
                framebuffer[row * 240 + column] = *pixels;
            }
            ++pixels;
        }
    }
    lv_disp_flush_ready(driver);
}

static int save(const char *path)
{
    FILE *output = fopen(path, "wb");
    if (output == NULL) {
        perror(path);
        return 1;
    }
    fprintf(output, "P6\n240 240\n255\n");
    for (int index = 0; index < 240 * 240; ++index) {
        lv_color32_t color;
        color.full = lv_color_to32(framebuffer[index]);
        unsigned char rgb[] = {color.ch.red, color.ch.green, color.ch.blue};
        if (fwrite(rgb, sizeof(rgb), 1, output) != 1) {
            fclose(output);
            return 1;
        }
    }
    return fclose(output) != 0;
}

static void temperature(lv_obj_t *label, int value)
{
    char text[24];
    snprintf(text, sizeof(text), "%d\xE2\x84\x83", value);
    lv_label_set_text(label, text);
}

static lv_obj_t *create_screen(const char *name, int value)
{
    if (strcmp(name, "nozzle") == 0 || strcmp(name, "nozzle_zero") == 0) {
        ui_ScreenHeatingNozzle_screen_init();
        int target = strcmp(name, "nozzle_zero") == 0 ? 0 : 220;
        temperature(ui_label_heating_nozzle_actual, target ? value * target / 100 : 25);
        temperature(ui_label_heating_nozzle_target, target);
        if (target) lv_slider_set_value(ui_slider_heating_nozzle, value, LV_ANIM_OFF);
        return ui_ScreenHeatingNozzle;
    }
    if (strcmp(name, "bed") == 0 || strcmp(name, "bed_zero") == 0) {
        ui_ScreenHeatingBed_screen_init();
        int target = strcmp(name, "bed_zero") == 0 ? 0 : 60;
        temperature(ui_label_heating_bed_actual, target ? value * target / 100 : 25);
        temperature(ui_label_heating_bed_target, target);
        if (target) lv_slider_set_value(ui_slider_heating_bed, value, LV_ANIM_OFF);
        return ui_ScreenHeatingBed;
    }
    if (strcmp(name, "print_v1") == 0 || strcmp(name, "print_v2") == 0) {
        ui_ScreenPrinting_screen_init();
        lv_label_set_text_fmt(ui_label_printing_progress, "%d%%", value);
        lv_arc_set_value(ui_arc_printing_progress, value);
        if (strcmp(name, "print_v1") == 0) {
            lv_obj_set_y(ui_label_printing_progress, 0);
            lv_obj_set_align(ui_label_printing_progress, LV_ALIGN_CENTER);
            lv_obj_del(ui_slider_printing_acc_x);
            lv_obj_del(ui_slider_printing_acc_y);
            lv_obj_del(ui_slider_printing_acc_z);
            lv_obj_del(ui_label_printing_acc_x);
            lv_obj_del(ui_label_printing_acc_y);
            lv_obj_del(ui_label_printing_acc_z);
        }
        return ui_ScreenPrinting;
    }
    if (strcmp(name, "temperatures") == 0) {
        ui_ScreenTemp_screen_init();
        temperature(ui_label_temp_nozzle_actual, 210);
        temperature(ui_label_temp_nozzle_target, 220);
        temperature(ui_label_temp_bed_actual, 55);
        temperature(ui_label_temp_bed_target, 60);
        return ui_ScreenTemp;
    }
    if (strcmp(name, "main_gif") == 0) {
        ui_ScreenMainGif_screen_init();
        lv_obj_t *image = lv_gif_create(ui_ScreenMainGif);
        lv_gif_set_src(image, &gif_voron);
        lv_obj_center(image);
        return ui_ScreenMainGif;
    }
    if (strcmp(name, "welcome") == 0) {
        ui_ScreenWelcome_screen_init();
        lv_obj_t *image = lv_gif_create(ui_ScreenWelcome);
        lv_gif_set_src(image, &gif_welcome);
        lv_obj_align(image, LV_ALIGN_CENTER, 0, -36);
        return ui_ScreenWelcome;
    }
    if (strcmp(name, "wifi_connecting") == 0) {
        ui_ScreenWIFIConnecting_screen_init();
        return ui_ScreenWIFIConnecting;
    }
    if (strcmp(name, "wifi_lost") == 0) {
        ui_ScreenWIFIDisconnect_screen_init();
        return ui_ScreenWIFIDisconnect;
    }
    if (strcmp(name, "qr") == 0) {
        ui_ScreenQRCode_screen_init();
        lv_obj_t *code = lv_qrcode_create(ui_ScreenQRCode, 130, lv_color_white(), lv_color_black());
        const char *url = "https://bigtreetech.github.io/docs/KNOMI2.html";
        lv_qrcode_update(code, url, strlen(url));
        lv_obj_center(code);
        return ui_ScreenQRCode;
    }
    if (strcmp(name, "info") == 0) {
        ui_ScreenInfo_screen_init();
        lv_label_set_text(ui_label_sta_ip, "192.0.2.10");
        lv_label_set_text(ui_label_ap_ip, "192.0.2.1");
        lv_label_set_text(ui_label_local, "knomi.local");
        lv_label_set_text(ui_label_host, "192.0.2.55");
        return ui_ScreenInfo;
    }
    if (strcmp(name, "popup") == 0) {
        ui_ScreenPopup_screen_init();
        lv_textarea_set_text(ui_textarea_popup, "Reference notification");
        return ui_ScreenPopup;
    }
    if (strcmp(name, "dialog") == 0) {
        ui_ScreenDialog_screen_init();
        return ui_ScreenDialog;
    }
    return NULL;
}

int main(int argc, char **argv)
{
    if (argc != 5) {
        fprintf(stderr, "Usage: screen_reference SCREEN VALUE MILLISECONDS OUTPUT.ppm\n");
        return 1;
    }
    char *end;
    long value = strtol(argv[2], &end, 10);
    if (*end || value < 0 || value > 150) return 1;
    long milliseconds = strtol(argv[3], &end, 10);
    if (*end || milliseconds < 0 || milliseconds > 10000) return 1;
    lv_init();
    lv_disp_draw_buf_t buffers;
    lv_disp_draw_buf_init(&buffers, draw_buffer, NULL, 240 * 24);
    lv_disp_drv_t driver;
    lv_disp_drv_init(&driver);
    driver.hor_res = 240;
    driver.ver_res = 240;
    driver.draw_buf = &buffers;
    driver.flush_cb = flush;
    lv_disp_t *display = lv_disp_drv_register(&driver);
    lv_disp_set_theme(display, lv_theme_default_init(
        display, lv_palette_main(LV_PALETTE_BLUE), lv_palette_main(LV_PALETTE_RED),
        true, LV_FONT_DEFAULT));
    lv_obj_t *screen = create_screen(argv[1], (int)value);
    if (screen == NULL) {
        fprintf(stderr, "Unknown screen: %s\n", argv[1]);
        return 1;
    }
    lv_scr_load(screen);
    for (long elapsed = 0; elapsed < milliseconds; elapsed += 20) {
        lv_tick_inc(20);
        lv_timer_handler();
    }
    lv_refr_now(NULL);
    return save(argv[4]);
}
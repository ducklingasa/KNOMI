#include <stdio.h>

#include "splash.h"

static lv_color_t buffer[240 * 24];

static void discard_flush(lv_disp_drv_t *driver, const lv_area_t *area, lv_color_t *pixels)
{
    (void)area;
    (void)pixels;
    lv_disp_flush_ready(driver);
}

int main(int argc, char **argv)
{
    if (argc != 2) return 1;
    lv_init();
    lv_disp_draw_buf_t buffers;
    lv_disp_draw_buf_init(&buffers, buffer, NULL, 240 * 24);
    lv_disp_drv_t driver;
    lv_disp_drv_init(&driver);
    driver.hor_res = 240;
    driver.ver_res = 240;
    driver.draw_buf = &buffers;
    driver.flush_cb = discard_flush;
    lv_disp_t *display = lv_disp_drv_register(&driver);
    lv_disp_set_theme(display, lv_theme_default_init(display, lv_palette_main(LV_PALETTE_BLUE),
                                                    lv_palette_main(LV_PALETTE_RED), true, LV_FONT_DEFAULT));
    lv_obj_t *screen = knomi_splash_create();
    lv_scr_load(screen);
    lv_obj_update_layout(screen);
    if (lv_obj_get_child_cnt(screen) != 3) return 2;
    for (unsigned index = 0; index < 3; ++index) {
        lv_area_t bounds;
        lv_obj_get_coords(lv_obj_get_child(screen, index), &bounds);
        const int horizontal[] = {bounds.x1 - 120, bounds.x2 - 120};
        const int vertical[] = {bounds.y1 - 120, bounds.y2 - 120};
        for (unsigned column = 0; column < 2; ++column) {
            for (unsigned row = 0; row < 2; ++row) {
                if (horizontal[column] * horizontal[column] + vertical[row] * vertical[row] > 119 * 119) {
                    fprintf(stderr, "Splash label %u outside round safe area\n", index);
                    return 3;
                }
            }
        }
    }
    lv_img_dsc_t *image = lv_snapshot_take(screen, LV_IMG_CF_TRUE_COLOR);
    if (image == NULL || image->header.w != 240 || image->header.h != 240) return 4;
    FILE *output = fopen(argv[1], "wb");
    if (output == NULL) return 5;
    fprintf(output, "P6\n240 240\n255\n");
    const lv_color_t *pixels = (const lv_color_t *)image->data;
    for (unsigned index = 0; index < 240 * 240; ++index) {
        lv_color32_t color;
        color.full = lv_color_to32(pixels[index]);
        unsigned char rgb[] = {color.ch.red, color.ch.green, color.ch.blue};
        if (fwrite(rgb, sizeof(rgb), 1, output) != 1) return 6;
    }
    int result = fclose(output) != 0;
    lv_snapshot_free(image);
    return result;
}
#include "splash.h"
#include "app_config.h"

LV_FONT_DECLARE(ui_font_InterSemiBold14);
LV_FONT_DECLARE(ui_font_InterSemiBold18);
LV_FONT_DECLARE(ui_font_InterSemiBold48);

lv_obj_t *knomi_splash_create(void)
{
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(screen);
    lv_obj_set_style_text_font(title, &ui_font_InterSemiBold48, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0x329A40), 0);
    lv_label_set_text_static(title, "KNOMI");
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -28);

    lv_obj_t *edition = lv_label_create(screen);
    lv_obj_set_style_text_font(edition, &ui_font_InterSemiBold18, 0);
    lv_obj_set_style_text_color(edition, lv_color_white(), 0);
    lv_label_set_text_static(edition, "Creality edition");
    lv_obj_align(edition, LV_ALIGN_CENTER, 0, 27);

    lv_obj_t *version = lv_label_create(screen);
    lv_obj_set_style_text_font(version, &ui_font_InterSemiBold14, 0);
    lv_obj_set_style_text_color(version, lv_color_hex(0x808080), 0);
    lv_label_set_text_static(version, KNOMI_FW_VERSION);
    lv_obj_align(version, LV_ALIGN_BOTTOM_MID, 0, -35);
    return screen;
}
#include "lvgl.h"

static int ui_clicks;
static lv_obj_t *ui_clicks_label;

static void ui_on_click(lv_event_t *e)
{
    (void)e;
    ui_clicks++;
    lv_label_set_text_fmt(ui_clicks_label, "clicks: %d", ui_clicks);
}

static void ui_hello(void)
{
    lv_obj_t *s = lv_screen_active();
    lv_obj_set_flex_flow(s, LV_FLEX_FLOW_COLUMN);
    lv_obj_t *l = lv_label_create(s);
    lv_label_set_text(l, "hello world");
    lv_obj_t *b = lv_button_create(s);
    lv_obj_t *bl = lv_label_create(b);
    lv_label_set_text(bl, "Click me");
    ui_clicks_label = lv_label_create(s);
    lv_label_set_text(ui_clicks_label, "clicks: 0");
    lv_obj_add_event_cb(b, ui_on_click, LV_EVENT_CLICKED, NULL);
}

static void ui_dash(void)
{
    lv_obj_t *s = lv_screen_active();
    lv_obj_set_flex_flow(s, LV_FLEX_FLOW_COLUMN);
    for (int i = 0; i < 50; i++) {
        lv_obj_t *row = lv_obj_create(s);
        lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_t *l = lv_label_create(row);
        lv_label_set_text_fmt(l, "row %d", i);
        lv_obj_t *b = lv_button_create(row);
        lv_obj_t *bl = lv_label_create(b);
        lv_label_set_text(bl, "Open");
        lv_obj_t *c = lv_checkbox_create(row);
        lv_checkbox_set_text(c, "On");
        lv_obj_t *sl = lv_slider_create(row);
        lv_obj_set_width(sl, 200);
        lv_slider_set_range(sl, 0, 100);
    }
}

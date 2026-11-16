#ifndef GW_H
#define GW_H
#include <stdint.h>

int gw_init(int w, int h, double scale);
int gw_resize(int w, int h, double scale);
int gw_font_mem(const void *data, int n);
int gw_load(const char *json, int len);
const char *gw_error(void);
int gw_push(int type, double x, double y, int button, int dy, int key, int mods, const char *text);
int gw_frame(double ms);
int gw_width(void);
int gw_height(void);
uint8_t *gw_pixels(void);
uint32_t gw_hash(void);
int gw_cursor(void);
const char *gw_log_take(void);
const char *gw_clip_get(void);
int gw_clip_set(const char *s);
void *gw_alloc(int n);
void gw_free(void *p);
#endif

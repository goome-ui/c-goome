#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "goome.h"

static const char *const lines[] = {
    "goome \xF0\x9F\x98\x80 \xF0\x9F\x8E\x89 \xF0\x9F\x91\x8D\xF0\x9F\x8F\xBD \xF0\x9F\x91\xA8\xE2\x80\x8D\xF0\x9F\x91\xA9\xE2\x80\x8D\xF0\x9F\x91\xA7 \xF0\x9F\x87\xA9\xF0\x9F\x87\xAA #\xEF\xB8\x8F\xE2\x83\xA3 \xE2\x9D\xA4\xEF\xB8\x8F \xF0\x9F\x90\xB1",
    "\xD8\xA7\xD9\x84\xD8\xB3\xD9\x84\xD8\xA7\xD9\x85 \xD8\xB9\xD9\x84\xD9\x8A\xD9\x83\xD9\x85\xD8\x8C \xD9\x83\xD9\x8A\xD9\x81 \xD8\xAD\xD8\xA7\xD9\x84\xD9\x83\xD8\x9F",
    "\xD7\xA9\xD7\x9C\xD7\x95\xD7\x9D \xD7\xA2\xD7\x95\xD7\x9C\xD7\x9D, \xD7\x9E\xD7\x94 \xD7\xA9\xD7\x9C\xD7\x95\xD7\x9E\xD7\x9A?",
    "goome says \xD9\x85\xD8\xB1\xD8\xAD\xD8\xA8\xD8\xA7 (123) to \xD7\xA9\xD7\x9C\xD7\x95\xD7\x9D \xF0\x9F\x91\x8B",
    "\xD9\x84\xD8\xA7 \xD9\x84\xD8\xA3\xD9\x86 \xD9\x85\xD8\xAD\xD9\x85\xD8\xAF \xD9\x83\xD8\xAA\xD8\xA8 2024 [ok]",
};

static void key(goome *g, int k, int mods)
{
    goome_event e;
    memset(&e, 0, sizeof(e));
    e.type = GOOME_KEY, e.key = k, e.mods = mods;
    goome_push(g, &e);
}

static int save(goome *g, const char *path)
{
    goome_image f;
    if (goome_frame(g, &f) != GOOME_OK) return 1;
    uint32_t *out = malloc((size_t)f.w * f.h * 4);
    if (!out) return 1;
    for (int y = 0; y < f.h; y++)
        for (int x = 0; x < f.w; x++) {
            uint32_t s = f.px[(size_t)y * f.stride + x], a = s >> 24, v = 0xFF000000u;
            for (int k = 0; k < 24; k += 8) v |= (((s >> k) & 255) + (0x20 * (255 - a)) / 255) << k;
            out[(size_t)y * f.w + x] = v;
        }
    int e = goome_save_ppm(path, out, f.w, f.h, f.w);
    free(out);
    return e != GOOME_OK;
}

int main(int argc, char **argv)
{
    const char *shot = argc > 1 ? argv[1] : NULL;
    int err;
    goome_opts o = {0};
    o.mode = shot ? GOOME_MODE_OFFSCREEN : GOOME_MODE_WINDOW;
    o.width = 560, o.height = 600, o.scale = shot ? 2 : 0;
    o.window_flags = GOOME_WINDOW_RESIZABLE | GOOME_WINDOW_CENTER;
    o.title = "goome text";
    goome *g = goome_open(&o, &err);
    if (!g) {
        fprintf(stderr, "cant open: %s\n", goome_strerror(err));
        return 1;
    }
    goome_ui *u = goome_ui_new(g);
    if (!u) {
        goome_close(g);
        return 1;
    }
    goome_ui_theme(u)->anim_ms = shot ? 0 : goome_ui_theme(u)->anim_ms;
    goome_ui_cfg(u)->blink_ms = shot ? 0 : goome_ui_cfg(u)->blink_ms;
    int dir = GOOME_DIR_AUTO, run = 1, frame = 0;
    char name[128] = "\xD8\xA7\xD9\x84\xD8\xB9\xD8\xB1\xD8\xA8\xD9\x8A\xD8\xA9 abc \xF0\x9F\x98\x80";
    char notes[1024] = "\xD7\xA9\xD7\x9C\xD7\x95\xD7\x9D! this line starts in hebrew\nmixed: abc \xD8\xB9\xD8\xB1\xD8\xA8\xD9\x8A 42 \xF0\x9F\x91\x8D\xF0\x9F\x8F\xBD\n\xD9\x85\xD8\xB1\xD8\xAD\xD8\xA8\xD8\xA7 \xD8\xA8\xD9\x83\xD9\x85";
    while (run) {
        if (shot && frame == 3) {
            key(g, GOOME_K_END, 0);
            for (int i = 0; i < 6; i++) key(g, GOOME_K_RIGHT, GOOME_SHIFT);
        }
        goome_event ev;
        while (goome_poll(g, &ev, shot ? 0 : 16) > 0) {
            if (ev.type == GOOME_CLOSE || ev.type == GOOME_QUIT) run = 0;
            goome_ui_feed(u, &ev);
        }
        if (!goome_ui_begin(u, 0)) continue;
        goome_text_dir(dir);
        if (goome_ui_app(u, "goome text", 0)) {
            goome_ui_heading(u, "Emoji, Arabic, Hebrew");
            for (int i = 0; i < (int)(sizeof(lines) / sizeof(lines[0])); i++) goome_ui_label(u, "%s", lines[i]);
            goome_ui_separator(u);
            goome_ui_row(u, 3);
            goome_ui_radio(u, "Auto", &dir, GOOME_DIR_AUTO);
            goome_ui_radio(u, "Left to right", &dir, GOOME_DIR_LTR);
            goome_ui_radio(u, "Right to left", &dir, GOOME_DIR_RTL);
            goome_ui_row(u, 1);
            if (shot && frame == 2) goome_ui_set_focus(u);
            goome_ui_input(u, "Name", name, sizeof(name));
            goome_ui_text_edit(u, "notes", notes, sizeof(notes), 120, GOOME_EDIT_WRAP);
            goome_ui_app_end(u);
        }
        goome_ui_end(u);
        goome_text_dir(GOOME_DIR_AUTO);
        if (shot && ++frame == 6) run = 0;
    }
    int bad = shot ? save(g, shot) : 0;
    goome_ui_free(u);
    goome_close(g);
    return bad;
}

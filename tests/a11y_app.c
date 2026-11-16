#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "goome.h"

#ifdef __APPLE__
#include <dlfcn.h>
typedef void *ob;
static ob (*oc_cls)(const char *);
static void *(*oc_sel)(const char *);
static void (*oc_msg)(void);
static void (*oc_stret)(void);
static int self_bad;
static ob oc(ob o, const char *s) { return o ? ((ob(*)(ob, void *))oc_msg)(o, oc_sel(s)) : NULL; }
static ob oc1(ob o, const char *s, ob a) { return ((ob(*)(ob, void *, ob))oc_msg)(o, oc_sel(s), a); }
static const char *oc_txt(ob o) { return o ? ((const char *(*)(ob, void *))oc_msg)(oc(o, "description"), oc_sel("UTF8String")) : ""; }
static ob oc_str(const char *s) { return ((ob(*)(ob, void *, const char *))oc_msg)(oc_cls("NSString"), oc_sel("stringWithUTF8String:"), s); }

static ob self_walk(ob el, int depth, const char *role, const char *name)
{
    ob found = NULL;
    const char *r = oc_txt(oc(el, "accessibilityRole")), *t = oc_txt(oc(el, "accessibilityTitle")), *l = oc_txt(oc(el, "accessibilityLabel"));
    const char *nm = t && *t && strcmp(t, "(null)") ? t : l;
    if (role && !strcmp(r, role) && nm && !strcmp(nm, name)) found = el;
    if (!role) printf("%*s%s '%s' %s\n", depth * 2, "", r, nm, oc_txt(oc(el, "accessibilityValue")));
    ob kids = oc(el, "accessibilityChildren");
    unsigned long n = kids ? ((unsigned long (*)(ob, void *))oc_msg)(kids, oc_sel("count")) : 0;
    for (unsigned long i = 0; i < n && !found; i++) {
        ob c = ((ob(*)(ob, void *, unsigned long))oc_msg)(kids, oc_sel("objectAtIndex:"), i);
        ob f = self_walk(c, depth + 1, role, name);
        if (f) found = f;
    }
    return found;
}

static void self_need(int ok, const char *what)
{
    printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    self_bad += !ok;
}

static int self_step(int step)
{
    if (!oc_msg) {
        void *objc = dlopen("/usr/lib/libobjc.A.dylib", RTLD_NOW);
        if (!objc) return 0;
        void *p = dlsym(objc, "objc_getClass");
        memcpy(&oc_cls, &p, sizeof(p));
        p = dlsym(objc, "sel_registerName");
        memcpy(&oc_sel, &p, sizeof(p));
        p = dlsym(objc, "objc_msgSend");
        memcpy(&oc_msg, &p, sizeof(p));
        p = dlsym(objc, "objc_msgSend_stret");
        if (!p) p = dlsym(objc, "objc_msgSend");
        memcpy(&oc_stret, &p, sizeof(p));
    }
    ob app = oc(oc_cls("NSApplication"), "sharedApplication"), wins = oc(app, "windows"), view = NULL;
    unsigned long n = ((unsigned long (*)(ob, void *))oc_msg)(wins, oc_sel("count"));
    for (unsigned long i = 0; i < n && !view; i++) {
        ob w = ((ob(*)(ob, void *, unsigned long))oc_msg)(wins, oc_sel("objectAtIndex:"), i);
        if (!strcmp(oc_txt(oc(w, "title")), "goome a11y")) view = oc(w, "contentView");
    }
    if (!view) return self_need(0, "found the goome view"), 0;
    if (step == 0) {
        ob kids = oc(view, "accessibilityChildren");
        self_walk(view, 0, NULL, NULL);
        self_need(kids && ((unsigned long (*)(ob, void *))oc_msg)(kids, oc_sel("count")) > 0, "view has accessibility children");
        ob b = self_walk(view, 0, "AXButton", "Press me"), c = self_walk(view, 0, "AXCheckBox", "Dark mode");
        ob s = self_walk(view, 0, "AXSlider", "Volume"), t = self_walk(view, 0, "AXTextField", "Name"), p = self_walk(view, 0, "AXPopUpButton", "Fruit");
        self_need(b && c && s && t && p, "button, check box, slider, text field and popup by name");
        if (b) {
            typedef struct { double x, y, w, h; } rc;
#if defined(__aarch64__) || defined(__arm64__)
            rc r = ((rc(*)(ob, void *))oc_msg)(b, oc_sel("accessibilityFrame"));
#else
            rc r = ((rc(*)(ob, void *))oc_stret)(b, oc_sel("accessibilityFrame"));
#endif
            self_need(r.w > 10 && r.h > 10, "button frame has a size");
            self_need(((signed char (*)(ob, void *))oc_msg)(b, oc_sel("accessibilityPerformPress")) != 0, "press the button");
        }
        if (s) self_need(!strcmp(oc_txt(oc(s, "accessibilityValue")), "3"), "slider value 3");
        if (t) self_need(!strcmp(oc_txt(oc(t, "accessibilityValue")), "Bo"), "text field says Bo");
        if (c) ((signed char (*)(ob, void *))oc_msg)(c, oc_sel("accessibilityPerformPress"));
        if (s) oc1(s, "setAccessibilityValue:", ((ob(*)(ob, void *, double))oc_msg)(oc_cls("NSNumber"), oc_sel("numberWithDouble:"), 7.5));
        if (t) oc1(t, "setAccessibilityValue:", oc_str("Robin"));
        if (p) ((signed char (*)(ob, void *))oc_msg)(p, oc_sel("accessibilityPerformPress"));
    } else if (step == 1) {
        ob c = self_walk(view, 0, "AXCheckBox", "Dark mode"), s = self_walk(view, 0, "AXSlider", "Volume"), t = self_walk(view, 0, "AXTextField", "Name");
        self_need(c && !strcmp(oc_txt(oc(c, "accessibilityValue")), "1"), "check box is on now");
        self_need(s && !strcmp(oc_txt(oc(s, "accessibilityValue")), "7.5"), "slider reads 7.5 back");
        self_need(t && !strcmp(oc_txt(oc(t, "accessibilityValue")), "Robin"), "text field reads Robin back");
        ob f = self_walk(view, 0, "AXMenuItem", "Fig");
        self_need(f != NULL, "popup list is open with Fig in it");
        if (f) ((signed char (*)(ob, void *))oc_msg)(f, oc_sel("accessibilityPerformPress"));
    } else {
        ob q = self_walk(view, 0, "AXButton", "Quit");
        if (q) ((signed char (*)(ob, void *))oc_msg)(q, oc_sel("accessibilityPerformPress"));
        printf(self_bad ? "self FAILED\n" : "self ok\n");
    }
    return 1;
}
#endif

int main(int argc, char **argv)
{
    static const char *const fruits[] = {"Mango", "Kiwi", "Fig"};
    long secs = argc > 1 ? atol(argv[1]) : 30;
    int err;
    goome *g = goome_open_app("goome a11y", 420, 360, &err);
    if (!g) {
        printf("no window: %s\n", goome_strerror(err));
        return 1;
    }
    goome_ui *u = goome_ui_new(g);
    int dark = 0, fruit = 0, presses = 0, running = 1;
    double vol = 3;
    char name[64] = "Bo";
    long end = goome_now_ms() + secs * 1000;
    int frames = 0, self = getenv("GOOME_AX_SELF") != NULL;
    printf("ready %s\n", goome_backend_name(g));
    fflush(stdout);
    while (running && goome_now_ms() < end) {
        goome_event ev;
        while (goome_poll(g, &ev, 16) > 0) {
            if (ev.type == GOOME_QUIT || ev.type == GOOME_CLOSE) running = 0;
            goome_ui_feed(u, &ev);
        }
        if (!goome_ui_begin(u, 0)) continue;
        goome_ui_app(u, "goome a11y", 0);
        goome_ui_label(u, "Screen reader test");
        if (goome_ui_button(u, "Press me")) printf("pressed %d\n", ++presses);
        if (goome_ui_checkbox(u, "Dark mode", &dark)) printf("dark %d\n", dark);
        if (goome_ui_slider(u, "Volume", &vol, 0, 10, "%.1f")) printf("volume %.1f\n", vol);
        if (goome_ui_input(u, "Name", name, sizeof(name))) printf("name %s\n", name);
        goome_ui_combo(u, "Fruit", &fruit, fruits, 3);
        if (goome_ui_button(u, "Quit")) running = 0;
        goome_ui_app_end(u);
        goome_ui_end(u);
        frames++;
#ifdef __APPLE__
        if (self && (frames == 40 || frames == 80 || frames == 120)) self_step(frames / 40 - 1);
#else
        (void)self;
#endif
        fflush(stdout);
    }
    printf("bye fruit %d\n", fruit);
    goome_ui_free(u);
    goome_close(g);
    return 0;
}

#define _DEFAULT_SOURCE
#define _DARWIN_C_SOURCE
#include <dlfcn.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>

typedef const void *ref;
static struct {
    unsigned char (*trusted)(void);
    ref (*app)(int);
    int (*get)(ref, ref, ref *);
    int (*set)(ref, ref, ref);
    int (*act)(ref, ref);
    ref (*mkstr)(ref, const char *, unsigned);
    unsigned char (*cstr)(ref, char *, long, unsigned);
    unsigned long (*type)(ref);
    unsigned long (*strtype)(void);
    unsigned long (*arrtype)(void);
    unsigned long (*numtype)(void);
    long (*count)(ref);
    ref (*at)(ref, long);
    unsigned char (*numget)(ref, int, void *);
    ref (*num)(ref, int, const void *);
    void (*rel)(ref);
} F;

typedef struct {
    ref el;
    char role[64], name[128], value[128];
    double num;
} node;

static node nodes[512];
static int nn, bad;

static ref S(const char *s) { return F.mkstr(NULL, s, 0x08000100); }

static int text_of(ref el, const char *attr, char *out, int cap)
{
    ref v = NULL;
    out[0] = 0;
    if (F.get(el, S(attr), &v) || !v) return 0;
    int ok = 0;
    if (F.type(v) == F.strtype()) ok = F.cstr(v, out, cap, 0x08000100);
    else if (F.type(v) == F.numtype()) {
        double d = 0;
        F.numget(v, 13, &d);
        snprintf(out, (size_t)cap, "%g", d);
        ok = 1;
    }
    F.rel(v);
    return ok;
}

static void walk(ref el, int depth)
{
    if (nn >= 512 || depth > 12) return;
    node *n = &nodes[nn++];
    n->el = el;
    text_of(el, "AXRole", n->role, sizeof(n->role));
    if (!text_of(el, "AXTitle", n->name, sizeof(n->name)) || !n->name[0]) text_of(el, "AXDescription", n->name, sizeof(n->name));
    text_of(el, "AXValue", n->value, sizeof(n->value));
    printf("%*s%s '%s' %s\n", depth * 2, "", n->role, n->name, n->value);
    ref kids = NULL;
    if (F.get(el, S("AXChildren"), &kids) || !kids) return;
    if (F.type(kids) == F.arrtype())
        for (long i = 0; i < F.count(kids); i++) walk(F.at(kids, i), depth + 1);
}

static node *pick(const char *role, const char *name)
{
    for (int i = 0; i < nn; i++)
        if (!strcmp(nodes[i].role, role) && !strcmp(nodes[i].name, name)) return &nodes[i];
    return NULL;
}

static void need(int ok, const char *what)
{
    printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    bad += !ok;
}

static ref walk_app(int pid)
{
    ref app = F.app(pid), wins = NULL;
    nn = 0;
    for (int t = 0; t < 40; t++) {
        if (!F.get(app, S("AXWindows"), &wins) && wins && F.type(wins) == F.arrtype() && F.count(wins) > 0) break;
        wins = NULL;
        usleep(250000);
    }
    if (!wins) return NULL;
    walk(F.at(wins, 0), 0);
    return app;
}

int main(int argc, char **argv)
{
    if (argc < 3) return 2;
    void *ax = dlopen("/System/Library/Frameworks/ApplicationServices.framework/ApplicationServices", RTLD_NOW);
    void *cf = dlopen("/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation", RTLD_NOW);
    if (!ax || !cf) return printf("no frameworks\n"), 2;
    void *p;
#define L(lib, f, n) (p = dlsym(lib, n), memcpy(&F.f, &p, sizeof(p)))
    L(ax, trusted, "AXIsProcessTrusted"), L(ax, app, "AXUIElementCreateApplication"), L(ax, get, "AXUIElementCopyAttributeValue");
    L(ax, set, "AXUIElementSetAttributeValue"), L(ax, act, "AXUIElementPerformAction"), L(cf, mkstr, "CFStringCreateWithCString");
    L(cf, cstr, "CFStringGetCString"), L(cf, type, "CFGetTypeID"), L(cf, strtype, "CFStringGetTypeID"), L(cf, arrtype, "CFArrayGetTypeID");
    L(cf, numtype, "CFNumberGetTypeID"), L(cf, count, "CFArrayGetCount"), L(cf, at, "CFArrayGetValueAtIndex"), L(cf, numget, "CFNumberGetValue");
    L(cf, num, "CFNumberCreate"), L(cf, rel, "CFRelease");
#undef L
    int trusted = F.trusted();
    printf("AXIsProcessTrusted %d\n", trusted);
    int pid = fork();
    if (pid == 0) {
        int fd = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC, 0644);
        dup2(fd, 1), dup2(fd, 2);
        execl(argv[1], argv[1], "40", (char *)NULL);
        _exit(127);
    }
    sleep(2);
    ref app = walk_app(pid);
    if (!app) {
        printf("no ax windows for the app%s\n", trusted ? "" : " (this process is not trusted for accessibility)");
        kill(pid, SIGTERM);
        waitpid(pid, NULL, 0);
        return trusted ? 1 : 3;
    }
    node *btn = pick("AXButton", "Press me"), *chk = pick("AXCheckBox", "Dark mode"), *sld = pick("AXSlider", "Volume");
    node *ent = pick("AXTextField", "Name"), *cmb = pick("AXPopUpButton", "Fruit");
    need(pick("AXStaticText", "") != NULL || nn > 5, "tree has nodes");
    need(btn != NULL, "button by title");
    need(chk != NULL && !strcmp(chk->value, "0"), "check box is off");
    need(sld != NULL && !strcmp(sld->value, "3"), "slider value 3");
    need(ent != NULL && !strcmp(ent->value, "Bo"), "text field says Bo");
    need(cmb != NULL && !strcmp(cmb->value, "Mango"), "popup says Mango");
    if (btn) need(F.act(btn->el, S("AXPress")) == 0, "press the button");
    if (chk) F.act(chk->el, S("AXPress"));
    if (sld) {
        double d = 7.5;
        need(F.set(sld->el, S("AXValue"), F.num(NULL, 13, &d)) == 0, "set the slider");
    }
    if (ent) need(F.set(ent->el, S("AXValue"), S("Robin")) == 0, "set the text field");
    if (cmb) F.act(cmb->el, S("AXPress"));
    usleep(1200000);
    walk_app(pid);
    chk = pick("AXCheckBox", "Dark mode"), sld = pick("AXSlider", "Volume"), ent = pick("AXTextField", "Name");
    need(chk && !strcmp(chk->value, "1"), "check box is on now");
    need(sld && !strcmp(sld->value, "7.5"), "slider reads 7.5 back");
    need(ent && !strcmp(ent->value, "Robin"), "text field reads Robin back");
    node *fig = pick("AXMenuItem", "Fig");
    need(fig != NULL, "popup list is open with Fig in it");
    if (fig) F.act(fig->el, S("AXPress"));
    usleep(600000);
    node *q = pick("AXButton", "Quit");
    if (q) F.act(q->el, S("AXPress"));
    int st = 0;
    for (int t = 0; t < 50 && waitpid(pid, &st, WNOHANG) == 0; t++) usleep(200000);
    kill(pid, SIGTERM);
    waitpid(pid, NULL, WNOHANG);
    FILE *f = fopen(argv[2], "r");
    char log[4096] = "";
    if (f) log[fread(log, 1, sizeof(log) - 1, f)] = 0, fclose(f);
    printf("--- app said:\n%s", log);
    need(strstr(log, "pressed 1") != NULL, "app saw the press");
    need(strstr(log, "dark 1") != NULL, "app saw the toggle");
    need(strstr(log, "volume 7.5") != NULL, "app saw the slider value");
    need(strstr(log, "name Robin") != NULL, "app saw the new name");
    need(strstr(log, "bye fruit 2") != NULL, "app saw the popup pick");
    printf(bad ? "ax FAILED\n" : "ax ok\n");
    return bad ? 1 : 0;
}

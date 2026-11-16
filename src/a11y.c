#include "internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef GOOME_NO_UI
#if defined(GM_LINUX) || defined(_WIN32) || defined(__APPLE__)
#define AX_PLAT 1
int gm_axp_open(goome *g);
void gm_axp_event(goome *g, int ev, int i, int old);
int gm_axp_pump(goome *g);
void gm_axp_close(goome *g);
#else
static int gm_axp_open(goome *g) { return -1; }
static void gm_axp_event(goome *g, int ev, int i, int old) {}
static int gm_axp_pump(goome *g) { return -1; }
static void gm_axp_close(goome *g) {}
#endif

static uint32_t ax_mix(uint32_t id)
{
    id ^= id >> 16;
    id *= 0x7FEB352Du;
    id ^= id >> 15;
    return id;
}

static void ax_rehash(gm_ax *a)
{
    int need = 64;
    while (need < a->count * 2) need *= 2;
    if (need != a->hcap) {
        uint32_t *h = realloc(a->hash, sizeof(uint32_t) * (size_t)need);
        if (!h) {
            a->hcap = 0;
            return;
        }
        a->hash = h, a->hcap = need;
    }
    memset(a->hash, 0xFF, sizeof(uint32_t) * (size_t)a->hcap);
    for (int i = 0; i < a->count; i++) {
        uint32_t k = ax_mix(a->n[i].id) & (uint32_t)(a->hcap - 1);
        while (a->hash[k] != 0xFFFFFFFFu) k = (k + 1) & (uint32_t)(a->hcap - 1);
        a->hash[k] = (uint32_t)i;
    }
}

int gm_ax_find(const gm_ax *a, uint32_t id)
{
    if (!a || !id) return -1;
    if (!a->hcap) {
        for (int i = 0; i < a->count; i++)
            if (a->n[i].id == id) return i;
        return -1;
    }
    uint32_t k = ax_mix(id) & (uint32_t)(a->hcap - 1);
    while (a->hash[k] != 0xFFFFFFFFu) {
        if (a->n[a->hash[k]].id == id) return (int)a->hash[k];
        k = (k + 1) & (uint32_t)(a->hcap - 1);
    }
    return -1;
}

static int ax_pfind(const gm_ax *a, uint32_t id, int guess)
{
    if (guess >= 0 && guess < a->pcount && a->pn[guess].id == id) return guess;
    for (int i = 0; i < a->pcount; i++)
        if (a->pn[i].id == id) return i;
    return -1;
}

const char *gm_ax_str(const gm_ax *a, int i, int *len)
{
    if (!a || i < 0 || i >= a->count) {
        if (len) *len = 0;
        return "";
    }
    if (a->x[i].ntext > 0) {
        if (len) *len = a->x[i].ntext;
        return a->text + a->x[i].text;
    }
    if (len) *len = (int)strlen(a->n[i].value);
    return a->n[i].value;
}

int gm_ax_chars(const char *s, int n)
{
    int c = 0;
    for (int i = 0; i < n && s[i]; i++)
        if (((unsigned char)s[i] & 0xC0) != 0x80) c++;
    return c;
}

int gm_ax_bytes(const char *s, int n, int chars)
{
    int i = 0, c = 0;
    if (chars <= 0) return 0;
    while (i < n && s[i]) {
        if (((unsigned char)s[i] & 0xC0) != 0x80 && c++ == chars) return i;
        i++;
    }
    return i;
}

static int ax_grow(void **p, int *cap, int need, size_t sz)
{
    if (need <= *cap) return 1;
    int n = *cap ? *cap : 64;
    while (n < need) n *= 2;
    void *q = realloc(*p, sz * (size_t)n);
    if (!q) return 0;
    *p = q, *cap = n;
    return 1;
}

static int ax_same_text(const gm_ax *a, int i, int j)
{
    int an, bn;
    const char *s = gm_ax_str(a, i, &an);
    const char *t = a->pn[j].value;
    if (a->px[j].ntext > 0) t = a->ptext + a->px[j].text, bn = a->px[j].ntext;
    else bn = (int)strlen(t);
    return an == bn && !memcmp(s, t, (size_t)an);
}

static int ax_bridge(goome *g)
{
    gm_ax *a = g->ax;
    if (a->plat_state) return a->plat_state > 0;
    a->plat_state = -1;
    const char *e = gm_env("GOOME_A11Y");
    if (e && e[0] == '0') return 0;
    if (g->opts.mode == GOOME_MODE_OFFSCREEN || !g->ov || !strcmp(g->ov->name, "test") || !strcmp(g->ov->name, "offscreen")) return 0;
    if (gm_axp_open(g) == 0) a->plat_state = 1;
    return a->plat_state > 0;
}

void gm_ax_sync(goome *g, const goome_a11y_node *n, const gm_ax_x *x, int count, const char *text, int ntext, int active)
{
    if (!g) return;
    if (!g->ax) {
        g->ax = calloc(1, sizeof(gm_ax));
        if (!g->ax) return;
    }
    gm_ax *a = g->ax;
    goome_a11y_node *tn = a->pn;
    gm_ax_x *tx = a->px;
    char *tt = a->ptext;
    int tc = a->pcap, ttc = a->ptcap;
    a->pn = a->n, a->px = a->x, a->ptext = a->text, a->pcount = a->count, a->pcap = a->cap, a->pntext = a->ntext, a->ptcap = a->tcap, a->pactive = a->active;
    a->n = tn, a->x = tx, a->text = tt, a->cap = tc, a->tcap = ttc;
    if (count > a->cap) {
        int nc = a->cap ? a->cap : 64;
        while (nc < count) nc *= 2;
        goome_a11y_node *nn = realloc(a->n, sizeof(*nn) * (size_t)nc);
        if (nn) a->n = nn;
        gm_ax_x *nx = realloc(a->x, sizeof(*nx) * (size_t)nc);
        if (nx) a->x = nx;
        if (nn && nx) a->cap = nc;
        else count = 0, ntext = 0;
    }
    if (!ax_grow((void **)&a->text, &a->tcap, ntext + 1, 1)) count = 0, ntext = 0;
    if (count) memcpy(a->n, n, sizeof(*n) * (size_t)count), memcpy(a->x, x, sizeof(*x) * (size_t)count);
    if (ntext && a->text) memcpy(a->text, text, (size_t)ntext);
    if (a->text) a->text[ntext] = 0;
    a->count = count, a->ntext = ntext, a->active = active;
    ax_rehash(a);
    int first = !a->synced;
    a->synced = 1;
    if (!ax_bridge(g) || first) return;
    if (a->active != a->pactive) gm_axp_event(g, GM_AXE_ACTIVE, -1, a->pactive);
    int pf = -1, cf = -1;
    for (int j = 0; j < a->pcount; j++) {
        if (a->pn[j].states & GOOME_A11Y_FOCUSED) pf = j;
        if (gm_ax_find(a, a->pn[j].id) < 0) gm_axp_event(g, GM_AXE_REMOVE, j, 0);
    }
    for (int i = 0; i < a->count; i++) {
        goome_a11y_node *c = &a->n[i];
        if (c->states & GOOME_A11Y_FOCUSED) cf = i;
        int j = ax_pfind(a, c->id, i);
        if (j < 0) {
            gm_axp_event(g, GM_AXE_ADD, i, 0);
            continue;
        }
        goome_a11y_node *p = &a->pn[j];
        if (strcmp(c->label, p->label)) gm_axp_event(g, GM_AXE_NAME, i, 0);
        int ts = !ax_same_text(a, i, j);
        if (ts || c->num != p->num || c->min != p->min || c->max != p->max) gm_axp_event(g, GM_AXE_VALUE, i, 0);
        if (ts && (a->x[i].kind & GM_AXK_EDITABLE)) gm_axp_event(g, GM_AXE_TEXT, i, j);
        if (c->caret != p->caret) gm_axp_event(g, GM_AXE_CARET, i, 0);
        if ((c->states & ~GOOME_A11Y_FOCUSED) != (p->states & ~GOOME_A11Y_FOCUSED)) gm_axp_event(g, GM_AXE_STATE, i, p->states);
    }
    if (cf >= 0 && (pf < 0 || a->pn[pf].id != a->n[cf].id)) gm_axp_event(g, GM_AXE_FOCUS, cf, pf >= 0 ? (int)a->pn[pf].id : 0);
}

int gm_ax_act(goome *g, uint32_t id, const char *verb, const char *text)
{
    if (!g || !verb) return GOOME_EARG;
    goome_event e;
    memset(&e, 0, sizeof(e));
    e.type = GOOME_CONTROL;
    int n = snprintf(e.msg, sizeof(e.msg), "a11y %s %u", verb, (unsigned)id);
    if (text) {
        size_t tl = strlen(text);
        if (n > 0 && (size_t)n + 1 + tl < sizeof(e.msg)) snprintf(e.msg + n, sizeof(e.msg) - (size_t)n, " %s", text);
        else if (g->ax) {
            char *c = malloc(tl + 1);
            if (c) {
                memcpy(c, text, tl + 1);
                free(g->ax->long_text);
                g->ax->long_text = c, g->ax->long_id = id;
            }
        }
    }
    return gm_push(g, &e) ? GOOME_OK : GOOME_EAGAIN;
}

const char *gm_ax_long(goome *g, uint32_t id)
{
    if (!g || !g->ax || !g->ax->long_text || g->ax->long_id != id) return NULL;
    return g->ax->long_text;
}

int gm_ax_pump(goome *g)
{
    if (!g || !g->ax || g->ax->plat_state <= 0) return -1;
    return gm_axp_pump(g);
}

void gm_ax_close(goome *g)
{
    if (!g || !g->ax) return;
    gm_ax *a = g->ax;
    if (a->plat_state > 0) gm_axp_close(g);
    free(a->n), free(a->pn), free(a->x), free(a->px), free(a->text), free(a->ptext), free(a->hash), free(a->long_text);
    free(a);
    g->ax = NULL;
}
#else
int gm_ax_pump(goome *g) { return -1; }
void gm_ax_close(goome *g) {}
#endif

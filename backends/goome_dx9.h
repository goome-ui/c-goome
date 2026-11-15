#ifndef GOOME_DX9_H
#define GOOME_DX9_H

#include <d3d9.h>

#include "goome.h"

#ifdef __cplusplus
extern "C" {
#endif

int goome_dx9_init(goome *g, IDirect3DDevice9 *dev);
void goome_dx9_render(goome *g);
void goome_dx9_resize(goome *g, int w, int h, double scale);
void goome_dx9_shutdown(goome *g);

#ifdef __cplusplus
}
#endif

#endif

#if defined(GOOME_IMPL_DX9_IMPLEMENTATION) && !defined(GOOME_IMPL_DX9_DONE)
#define GOOME_IMPL_DX9_DONE

#include <stdlib.h>
#include <string.h>

#ifndef GM_COM
#ifdef __cplusplus
#define GM_COM(o, m, ...) (o)->m(__VA_ARGS__)
#define GM_COM0(o, m) (o)->m()
#define GM_REF(x) x
#else
#define GM_COM(o, m, ...) (o)->lpVtbl->m(o, __VA_ARGS__)
#define GM_COM0(o, m) (o)->lpVtbl->m(o)
#define GM_REF(x) &x
#endif
#define GM_REL(o) ((void)((o) && (GM_COM0(o, Release), (o) = NULL, 1)))
#endif

typedef struct {
    goome *g;
    IDirect3DDevice9 *dev;
    IDirect3DTexture9 *tex, *sys;
    int tw, th;
} gm_dx9;

static gm_dx9 gm_dx9_all[8];

static gm_dx9 *gm_dx9_find(goome *g)
{
    for (int i = 0; i < 8; i++)
        if (g && gm_dx9_all[i].g == g) return &gm_dx9_all[i];
    return NULL;
}

static void gm_dx9_drop_tex(gm_dx9 *d)
{
    GM_REL(d->tex);
    GM_REL(d->sys);
    d->tw = d->th = 0;
}

static int gm_dx9_upload(gm_dx9 *d, const goome_image *f, const goome_rect *r)
{
    RECT lr = {r->x, r->y, r->x + r->w, r->y + r->h};
    D3DLOCKED_RECT lk;
    if (FAILED(GM_COM(d->sys, LockRect, 0, &lk, &lr, 0))) return GOOME_EAGAIN;
    for (int y = 0; y < r->h; y++) memcpy((unsigned char *)lk.pBits + (size_t)y * lk.Pitch, f->px + (size_t)(r->y + y) * f->stride + r->x, (size_t)r->w * 4);
    GM_COM(d->sys, UnlockRect, 0);
    return FAILED(GM_COM(d->dev, UpdateTexture, (IDirect3DBaseTexture9 *)d->sys, (IDirect3DBaseTexture9 *)d->tex)) ? GOOME_EAGAIN : GOOME_OK;
}

static int gm_dx9_make_tex(gm_dx9 *d, const goome_image *f)
{
    gm_dx9_drop_tex(d);
    if (FAILED(GM_COM(d->dev, CreateTexture, (UINT)f->w, (UINT)f->h, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &d->tex, NULL)) ||
        FAILED(GM_COM(d->dev, CreateTexture, (UINT)f->w, (UINT)f->h, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &d->sys, NULL))) {
        gm_dx9_drop_tex(d);
        return GOOME_ENOMEM;
    }
    d->tw = f->w, d->th = f->h;
    goome_rect all = {0, 0, f->w, f->h};
    return gm_dx9_upload(d, f, &all);
}

int goome_dx9_init(goome *g, IDirect3DDevice9 *dev)
{
    if (!g || !dev) return GOOME_EARG;
    if (gm_dx9_find(g)) return GOOME_OK;
    gm_dx9 *d = NULL;
    for (int i = 0; i < 8 && !d; i++)
        if (!gm_dx9_all[i].g) d = &gm_dx9_all[i];
    if (!d) return GOOME_ENOMEM;
    memset(d, 0, sizeof(*d));
    d->dev = dev;
    GM_COM0(dev, AddRef);
    d->g = g;
    return GOOME_OK;
}

void goome_dx9_render(goome *g)
{
    gm_dx9 *d = gm_dx9_find(g);
    goome_image f;
    if (!d || goome_frame(g, &f) != GOOME_OK || f.w <= 0 || f.h <= 0) return;
    IDirect3DDevice9 *dev = d->dev;
    goome_rect r;
    int dirty = goome_frame_dirty(g, &r);
    if (!d->tex || f.w != d->tw || f.h != d->th) {
        if (gm_dx9_make_tex(d, &f) != GOOME_OK) return;
    } else if (dirty)
        gm_dx9_upload(d, &f, &r);

    IDirect3DStateBlock9 *saved = NULL;
    if (FAILED(GM_COM(dev, CreateStateBlock, D3DSBT_ALL, &saved))) return;
    D3DVIEWPORT9 vp = {0, 0, (DWORD)d->tw, (DWORD)d->th, 0, 1};
    IDirect3DSurface9 *rt = NULL;
    D3DSURFACE_DESC sd;
    if (SUCCEEDED(GM_COM(dev, GetRenderTarget, 0, &rt))) {
        if (SUCCEEDED(GM_COM(rt, GetDesc, &sd))) {
            if (vp.Width > sd.Width) vp.Width = sd.Width;
            if (vp.Height > sd.Height) vp.Height = sd.Height;
        }
        GM_REL(rt);
    }
    GM_COM(dev, SetViewport, &vp);
    GM_COM(dev, SetPixelShader, NULL);
    GM_COM(dev, SetVertexShader, NULL);
    GM_COM(dev, SetFVF, D3DFVF_XYZRHW | D3DFVF_TEX1);
    static const DWORD rs[][2] = {
        {D3DRS_FILLMODE, D3DFILL_SOLID}, {D3DRS_SHADEMODE, D3DSHADE_GOURAUD}, {D3DRS_CULLMODE, D3DCULL_NONE},
        {D3DRS_LIGHTING, FALSE}, {D3DRS_ZENABLE, FALSE}, {D3DRS_ZWRITEENABLE, FALSE}, {D3DRS_STENCILENABLE, FALSE},
        {D3DRS_ALPHATESTENABLE, FALSE}, {D3DRS_ALPHABLENDENABLE, TRUE}, {D3DRS_BLENDOP, D3DBLENDOP_ADD},
        {D3DRS_SRCBLEND, D3DBLEND_ONE}, {D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA}, {D3DRS_SEPARATEALPHABLENDENABLE, TRUE},
        {D3DRS_BLENDOPALPHA, D3DBLENDOP_ADD}, {D3DRS_SRCBLENDALPHA, D3DBLEND_ONE}, {D3DRS_DESTBLENDALPHA, D3DBLEND_INVSRCALPHA},
        {D3DRS_SCISSORTESTENABLE, FALSE}, {D3DRS_FOGENABLE, FALSE}, {D3DRS_RANGEFOGENABLE, FALSE}, {D3DRS_SPECULARENABLE, FALSE},
        {D3DRS_CLIPPING, TRUE}, {D3DRS_CLIPPLANEENABLE, 0}, {D3DRS_COLORWRITEENABLE, 0xF}, {D3DRS_SRGBWRITEENABLE, FALSE},
        {D3DRS_TEXTUREFACTOR, 0xFFFFFFFFu}, {D3DRS_MULTISAMPLEANTIALIAS, FALSE},
    };
    for (size_t i = 0; i < sizeof(rs) / sizeof(rs[0]); i++) GM_COM(dev, SetRenderState, (D3DRENDERSTATETYPE)rs[i][0], rs[i][1]);
    GM_COM(dev, SetTexture, 0, (IDirect3DBaseTexture9 *)d->tex);
    GM_COM(dev, SetTextureStageState, 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    GM_COM(dev, SetTextureStageState, 0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    GM_COM(dev, SetTextureStageState, 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
    GM_COM(dev, SetTextureStageState, 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    GM_COM(dev, SetTextureStageState, 0, D3DTSS_TEXCOORDINDEX, 0);
    GM_COM(dev, SetTextureStageState, 0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
    GM_COM(dev, SetTextureStageState, 1, D3DTSS_COLOROP, D3DTOP_DISABLE);
    GM_COM(dev, SetTextureStageState, 1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
    GM_COM(dev, SetSamplerState, 0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
    GM_COM(dev, SetSamplerState, 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
    GM_COM(dev, SetSamplerState, 0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
    GM_COM(dev, SetSamplerState, 0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
    GM_COM(dev, SetSamplerState, 0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
    GM_COM(dev, SetSamplerState, 0, D3DSAMP_SRGBTEXTURE, FALSE);
    float x1 = (float)d->tw - 0.5f, y1 = (float)d->th - 0.5f;
    const float quad[4][6] = {
        {-0.5f, -0.5f, 0, 1, 0, 0}, {x1, -0.5f, 0, 1, 1, 0}, {-0.5f, y1, 0, 1, 0, 1}, {x1, y1, 0, 1, 1, 1},
    };
    GM_COM(dev, DrawPrimitiveUP, D3DPT_TRIANGLESTRIP, 2, quad, sizeof(quad[0]));
    GM_COM0(saved, Apply);
    GM_REL(saved);
}

void goome_dx9_resize(goome *g, int w, int h, double scale)
{
    gm_dx9 *d = gm_dx9_find(g);
    if (goome_resize(g, w, h, scale) == GOOME_OK && d) gm_dx9_drop_tex(d);
}

void goome_dx9_shutdown(goome *g)
{
    gm_dx9 *d = gm_dx9_find(g);
    if (!d) return;
    gm_dx9_drop_tex(d);
    GM_REL(d->dev);
    memset(d, 0, sizeof(*d));
}

#endif

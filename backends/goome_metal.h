#ifndef GOOME_METAL_H
#define GOOME_METAL_H

#include "goome.h"

#ifdef __cplusplus
extern "C" {
#endif

int goome_metal_init(goome *g, void *device);
void goome_metal_render(goome *g, void *pass, void *encoder);
void goome_metal_resize(goome *g, int w, int h, double scale);
void goome_metal_shutdown(goome *g);

#ifdef __cplusplus
}
#endif

#endif

#if defined(GOOME_IMPL_METAL_IMPLEMENTATION) && !defined(GOOME_IMPL_METAL_DONE) && defined(__APPLE__)
#define GOOME_IMPL_METAL_DONE

#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>

#if defined(__aarch64__) || defined(__arm64__)
typedef _Bool gm_mtl_bool;
#else
typedef signed char gm_mtl_bool;
#endif

typedef void *gm_id;
typedef unsigned long gm_uint;
typedef struct {
    gm_uint x, y, z, w, h, d;
} gm_mtl_region;
typedef struct {
    double x, y, w, h, znear, zfar;
} gm_mtl_viewport;
typedef struct {
    gm_uint x, y, w, h;
} gm_mtl_scissor;

#define GM_MTL_BGRA8 80

static const char gm_mtl_src[] =
    "#include <metal_stdlib>\n"
    "using namespace metal;\n"
    "struct gm_v { float4 p [[position]]; };\n"
    "vertex gm_v gm_vs(uint id [[vertex_id]]) {\n"
    "    float2 uv = float2(id & 1, id >> 1);\n"
    "    gm_v o;\n"
    "    o.p = float4(uv.x * 2 - 1, 1 - uv.y * 2, 0, 1);\n"
    "    return o;\n"
    "}\n"
    "fragment float4 gm_fs(gm_v i [[stage_in]], texture2d<float> t [[texture(0)]]) {\n"
    "    return t.read(uint2(i.p.xy));\n"
    "}\n";

static struct {
    int loaded, ok;
    gm_id (*cls)(const char *);
    void *(*sel)(const char *);
    void (*msg)(void);
    void *(*push)(void);
    void (*pop)(void *);
} gm_mtl_rt;

#define GM_SEL(s) gm_mtl_rt.sel(s)
#define GM_MSG0(ret) ((ret(*)(gm_id, void *))gm_mtl_rt.msg)
#define GM_MSG(ret, ...) ((ret(*)(gm_id, void *, __VA_ARGS__))gm_mtl_rt.msg)

static int gm_mtl_load(void)
{
    if (gm_mtl_rt.loaded) return gm_mtl_rt.ok;
    gm_mtl_rt.loaded = 1;
    void *objc = dlopen("/usr/lib/libobjc.A.dylib", RTLD_NOW | RTLD_GLOBAL);
    dlopen("/System/Library/Frameworks/Foundation.framework/Foundation", RTLD_NOW | RTLD_GLOBAL);
    dlopen("/System/Library/Frameworks/Metal.framework/Metal", RTLD_NOW | RTLD_GLOBAL);
    if (!objc) return 0;
    void *p;
    p = dlsym(objc, "objc_getClass"), memcpy(&gm_mtl_rt.cls, &p, sizeof(p));
    p = dlsym(objc, "sel_registerName"), memcpy(&gm_mtl_rt.sel, &p, sizeof(p));
    p = dlsym(objc, "objc_msgSend"), memcpy(&gm_mtl_rt.msg, &p, sizeof(p));
    p = dlsym(objc, "objc_autoreleasePoolPush"), memcpy(&gm_mtl_rt.push, &p, sizeof(p));
    p = dlsym(objc, "objc_autoreleasePoolPop"), memcpy(&gm_mtl_rt.pop, &p, sizeof(p));
    gm_mtl_rt.ok = gm_mtl_rt.cls && gm_mtl_rt.sel && gm_mtl_rt.msg && gm_mtl_rt.push && gm_mtl_rt.pop;
    return gm_mtl_rt.ok;
}

static gm_id gm_mtl_new(const char *cls)
{
    gm_id o = GM_MSG0(gm_id)(gm_mtl_rt.cls(cls), GM_SEL("alloc"));
    return o ? GM_MSG0(gm_id)(o, GM_SEL("init")) : NULL;
}

static void gm_mtl_release(gm_id o)
{
    if (o) GM_MSG0(void)(o, GM_SEL("release"));
}

static gm_id gm_mtl_get(gm_id o, const char *s) { return o ? GM_MSG0(gm_id)(o, GM_SEL(s)) : NULL; }
static gm_uint gm_mtl_uint(gm_id o, const char *s) { return o ? GM_MSG0(gm_uint)(o, GM_SEL(s)) : 0; }
static void gm_mtl_set(gm_id o, const char *s, gm_uint v) { GM_MSG(void, gm_uint)(o, GM_SEL(s), v); }
static void gm_mtl_seto(gm_id o, const char *s, gm_id v) { GM_MSG(void, gm_id)(o, GM_SEL(s), v); }
static gm_id gm_mtl_at(gm_id arr, gm_uint i) { return arr ? GM_MSG(gm_id, gm_uint)(arr, GM_SEL("objectAtIndexedSubscript:"), i) : NULL; }

typedef struct {
    gm_uint color, depth, stencil, samples;
    gm_id pso;
} gm_mtl_pipe;

typedef struct {
    gm_id tex;
    int dirty, x0, y0, x1, y1;
} gm_mtl_tex;

typedef struct {
    goome *g;
    gm_id dev, vs, fs, ds;
    gm_mtl_pipe pipes[4];
    gm_mtl_tex tex[3];
    int cur, tw, th;
} gm_mtl;

static gm_mtl gm_mtl_all[8];

static gm_mtl *gm_mtl_find(goome *g)
{
    for (int i = 0; i < 8; i++)
        if (g && gm_mtl_all[i].g == g) return &gm_mtl_all[i];
    return NULL;
}

static void gm_mtl_drop_tex(gm_mtl *d)
{
    for (int i = 0; i < 3; i++) gm_mtl_release(d->tex[i].tex);
    memset(d->tex, 0, sizeof(d->tex));
    d->tw = d->th = 0;
}

static int gm_mtl_make_tex(gm_mtl *d, int w, int h)
{
    gm_mtl_drop_tex(d);
    gm_id td = gm_mtl_new("MTLTextureDescriptor");
    if (!td) return GOOME_ENOMEM;
    gm_mtl_set(td, "setPixelFormat:", GM_MTL_BGRA8);
    gm_mtl_set(td, "setWidth:", (gm_uint)w);
    gm_mtl_set(td, "setHeight:", (gm_uint)h);
    gm_mtl_set(td, "setUsage:", 1);
    int ok = 1;
    for (int i = 0; i < 3; i++) {
        d->tex[i].tex = GM_MSG(gm_id, gm_id)(d->dev, GM_SEL("newTextureWithDescriptor:"), td);
        d->tex[i].dirty = 1, d->tex[i].x0 = d->tex[i].y0 = 0, d->tex[i].x1 = w, d->tex[i].y1 = h;
        ok &= d->tex[i].tex != NULL;
    }
    gm_mtl_release(td);
    if (!ok) {
        gm_mtl_drop_tex(d);
        return GOOME_ENOMEM;
    }
    d->tw = w, d->th = h, d->cur = 0;
    return GOOME_OK;
}

static gm_id gm_mtl_pipeline(gm_mtl *d, gm_uint color, gm_uint depth, gm_uint stencil, gm_uint samples)
{
    for (int i = 0; i < 4; i++) {
        gm_mtl_pipe *p = &d->pipes[i];
        if (p->pso && p->color == color && p->depth == depth && p->stencil == stencil && p->samples == samples) return p->pso;
    }
    gm_id pd = gm_mtl_new("MTLRenderPipelineDescriptor");
    if (!pd) return NULL;
    gm_mtl_seto(pd, "setVertexFunction:", d->vs);
    gm_mtl_seto(pd, "setFragmentFunction:", d->fs);
    gm_id ca = gm_mtl_at(gm_mtl_get(pd, "colorAttachments"), 0);
    gm_mtl_set(ca, "setPixelFormat:", color);
    GM_MSG(void, gm_mtl_bool)(ca, GM_SEL("setBlendingEnabled:"), 1);
    gm_mtl_set(ca, "setSourceRGBBlendFactor:", 1);
    gm_mtl_set(ca, "setSourceAlphaBlendFactor:", 1);
    gm_mtl_set(ca, "setDestinationRGBBlendFactor:", 5);
    gm_mtl_set(ca, "setDestinationAlphaBlendFactor:", 5);
    gm_mtl_set(pd, "setDepthAttachmentPixelFormat:", depth);
    gm_mtl_set(pd, "setStencilAttachmentPixelFormat:", stencil);
    gm_mtl_set(pd, "setRasterSampleCount:", samples ? samples : 1);
    gm_id err = NULL;
    gm_id pso = GM_MSG(gm_id, gm_id, gm_id *)(d->dev, GM_SEL("newRenderPipelineStateWithDescriptor:error:"), pd, &err);
    gm_mtl_release(pd);
    if (!pso) return NULL;
    gm_mtl_pipe *slot = &d->pipes[3];
    for (int i = 0; i < 4; i++)
        if (!d->pipes[i].pso) {
            slot = &d->pipes[i];
            break;
        }
    gm_mtl_release(slot->pso);
    slot->color = color, slot->depth = depth, slot->stencil = stencil, slot->samples = samples, slot->pso = pso;
    return pso;
}

int goome_metal_init(goome *g, void *device)
{
    if (!g || !device) return GOOME_EARG;
    if (!gm_mtl_load()) return GOOME_ENOTSUP;
    if (gm_mtl_find(g)) return GOOME_OK;
    gm_mtl *d = NULL;
    for (int i = 0; i < 8 && !d; i++)
        if (!gm_mtl_all[i].g) d = &gm_mtl_all[i];
    if (!d) return GOOME_ENOMEM;
    memset(d, 0, sizeof(*d));
    void *pool = gm_mtl_rt.push();
    gm_id src = GM_MSG(gm_id, const char *)(GM_MSG0(gm_id)(gm_mtl_rt.cls("NSString"), GM_SEL("alloc")), GM_SEL("initWithUTF8String:"), gm_mtl_src);
    gm_id err = NULL;
    gm_id lib = src ? GM_MSG(gm_id, gm_id, gm_id, gm_id *)(device, GM_SEL("newLibraryWithSource:options:error:"), src, NULL, &err) : NULL;
    gm_mtl_release(src);
    gm_id vn = GM_MSG(gm_id, const char *)(GM_MSG0(gm_id)(gm_mtl_rt.cls("NSString"), GM_SEL("alloc")), GM_SEL("initWithUTF8String:"), "gm_vs");
    gm_id fn = GM_MSG(gm_id, const char *)(GM_MSG0(gm_id)(gm_mtl_rt.cls("NSString"), GM_SEL("alloc")), GM_SEL("initWithUTF8String:"), "gm_fs");
    if (lib) {
        d->vs = GM_MSG(gm_id, gm_id)(lib, GM_SEL("newFunctionWithName:"), vn);
        d->fs = GM_MSG(gm_id, gm_id)(lib, GM_SEL("newFunctionWithName:"), fn);
    }
    gm_mtl_release(vn);
    gm_mtl_release(fn);
    gm_mtl_release(lib);
    gm_id dd = gm_mtl_new("MTLDepthStencilDescriptor");
    if (dd) {
        gm_mtl_set(dd, "setDepthCompareFunction:", 7);
        GM_MSG(void, gm_mtl_bool)(dd, GM_SEL("setDepthWriteEnabled:"), 0);
        d->ds = GM_MSG(gm_id, gm_id)(device, GM_SEL("newDepthStencilStateWithDescriptor:"), dd);
        gm_mtl_release(dd);
    }
    gm_mtl_rt.pop(pool);
    if (!d->vs || !d->fs || !d->ds) {
        gm_mtl_release(d->vs), gm_mtl_release(d->fs), gm_mtl_release(d->ds);
        memset(d, 0, sizeof(*d));
        return GOOME_ENOTSUP;
    }
    d->dev = GM_MSG0(gm_id)(device, GM_SEL("retain"));
    d->g = g;
    return GOOME_OK;
}

void goome_metal_render(goome *g, void *pass, void *encoder)
{
    gm_mtl *d = gm_mtl_find(g);
    goome_image f;
    if (!d || !pass || !encoder || goome_frame(g, &f) != GOOME_OK || f.w <= 0 || f.h <= 0) return;
    goome_rect r;
    int dirty = goome_frame_dirty(g, &r);
    if ((!d->tex[0].tex || f.w != d->tw || f.h != d->th) && gm_mtl_make_tex(d, f.w, f.h) != GOOME_OK) return;
    if (dirty)
        for (int i = 0; i < 3; i++) {
            gm_mtl_tex *t = &d->tex[i];
            if (!t->dirty) t->dirty = 1, t->x0 = r.x, t->y0 = r.y, t->x1 = r.x + r.w, t->y1 = r.y + r.h;
            else {
                if (r.x < t->x0) t->x0 = r.x;
                if (r.y < t->y0) t->y0 = r.y;
                if (r.x + r.w > t->x1) t->x1 = r.x + r.w;
                if (r.y + r.h > t->y1) t->y1 = r.y + r.h;
            }
        }
    d->cur = (d->cur + 1) % 3;
    gm_mtl_tex *t = &d->tex[d->cur];
    if (t->dirty) {
        gm_mtl_region rg = {(gm_uint)t->x0, (gm_uint)t->y0, 0, (gm_uint)(t->x1 - t->x0), (gm_uint)(t->y1 - t->y0), 1};
        GM_MSG(void, gm_mtl_region, gm_uint, const void *, gm_uint)(t->tex, GM_SEL("replaceRegion:mipmapLevel:withBytes:bytesPerRow:"), rg, 0,
                                                                   f.px + (size_t)t->y0 * f.stride + t->x0, (gm_uint)f.stride * 4);
        t->dirty = 0;
    }

    void *pool = gm_mtl_rt.push();
    gm_id ca = gm_mtl_at(gm_mtl_get(pass, "colorAttachments"), 0);
    gm_id ct = gm_mtl_get(ca, "texture");
    gm_id dt = gm_mtl_get(gm_mtl_get(pass, "depthAttachment"), "texture");
    gm_id st = gm_mtl_get(gm_mtl_get(pass, "stencilAttachment"), "texture");
    gm_id pso = ct ? gm_mtl_pipeline(d, gm_mtl_uint(ct, "pixelFormat"), gm_mtl_uint(dt, "pixelFormat"), gm_mtl_uint(st, "pixelFormat"), gm_mtl_uint(ct, "sampleCount")) : NULL;
    if (pso) {
        gm_uint rw = gm_mtl_uint(ct, "width"), rh = gm_mtl_uint(ct, "height");
        gm_mtl_viewport vp = {0, 0, (double)d->tw, (double)d->th, 0, 1};
        gm_mtl_scissor sc = {0, 0, (gm_uint)d->tw < rw ? (gm_uint)d->tw : rw, (gm_uint)d->th < rh ? (gm_uint)d->th : rh};
        gm_mtl_seto(encoder, "setRenderPipelineState:", pso);
        if (dt || st) gm_mtl_seto(encoder, "setDepthStencilState:", d->ds);
        GM_MSG(void, gm_mtl_viewport)(encoder, GM_SEL("setViewport:"), vp);
        GM_MSG(void, gm_mtl_scissor)(encoder, GM_SEL("setScissorRect:"), sc);
        gm_mtl_set(encoder, "setCullMode:", 0);
        gm_mtl_set(encoder, "setTriangleFillMode:", 0);
        GM_MSG(void, gm_id, gm_uint)(encoder, GM_SEL("setFragmentTexture:atIndex:"), t->tex, 0);
        GM_MSG(void, gm_uint, gm_uint, gm_uint)(encoder, GM_SEL("drawPrimitives:vertexStart:vertexCount:"), 4, 0, 4);
    }
    gm_mtl_rt.pop(pool);
}

void goome_metal_resize(goome *g, int w, int h, double scale)
{
    gm_mtl *d = gm_mtl_find(g);
    if (goome_resize(g, w, h, scale) == GOOME_OK && d) gm_mtl_drop_tex(d);
}

void goome_metal_shutdown(goome *g)
{
    gm_mtl *d = gm_mtl_find(g);
    if (!d) return;
    gm_mtl_drop_tex(d);
    for (int i = 0; i < 4; i++) gm_mtl_release(d->pipes[i].pso);
    gm_mtl_release(d->vs), gm_mtl_release(d->fs), gm_mtl_release(d->ds), gm_mtl_release(d->dev);
    memset(d, 0, sizeof(*d));
}

#endif

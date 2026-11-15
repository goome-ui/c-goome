#ifndef GOOME_GL2_H
#define GOOME_GL2_H

#include "goome.h"

#ifndef GOOME_GL_PROC_DEFINED
#define GOOME_GL_PROC_DEFINED
typedef void (*goome_gl_proc)(void);
typedef goome_gl_proc (*goome_gl_loader)(const char *name);
#endif

int goome_gl2_init(goome *g, goome_gl_loader load);
void goome_gl2_render(goome *g);
void goome_gl2_resize(goome *g, int w, int h, double scale);
void goome_gl2_shutdown(goome *g);

#endif

#if defined(GOOME_IMPL_GL2_IMPLEMENTATION) && !defined(GOOME_IMPL_GL2_DONE)
#define GOOME_IMPL_GL2_DONE

#include <stdlib.h>
#include <string.h>

#ifndef GOOME_GL_LOAD_DONE
#define GOOME_GL_LOAD_DONE
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#define GOOME_GLAPI __stdcall
static goome_gl_proc gogl_default_proc(const char *name)
{
    static HMODULE gl;
    if (!gl) gl = LoadLibraryA("opengl32.dll");
    if (!gl) return NULL;
    static PROC(WINAPI * wgl)(LPCSTR);
    if (!wgl) {
        FARPROC f = GetProcAddress(gl, "wglGetProcAddress");
        memcpy(&wgl, &f, sizeof(f));
    }
    PROC p = wgl ? wgl(name) : NULL;
    if ((INT_PTR)p >= -1 && (INT_PTR)p <= 3) p = NULL;
    if (!p) p = GetProcAddress(gl, name);
    goome_gl_proc r;
    memcpy(&r, &p, sizeof(r));
    return r;
}
#else
#include <dlfcn.h>
#define GOOME_GLAPI
static goome_gl_proc gogl_default_proc(const char *name)
{
    static void *gl, *egl;
    static goome_gl_proc (*glx_get)(const unsigned char *);
    static goome_gl_proc (*egl_get)(const char *);
    static void *(*egl_cur)(void);
    static int tried;
    if (!tried) {
        tried = 1;
#ifdef __APPLE__
        gl = dlopen("/System/Library/Frameworks/OpenGL.framework/OpenGL", RTLD_LAZY | RTLD_LOCAL);
#else
        gl = dlopen("libGL.so.1", RTLD_LAZY | RTLD_LOCAL);
        if (!gl) gl = dlopen("libGLESv2.so.2", RTLD_LAZY | RTLD_LOCAL);
        egl = dlopen("libEGL.so.1", RTLD_LAZY | RTLD_LOCAL);
        void *p = gl ? dlsym(gl, "glXGetProcAddressARB") : NULL;
        memcpy(&glx_get, &p, sizeof(p));
        p = egl ? dlsym(egl, "eglGetProcAddress") : NULL;
        memcpy(&egl_get, &p, sizeof(p));
        p = egl ? dlsym(egl, "eglGetCurrentContext") : NULL;
        memcpy(&egl_cur, &p, sizeof(p));
#endif
    }
    goome_gl_proc r = NULL;
    if (egl_get && egl_cur && egl_cur()) r = egl_get(name);
    if (!r && glx_get) r = glx_get((const unsigned char *)name);
    if (!r) {
        void *p = gl ? dlsym(gl, name) : NULL;
        if (!p) p = dlsym(RTLD_DEFAULT, name);
        memcpy(&r, &p, sizeof(p));
    }
    return r;
}
#endif
#endif

#ifndef GOOME_GL_ENUMS_DONE
#define GOOME_GL_ENUMS_DONE
enum {
    GOGL_ACTIVE_TEXTURE = 0x84E0,
    GOGL_BLEND = 0x0BE2,
    GOGL_BLEND_DST_ALPHA = 0x80CA,
    GOGL_BLEND_DST_RGB = 0x80C8,
    GOGL_BLEND_EQUATION_ALPHA = 0x883D,
    GOGL_BLEND_EQUATION_RGB = 0x8009,
    GOGL_BLEND_SRC_ALPHA = 0x80CB,
    GOGL_BLEND_SRC_RGB = 0x80C9,
    GOGL_CLAMP_TO_EDGE = 0x812F,
    GOGL_COMPILE_STATUS = 0x8B81,
    GOGL_CULL_FACE = 0x0B44,
    GOGL_CURRENT_PROGRAM = 0x8B8D,
    GOGL_DEPTH_TEST = 0x0B71,
    GOGL_FILL = 0x1B02,
    GOGL_FRAGMENT_SHADER = 0x8B30,
    GOGL_FRAMEBUFFER_SRGB = 0x8DB9,
    GOGL_FRONT_AND_BACK = 0x0408,
    GOGL_FUNC_ADD = 0x8006,
    GOGL_LINEAR = 0x2601,
    GOGL_LINK_STATUS = 0x8B82,
    GOGL_NEAREST = 0x2600,
    GOGL_ONE = 1,
    GOGL_ONE_MINUS_SRC_ALPHA = 0x0303,
    GOGL_PIXEL_UNPACK_BUFFER = 0x88EC,
    GOGL_PIXEL_UNPACK_BUFFER_BINDING = 0x88EF,
    GOGL_POLYGON_MODE = 0x0B40,
    GOGL_RGBA = 0x1908,
    GOGL_RGBA8 = 0x8058,
    GOGL_SAMPLER_BINDING = 0x8919,
    GOGL_SCISSOR_TEST = 0x0C11,
    GOGL_STENCIL_TEST = 0x0B90,
    GOGL_TEXTURE0 = 0x84C0,
    GOGL_TEXTURE_2D = 0x0DE1,
    GOGL_TEXTURE_BINDING_2D = 0x8069,
    GOGL_TEXTURE_MAG_FILTER = 0x2800,
    GOGL_TEXTURE_MIN_FILTER = 0x2801,
    GOGL_TEXTURE_WRAP_S = 0x2802,
    GOGL_TEXTURE_WRAP_T = 0x2803,
    GOGL_TRIANGLE_STRIP = 5,
    GOGL_UNPACK_ALIGNMENT = 0x0CF5,
    GOGL_UNPACK_ROW_LENGTH = 0x0CF2,
    GOGL_UNPACK_SKIP_PIXELS = 0x0CF4,
    GOGL_UNPACK_SKIP_ROWS = 0x0CF3,
    GOGL_UNSIGNED_BYTE = 0x1401,
    GOGL_VERSION = 0x1F02,
    GOGL_VERTEX_ARRAY_BINDING = 0x85B5,
    GOGL_VERTEX_SHADER = 0x8B31,
    GOGL_VIEWPORT = 0x0BA2,
};
#endif

enum {
    GOGL2_CURRENT_BIT = 0x0001,
    GOGL2_POLYGON_BIT = 0x0008,
    GOGL2_TRANSFORM_BIT = 0x1000,
    GOGL2_ENABLE_BIT = 0x2000,
    GOGL2_COLOR_BUFFER_BIT = 0x4000,
    GOGL2_TEXTURE_BIT = 0x40000,
    GOGL2_SCISSOR_BIT = 0x80000,
    GOGL2_CLIENT_PIXEL_STORE_BIT = 0x0001,
    GOGL2_LIGHTING = 0x0B50,
    GOGL2_FOG = 0x0B60,
    GOGL2_ALPHA_TEST = 0x0BC0,
    GOGL2_TEXTURE_1D = 0x0DE0,
    GOGL2_TEXTURE_ENV = 0x2300,
    GOGL2_TEXTURE_ENV_MODE = 0x2200,
    GOGL2_REPLACE = 0x1E01,
    GOGL2_MODELVIEW = 0x1700,
    GOGL2_PROJECTION = 0x1701,
    GOGL2_TEXTURE = 0x1702,
    GOGL2_COLOR_MATERIAL = 0x0B57,
};

typedef struct {
    void(GOOME_GLAPI *GenTextures)(int, unsigned *);
    void(GOOME_GLAPI *DeleteTextures)(int, const unsigned *);
    void(GOOME_GLAPI *BindTexture)(unsigned, unsigned);
    void(GOOME_GLAPI *TexParameteri)(unsigned, unsigned, int);
    void(GOOME_GLAPI *TexImage2D)(unsigned, int, int, int, int, int, unsigned, unsigned, const void *);
    void(GOOME_GLAPI *TexSubImage2D)(unsigned, int, int, int, int, int, unsigned, unsigned, const void *);
    void(GOOME_GLAPI *PixelStorei)(unsigned, int);
    void(GOOME_GLAPI *GetIntegerv)(unsigned, int *);
    const unsigned char *(GOOME_GLAPI *GetString)(unsigned);
    void(GOOME_GLAPI *Enable)(unsigned);
    void(GOOME_GLAPI *Disable)(unsigned);
    void(GOOME_GLAPI *BlendFunc)(unsigned, unsigned);
    void(GOOME_GLAPI *PushAttrib)(unsigned);
    void(GOOME_GLAPI *PopAttrib)(void);
    void(GOOME_GLAPI *PushClientAttrib)(unsigned);
    void(GOOME_GLAPI *PopClientAttrib)(void);
    void(GOOME_GLAPI *MatrixMode)(unsigned);
    void(GOOME_GLAPI *PushMatrix)(void);
    void(GOOME_GLAPI *PopMatrix)(void);
    void(GOOME_GLAPI *LoadIdentity)(void);
    void(GOOME_GLAPI *TexEnvi)(unsigned, unsigned, int);
    void(GOOME_GLAPI *PolygonMode)(unsigned, unsigned);
    void(GOOME_GLAPI *Begin)(unsigned);
    void(GOOME_GLAPI *End)(void);
    void(GOOME_GLAPI *TexCoord2f)(float, float);
    void(GOOME_GLAPI *Vertex2f)(float, float);
    void(GOOME_GLAPI *Color4f)(float, float, float, float);
    void(GOOME_GLAPI *UseProgram)(unsigned);
    void(GOOME_GLAPI *BindBuffer)(unsigned, unsigned);
    void(GOOME_GLAPI *ActiveTexture)(unsigned);
} gg2_fns;

typedef struct {
    goome *g;
    gg2_fns gl;
    int tw, th, pw, ph, pot;
    unsigned tex;
    unsigned char *buf;
    size_t cap;
} gg2_state;

static gg2_state gg2;

static const char *const gg2_names[] = {
    "glGenTextures", "glDeleteTextures", "glBindTexture", "glTexParameteri", "glTexImage2D", "glTexSubImage2D", "glPixelStorei",
    "glGetIntegerv", "glGetString", "glEnable", "glDisable", "glBlendFunc", "glPushAttrib", "glPopAttrib", "glPushClientAttrib", "glPopClientAttrib",
    "glMatrixMode", "glPushMatrix", "glPopMatrix", "glLoadIdentity", "glTexEnvi", "glPolygonMode", "glBegin", "glEnd", "glTexCoord2f",
    "glVertex2f", "glColor4f", "glUseProgram", "glBindBuffer", "glActiveTexture",
};

int goome_gl2_init(goome *g, goome_gl_loader load)
{
    if (!g) return GOOME_EARG;
    if (gg2.g) goome_gl2_shutdown(gg2.g);
    memset(&gg2, 0, sizeof(gg2));
    if (!load) load = gogl_default_proc;
    size_t n = sizeof(gg2_names) / sizeof(gg2_names[0]);
    goome_gl_proc *slot = (goome_gl_proc *)(void *)&gg2.gl;
    for (size_t i = 0; i < n; i++) {
        slot[i] = load(gg2_names[i]);
        if (!slot[i] && i < n - 3) return GOOME_ENOTSUP;
    }
    const char *v = (const char *)gg2.gl.GetString(GOGL_VERSION);
    gg2.pot = !v || v[0] < '2';
    gg2.g = g;
    return GOOME_OK;
}

void goome_gl2_render(goome *g)
{
    if (!g || g != gg2.g) return;
    goome_image f;
    if (goome_frame(g, &f) != GOOME_OK) return;
    goome_rect r;
    int dirty = goome_frame_dirty(g, &r);
    gg2_fns *gl = &gg2.gl;
    int prog = 0, ubuf = 0, active = GOGL_TEXTURE0, vp[4];
    if (gl->UseProgram) gl->GetIntegerv(GOGL_CURRENT_PROGRAM, &prog);
    if (gl->BindBuffer) gl->GetIntegerv(GOGL_PIXEL_UNPACK_BUFFER_BINDING, &ubuf);
    if (gl->ActiveTexture) gl->GetIntegerv(GOGL_ACTIVE_TEXTURE, &active);
    gl->GetIntegerv(GOGL_VIEWPORT, vp);
    gl->PushAttrib(GOGL2_CURRENT_BIT | GOGL2_POLYGON_BIT | GOGL2_TRANSFORM_BIT | GOGL2_ENABLE_BIT | GOGL2_COLOR_BUFFER_BIT | GOGL2_TEXTURE_BIT | GOGL2_SCISSOR_BIT);
    gl->PushClientAttrib(GOGL2_CLIENT_PIXEL_STORE_BIT);
    if (gl->UseProgram) gl->UseProgram(0);
    if (gl->BindBuffer) gl->BindBuffer(GOGL_PIXEL_UNPACK_BUFFER, 0);
    if (gl->ActiveTexture) gl->ActiveTexture(GOGL_TEXTURE0);

    if (!gg2.tex) gl->GenTextures(1, &gg2.tex);
    gl->BindTexture(GOGL_TEXTURE_2D, gg2.tex);
    gl->PixelStorei(GOGL_UNPACK_ALIGNMENT, 4);
    gl->PixelStorei(GOGL_UNPACK_ROW_LENGTH, 0);
    gl->PixelStorei(GOGL_UNPACK_SKIP_PIXELS, 0);
    gl->PixelStorei(GOGL_UNPACK_SKIP_ROWS, 0);
    if (gg2.tw != f.w || gg2.th != f.h) {
        gl->TexParameteri(GOGL_TEXTURE_2D, GOGL_TEXTURE_WRAP_S, GOGL_CLAMP_TO_EDGE);
        gl->TexParameteri(GOGL_TEXTURE_2D, GOGL_TEXTURE_WRAP_T, GOGL_CLAMP_TO_EDGE);
        gg2.pw = f.w, gg2.ph = f.h;
        if (gg2.pot) {
            for (gg2.pw = 1; gg2.pw < f.w; gg2.pw *= 2) {}
            for (gg2.ph = 1; gg2.ph < f.h; gg2.ph *= 2) {}
        }
        gl->TexImage2D(GOGL_TEXTURE_2D, 0, GOGL_RGBA8, gg2.pw, gg2.ph, 0, GOGL_RGBA, GOGL_UNSIGNED_BYTE, NULL);
        gg2.tw = f.w, gg2.th = f.h;
        r = (goome_rect){0, 0, f.w, f.h};
        dirty = 1;
    }
    if (dirty) {
        size_t need = (size_t)r.w * r.h * 4;
        if (need > gg2.cap) {
            unsigned char *b = (unsigned char *)realloc(gg2.buf, need);
            if (b) gg2.buf = b, gg2.cap = need;
        }
        if (need <= gg2.cap && goome_frame_copy(g, gg2.buf, r.w * 4, GOOME_FMT_RGBA_PREMUL, &r) == GOOME_OK)
            gl->TexSubImage2D(GOGL_TEXTURE_2D, 0, r.x, r.y, r.w, r.h, GOGL_RGBA, GOGL_UNSIGNED_BYTE, gg2.buf);
    }
    int filter = vp[2] == f.w && vp[3] == f.h ? GOGL_NEAREST : GOGL_LINEAR;
    gl->TexParameteri(GOGL_TEXTURE_2D, GOGL_TEXTURE_MIN_FILTER, filter);
    gl->TexParameteri(GOGL_TEXTURE_2D, GOGL_TEXTURE_MAG_FILTER, filter);

    gl->Disable(GOGL2_LIGHTING);
    gl->Disable(GOGL2_FOG);
    gl->Disable(GOGL2_ALPHA_TEST);
    gl->Disable(GOGL2_COLOR_MATERIAL);
    gl->Disable(GOGL_DEPTH_TEST);
    gl->Disable(GOGL_CULL_FACE);
    gl->Disable(GOGL_SCISSOR_TEST);
    gl->Disable(GOGL_STENCIL_TEST);
    gl->Disable(GOGL2_TEXTURE_1D);
    gl->Enable(GOGL_TEXTURE_2D);
    gl->Enable(GOGL_BLEND);
    gl->BlendFunc(GOGL_ONE, GOGL_ONE_MINUS_SRC_ALPHA);
    gl->TexEnvi(GOGL2_TEXTURE_ENV, GOGL2_TEXTURE_ENV_MODE, GOGL2_REPLACE);
    gl->PolygonMode(GOGL_FRONT_AND_BACK, GOGL_FILL);
    gl->MatrixMode(GOGL2_TEXTURE);
    gl->PushMatrix();
    gl->LoadIdentity();
    gl->MatrixMode(GOGL2_PROJECTION);
    gl->PushMatrix();
    gl->LoadIdentity();
    gl->MatrixMode(GOGL2_MODELVIEW);
    gl->PushMatrix();
    gl->LoadIdentity();
    float u = (float)f.w / gg2.pw, v = (float)f.h / gg2.ph;
    gl->Color4f(1, 1, 1, 1);
    gl->Begin(GOGL_TRIANGLE_STRIP);
    gl->TexCoord2f(0, v);
    gl->Vertex2f(-1, -1);
    gl->TexCoord2f(u, v);
    gl->Vertex2f(1, -1);
    gl->TexCoord2f(0, 0);
    gl->Vertex2f(-1, 1);
    gl->TexCoord2f(u, 0);
    gl->Vertex2f(1, 1);
    gl->End();
    gl->PopMatrix();
    gl->MatrixMode(GOGL2_PROJECTION);
    gl->PopMatrix();
    gl->MatrixMode(GOGL2_TEXTURE);
    gl->PopMatrix();

    gl->PopClientAttrib();
    gl->PopAttrib();
    if (gl->ActiveTexture) gl->ActiveTexture((unsigned)active);
    if (gl->BindBuffer) gl->BindBuffer(GOGL_PIXEL_UNPACK_BUFFER, (unsigned)ubuf);
    if (gl->UseProgram) gl->UseProgram((unsigned)prog);
}

void goome_gl2_resize(goome *g, int w, int h, double scale)
{
    if (!g) return;
    goome_resize(g, w, h, scale);
    if (g == gg2.g) gg2.tw = gg2.th = 0;
}

void goome_gl2_shutdown(goome *g)
{
    if (!g || g != gg2.g) return;
    if (gg2.tex) gg2.gl.DeleteTextures(1, &gg2.tex);
    free(gg2.buf);
    memset(&gg2, 0, sizeof(gg2));
}

#endif

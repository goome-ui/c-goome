#ifndef GOOME_GL3_H
#define GOOME_GL3_H

#include "goome.h"

#ifndef GOOME_GL_PROC_DEFINED
#define GOOME_GL_PROC_DEFINED
typedef void (*goome_gl_proc)(void);
typedef goome_gl_proc (*goome_gl_loader)(const char *name);
#endif

int goome_gl3_init(goome *g, goome_gl_loader load);
void goome_gl3_render(goome *g);
void goome_gl3_resize(goome *g, int w, int h, double scale);
void goome_gl3_shutdown(goome *g);

#endif

#if defined(GOOME_IMPL_GL3_IMPLEMENTATION) && !defined(GOOME_IMPL_GL3_DONE)
#define GOOME_IMPL_GL3_DONE

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
    unsigned char(GOOME_GLAPI *IsEnabled)(unsigned);
    void(GOOME_GLAPI *Enable)(unsigned);
    void(GOOME_GLAPI *Disable)(unsigned);
    void(GOOME_GLAPI *BlendFuncSeparate)(unsigned, unsigned, unsigned, unsigned);
    void(GOOME_GLAPI *BlendEquationSeparate)(unsigned, unsigned);
    void(GOOME_GLAPI *DrawArrays)(unsigned, int, int);
    void(GOOME_GLAPI *ActiveTexture)(unsigned);
    void(GOOME_GLAPI *BindBuffer)(unsigned, unsigned);
    void(GOOME_GLAPI *BindSampler)(unsigned, unsigned);
    void(GOOME_GLAPI *PolygonMode)(unsigned, unsigned);
    unsigned(GOOME_GLAPI *CreateShader)(unsigned);
    void(GOOME_GLAPI *ShaderSource)(unsigned, int, const char *const *, const int *);
    void(GOOME_GLAPI *CompileShader)(unsigned);
    void(GOOME_GLAPI *GetShaderiv)(unsigned, unsigned, int *);
    void(GOOME_GLAPI *DeleteShader)(unsigned);
    unsigned(GOOME_GLAPI *CreateProgram)(void);
    void(GOOME_GLAPI *AttachShader)(unsigned, unsigned);
    void(GOOME_GLAPI *LinkProgram)(unsigned);
    void(GOOME_GLAPI *GetProgramiv)(unsigned, unsigned, int *);
    void(GOOME_GLAPI *DeleteProgram)(unsigned);
    void(GOOME_GLAPI *UseProgram)(unsigned);
    int(GOOME_GLAPI *GetUniformLocation)(unsigned, const char *);
    void(GOOME_GLAPI *Uniform1i)(int, int);
    void(GOOME_GLAPI *GenVertexArrays)(int, unsigned *);
    void(GOOME_GLAPI *BindVertexArray)(unsigned);
    void(GOOME_GLAPI *DeleteVertexArrays)(int, const unsigned *);
} gg3_fns;

typedef struct {
    goome *g;
    gg3_fns gl;
    int es, tw, th, loc;
    unsigned tex, prog, vao;
    unsigned char *buf;
    size_t cap;
} gg3_state;

static gg3_state gg3;

static const char *const gg3_names[] = {
    "glGenTextures", "glDeleteTextures", "glBindTexture", "glTexParameteri", "glTexImage2D", "glTexSubImage2D", "glPixelStorei",
    "glGetIntegerv", "glGetString", "glIsEnabled", "glEnable", "glDisable", "glBlendFuncSeparate", "glBlendEquationSeparate",
    "glDrawArrays", "glActiveTexture", "glBindBuffer", "glBindSampler", "glPolygonMode", "glCreateShader", "glShaderSource",
    "glCompileShader", "glGetShaderiv", "glDeleteShader", "glCreateProgram", "glAttachShader", "glLinkProgram", "glGetProgramiv",
    "glDeleteProgram", "glUseProgram", "glGetUniformLocation", "glUniform1i", "glGenVertexArrays", "glBindVertexArray", "glDeleteVertexArrays",
};

static unsigned gg3_shader(unsigned type, const char *head, const char *body)
{
    const char *src[2] = {head, body};
    unsigned s = gg3.gl.CreateShader(type);
    gg3.gl.ShaderSource(s, 2, src, NULL);
    gg3.gl.CompileShader(s);
    int ok = 0;
    gg3.gl.GetShaderiv(s, GOGL_COMPILE_STATUS, &ok);
    if (!ok) {
        gg3.gl.DeleteShader(s);
        return 0;
    }
    return s;
}

int goome_gl3_init(goome *g, goome_gl_loader load)
{
    if (!g) return GOOME_EARG;
    if (gg3.g) goome_gl3_shutdown(gg3.g);
    memset(&gg3, 0, sizeof(gg3));
    if (!load) load = gogl_default_proc;
    goome_gl_proc *slot = (goome_gl_proc *)(void *)&gg3.gl;
    for (size_t i = 0; i < sizeof(gg3_names) / sizeof(gg3_names[0]); i++) {
        slot[i] = load(gg3_names[i]);
        if (!slot[i] && strcmp(gg3_names[i], "glPolygonMode") && strcmp(gg3_names[i], "glBindSampler")) return GOOME_ENOTSUP;
    }
    const char *v = (const char *)gg3.gl.GetString(GOGL_VERSION);
    gg3.es = v && strstr(v, "OpenGL ES") != NULL;
    const char *head = gg3.es ? "#version 300 es\nprecision highp float;\n" : "#version 330 core\n";
    unsigned vs = gg3_shader(GOGL_VERTEX_SHADER, head,
                             "out vec2 uv;\n"
                             "void main() {\n"
                             "    vec2 p = vec2(float(gl_VertexID & 1), float((gl_VertexID >> 1) & 1));\n"
                             "    uv = vec2(p.x, 1.0 - p.y);\n"
                             "    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);\n"
                             "}\n");
    unsigned fs = gg3_shader(GOGL_FRAGMENT_SHADER, head,
                             "uniform sampler2D tex;\n"
                             "in vec2 uv;\n"
                             "out vec4 col;\n"
                             "void main() { col = texture(tex, uv); }\n");
    if (!vs || !fs) {
        if (vs) gg3.gl.DeleteShader(vs);
        if (fs) gg3.gl.DeleteShader(fs);
        return GOOME_ENOTSUP;
    }
    gg3.prog = gg3.gl.CreateProgram();
    gg3.gl.AttachShader(gg3.prog, vs);
    gg3.gl.AttachShader(gg3.prog, fs);
    gg3.gl.LinkProgram(gg3.prog);
    gg3.gl.DeleteShader(vs);
    gg3.gl.DeleteShader(fs);
    int ok = 0;
    gg3.gl.GetProgramiv(gg3.prog, GOGL_LINK_STATUS, &ok);
    if (!ok) {
        gg3.gl.DeleteProgram(gg3.prog);
        return GOOME_ENOTSUP;
    }
    gg3.loc = gg3.gl.GetUniformLocation(gg3.prog, "tex");
    gg3.gl.GenVertexArrays(1, &gg3.vao);
    gg3.g = g;
    return GOOME_OK;
}

void goome_gl3_render(goome *g)
{
    if (!g || g != gg3.g) return;
    goome_image f;
    if (goome_frame(g, &f) != GOOME_OK) return;
    goome_rect r;
    int dirty = goome_frame_dirty(g, &r);
    gg3_fns *gl = &gg3.gl;
    int prog, vao, active, tex, sampler = 0, ubuf, align, rowlen, skipx, skipy, vp[4], poly[2] = {GOGL_FILL, GOGL_FILL};
    int bsrc, bdst, basrc, badst, beq, beqa;
    unsigned char blend, depth, cull, scissor, stencil, srgb = 0;
    gl->GetIntegerv(GOGL_CURRENT_PROGRAM, &prog);
    gl->GetIntegerv(GOGL_VERTEX_ARRAY_BINDING, &vao);
    gl->GetIntegerv(GOGL_ACTIVE_TEXTURE, &active);
    gl->ActiveTexture(GOGL_TEXTURE0);
    gl->GetIntegerv(GOGL_TEXTURE_BINDING_2D, &tex);
    if (gl->BindSampler) gl->GetIntegerv(GOGL_SAMPLER_BINDING, &sampler);
    gl->GetIntegerv(GOGL_PIXEL_UNPACK_BUFFER_BINDING, &ubuf);
    gl->GetIntegerv(GOGL_UNPACK_ALIGNMENT, &align);
    gl->GetIntegerv(GOGL_UNPACK_ROW_LENGTH, &rowlen);
    gl->GetIntegerv(GOGL_UNPACK_SKIP_PIXELS, &skipx);
    gl->GetIntegerv(GOGL_UNPACK_SKIP_ROWS, &skipy);
    gl->GetIntegerv(GOGL_VIEWPORT, vp);
    gl->GetIntegerv(GOGL_BLEND_SRC_RGB, &bsrc);
    gl->GetIntegerv(GOGL_BLEND_DST_RGB, &bdst);
    gl->GetIntegerv(GOGL_BLEND_SRC_ALPHA, &basrc);
    gl->GetIntegerv(GOGL_BLEND_DST_ALPHA, &badst);
    gl->GetIntegerv(GOGL_BLEND_EQUATION_RGB, &beq);
    gl->GetIntegerv(GOGL_BLEND_EQUATION_ALPHA, &beqa);
    blend = gl->IsEnabled(GOGL_BLEND), depth = gl->IsEnabled(GOGL_DEPTH_TEST), cull = gl->IsEnabled(GOGL_CULL_FACE);
    scissor = gl->IsEnabled(GOGL_SCISSOR_TEST), stencil = gl->IsEnabled(GOGL_STENCIL_TEST);
    if (!gg3.es) {
        srgb = gl->IsEnabled(GOGL_FRAMEBUFFER_SRGB);
        if (gl->PolygonMode) gl->GetIntegerv(GOGL_POLYGON_MODE, poly);
    }

    if (!gg3.tex) gl->GenTextures(1, &gg3.tex);
    gl->BindTexture(GOGL_TEXTURE_2D, gg3.tex);
    if (gl->BindSampler) gl->BindSampler(0, 0);
    gl->BindBuffer(GOGL_PIXEL_UNPACK_BUFFER, 0);
    gl->PixelStorei(GOGL_UNPACK_ALIGNMENT, 4);
    gl->PixelStorei(GOGL_UNPACK_ROW_LENGTH, 0);
    gl->PixelStorei(GOGL_UNPACK_SKIP_PIXELS, 0);
    gl->PixelStorei(GOGL_UNPACK_SKIP_ROWS, 0);
    if (gg3.tw != f.w || gg3.th != f.h) {
        gl->TexParameteri(GOGL_TEXTURE_2D, GOGL_TEXTURE_WRAP_S, GOGL_CLAMP_TO_EDGE);
        gl->TexParameteri(GOGL_TEXTURE_2D, GOGL_TEXTURE_WRAP_T, GOGL_CLAMP_TO_EDGE);
        gl->TexImage2D(GOGL_TEXTURE_2D, 0, GOGL_RGBA8, f.w, f.h, 0, GOGL_RGBA, GOGL_UNSIGNED_BYTE, NULL);
        gg3.tw = f.w, gg3.th = f.h;
        r = (goome_rect){0, 0, f.w, f.h};
        dirty = 1;
    }
    if (dirty) {
        size_t need = (size_t)r.w * r.h * 4;
        if (need > gg3.cap) {
            unsigned char *b = (unsigned char *)realloc(gg3.buf, need);
            if (b) gg3.buf = b, gg3.cap = need;
        }
        if (need <= gg3.cap && goome_frame_copy(g, gg3.buf, r.w * 4, GOOME_FMT_RGBA_PREMUL, &r) == GOOME_OK)
            gl->TexSubImage2D(GOGL_TEXTURE_2D, 0, r.x, r.y, r.w, r.h, GOGL_RGBA, GOGL_UNSIGNED_BYTE, gg3.buf);
    }
    int filter = vp[2] == f.w && vp[3] == f.h ? GOGL_NEAREST : GOGL_LINEAR;
    gl->TexParameteri(GOGL_TEXTURE_2D, GOGL_TEXTURE_MIN_FILTER, filter);
    gl->TexParameteri(GOGL_TEXTURE_2D, GOGL_TEXTURE_MAG_FILTER, filter);

    gl->UseProgram(gg3.prog);
    gl->Uniform1i(gg3.loc, 0);
    gl->BindVertexArray(gg3.vao);
    gl->Enable(GOGL_BLEND);
    gl->BlendEquationSeparate(GOGL_FUNC_ADD, GOGL_FUNC_ADD);
    gl->BlendFuncSeparate(GOGL_ONE, GOGL_ONE_MINUS_SRC_ALPHA, GOGL_ONE, GOGL_ONE_MINUS_SRC_ALPHA);
    gl->Disable(GOGL_DEPTH_TEST);
    gl->Disable(GOGL_CULL_FACE);
    gl->Disable(GOGL_SCISSOR_TEST);
    gl->Disable(GOGL_STENCIL_TEST);
    if (!gg3.es) {
        gl->Disable(GOGL_FRAMEBUFFER_SRGB);
        if (gl->PolygonMode) gl->PolygonMode(GOGL_FRONT_AND_BACK, GOGL_FILL);
    }
    gl->DrawArrays(GOGL_TRIANGLE_STRIP, 0, 4);

    if (!gg3.es) {
        if (gl->PolygonMode) gl->PolygonMode(GOGL_FRONT_AND_BACK, (unsigned)poly[0]);
        if (srgb) gl->Enable(GOGL_FRAMEBUFFER_SRGB);
    }
    if (stencil) gl->Enable(GOGL_STENCIL_TEST);
    if (scissor) gl->Enable(GOGL_SCISSOR_TEST);
    if (cull) gl->Enable(GOGL_CULL_FACE);
    if (depth) gl->Enable(GOGL_DEPTH_TEST);
    gl->BlendFuncSeparate((unsigned)bsrc, (unsigned)bdst, (unsigned)basrc, (unsigned)badst);
    gl->BlendEquationSeparate((unsigned)beq, (unsigned)beqa);
    if (!blend) gl->Disable(GOGL_BLEND);
    gl->BindVertexArray((unsigned)vao);
    gl->UseProgram((unsigned)prog);
    gl->PixelStorei(GOGL_UNPACK_ALIGNMENT, align);
    gl->PixelStorei(GOGL_UNPACK_ROW_LENGTH, rowlen);
    gl->PixelStorei(GOGL_UNPACK_SKIP_PIXELS, skipx);
    gl->PixelStorei(GOGL_UNPACK_SKIP_ROWS, skipy);
    gl->BindBuffer(GOGL_PIXEL_UNPACK_BUFFER, (unsigned)ubuf);
    if (gl->BindSampler) gl->BindSampler(0, (unsigned)sampler);
    gl->BindTexture(GOGL_TEXTURE_2D, (unsigned)tex);
    gl->ActiveTexture((unsigned)active);
}

void goome_gl3_resize(goome *g, int w, int h, double scale)
{
    if (!g) return;
    goome_resize(g, w, h, scale);
    if (g == gg3.g) gg3.tw = gg3.th = 0;
}

void goome_gl3_shutdown(goome *g)
{
    if (!g || g != gg3.g) return;
    if (gg3.tex) gg3.gl.DeleteTextures(1, &gg3.tex);
    if (gg3.vao) gg3.gl.DeleteVertexArrays(1, &gg3.vao);
    if (gg3.prog) gg3.gl.DeleteProgram(gg3.prog);
    free(gg3.buf);
    memset(&gg3, 0, sizeof(gg3));
}

#endif

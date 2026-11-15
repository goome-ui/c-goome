typedef struct {
    void(GOOME_GLAPI *Viewport)(int, int, int, int);
    void(GOOME_GLAPI *ClearColor)(float, float, float, float);
    void(GOOME_GLAPI *Clear)(unsigned);
    void(GOOME_GLAPI *ReadPixels)(int, int, int, int, unsigned, unsigned, void *);
    void(GOOME_GLAPI *GetIntegerv)(unsigned, int *);
    unsigned char(GOOME_GLAPI *IsEnabled)(unsigned);
    void(GOOME_GLAPI *GenTextures)(int, unsigned *);
    void(GOOME_GLAPI *BindTexture)(unsigned, unsigned);
    void(GOOME_GLAPI *Finish)(void);
    const unsigned char *(GOOME_GLAPI *GetString)(unsigned);
} gltest_fns;

static gltest_fns gt;

static int gltest_load(goome_gl_loader load)
{
    static const char *const names[] = {"glViewport", "glClearColor", "glClear", "glReadPixels", "glGetIntegerv", "glIsEnabled", "glGenTextures", "glBindTexture", "glFinish", "glGetString"};
    goome_gl_proc *slot = (goome_gl_proc *)(void *)&gt;
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++)
        if (!(slot[i] = load(names[i]))) return printf("  no %s\n", names[i]), 0;
    printf("  gl: %s / %s\n", (const char *)gt.GetString(0x1F02), (const char *)gt.GetString(0x1F01));
    return 1;
}

static void gltest_clear(int w, int h)
{
    gt.Viewport(0, 0, w, h);
    gt.ClearColor(CLEAR_R / 255.0f, CLEAR_G / 255.0f, CLEAR_B / 255.0f, 1);
    gt.Clear(0x4000);
}

static int gltest_compare(demo *d, const char *what, int w, int h)
{
    unsigned char *px = malloc((size_t)w * h * 4);
    if (!px) return 0;
    gt.Finish();
    gt.ReadPixels(0, 0, w, h, 0x1908, 0x1401, px);
    int ok = demo_compare(d, what, px, w, h);
    free(px);
    return ok;
}

static void gltest_state(demo *d, void (*render)(goome *), int w, int h)
{
    unsigned game_tex = 0;
    gt.GenTextures(1, &game_tex);
    gt.BindTexture(0x0DE1, game_tex);
    int before = 0, after = -1;
    gt.GetIntegerv(0x8069, &before);
    gltest_clear(w, h);
    render(d->g);
    gt.GetIntegerv(0x8069, &after);
    demo_expect(d, "game texture binding kept", before == after && after == (int)game_tex, 1);
    demo_expect(d, "game blend state kept", gt.IsEnabled(0x0BE2), 0);
}

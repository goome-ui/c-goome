#define GOOME_IMPL_METAL_IMPLEMENTATION
#include "../../backends/goome_metal.h"

#include "scene.h"

typedef struct {
    double r, g, b, a;
} clear_color;

int main(void)
{
    if (!gm_mtl_load()) {
        printf("no objc runtime\n");
        return 1;
    }
    void *metal = dlopen("/System/Library/Frameworks/Metal.framework/Metal", RTLD_NOW | RTLD_GLOBAL);
    void *sym = metal ? dlsym(metal, "MTLCreateSystemDefaultDevice") : NULL;
    gm_id (*make_dev)(void);
    memcpy(&make_dev, &sym, sizeof(sym));
    gm_id dev = make_dev ? make_dev() : NULL;
    if (!dev) {
        printf("SKIP: no metal device on this machine\n");
        return 0;
    }
    void *pool = gm_mtl_rt.push();
    gm_id name = gm_mtl_get(dev, "name");
    printf("metal device %s\n", name ? GM_MSG0(const char *)(name, GM_SEL("UTF8String")) : "?");
    gm_id queue = gm_mtl_get(dev, "newCommandQueue");
    gm_id td = gm_mtl_new("MTLTextureDescriptor");
    gm_mtl_set(td, "setPixelFormat:", GM_MTL_BGRA8);
    gm_mtl_set(td, "setWidth:", SCENE_W);
    gm_mtl_set(td, "setHeight:", SCENE_H);
    gm_mtl_set(td, "setUsage:", 1 | 4);
    gm_mtl_set(td, "setStorageMode:", 1);
    gm_id rt = GM_MSG(gm_id, gm_id)(dev, GM_SEL("newTextureWithDescriptor:"), td);
    gm_mtl_release(td);
    gm_mtl_rt.pop(pool);
    if (!queue || !rt) {
        printf("no metal queue or target\n");
        return 1;
    }
    unsigned char *px = malloc(SCENE_W * SCENE_H * 4);

    scene s;
    if (scene_open(&s)) return 1;
    if (goome_metal_init(s.g, dev) != GOOME_OK) {
        printf("goome_metal_init failed\n");
        return 1;
    }
    for (int frame = 0; frame <= 12; frame++) {
        if (frame == 10) goome_metal_resize(s.g, 300, 260, 1.25);
        scene_frame(&s, frame);
        pool = gm_mtl_rt.push();
        gm_id pass = gm_mtl_get((gm_id)gm_mtl_rt.cls("MTLRenderPassDescriptor"), "renderPassDescriptor");
        gm_id ca = gm_mtl_at(gm_mtl_get(pass, "colorAttachments"), 0);
        gm_mtl_seto(ca, "setTexture:", rt);
        gm_mtl_set(ca, "setLoadAction:", 2);
        gm_mtl_set(ca, "setStoreAction:", 1);
        clear_color cc = {0x33 / 255.0, 0x66 / 255.0, 0x99 / 255.0, 1};
        GM_MSG(void, clear_color)(ca, GM_SEL("setClearColor:"), cc);
        gm_id cb = gm_mtl_get(queue, "commandBuffer");
        gm_id enc = GM_MSG(gm_id, gm_id)(cb, GM_SEL("renderCommandEncoderWithDescriptor:"), pass);
        goome_metal_render(s.g, pass, enc);
        gm_mtl_get(enc, "endEncoding");
        gm_id blit = gm_mtl_get(cb, "blitCommandEncoder");
        gm_mtl_seto(blit, "synchronizeResource:", rt);
        gm_mtl_get(blit, "endEncoding");
        gm_mtl_get(cb, "commit");
        gm_mtl_get(cb, "waitUntilCompleted");
        gm_mtl_rt.pop(pool);
        if (frame != 0 && frame != 4 && frame != 6 && frame != 9 && frame != 12) continue;
        gm_mtl_region rg = {0, 0, 0, SCENE_W, SCENE_H, 1};
        GM_MSG(void, void *, gm_uint, gm_mtl_region, gm_uint)(rt, GM_SEL("getBytes:bytesPerRow:fromRegion:mipmapLevel:"), px, SCENE_W * 4, rg, 0);
        char tag[32];
        snprintf(tag, sizeof(tag), "metal frame %d", frame);
        scene_check(&s, px, SCENE_W * 4, 0, tag);
    }
    goome_metal_shutdown(s.g);
    printf("check %d volume %.2f window %.0f,%.0f\n", s.check, s.volume, s.wx, s.wy);
    int bad = s.bad;
    scene_close(&s);
    free(px);
    printf("%s\n", bad ? "FAIL" : "PASS");
    return bad;
}

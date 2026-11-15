#define COBJMACROS
#define GOOME_IMPL_DX9_IMPLEMENTATION
#include "../../backends/goome_dx9.h"

#include "scene.h"

int main(int argc, char **argv)
{
    int soft = argc > 1 && !strcmp(argv[1], "sw");
    WNDCLASSA wc;
    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = DefWindowProcA, wc.hInstance = GetModuleHandleA(NULL), wc.lpszClassName = "gfx_dx9";
    RegisterClassA(&wc);
    HWND win = CreateWindowA("gfx_dx9", "gfx_dx9", WS_OVERLAPPEDWINDOW, 0, 0, SCENE_W, SCENE_H, NULL, NULL, wc.hInstance, NULL);
    IDirect3D9 *d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d) {
        printf("no d3d9\n");
        return 1;
    }
    D3DPRESENT_PARAMETERS pp;
    memset(&pp, 0, sizeof(pp));
    pp.Windowed = TRUE, pp.SwapEffect = D3DSWAPEFFECT_DISCARD, pp.BackBufferFormat = D3DFMT_UNKNOWN, pp.hDeviceWindow = win;
    pp.BackBufferWidth = SCENE_W, pp.BackBufferHeight = SCENE_H;
    IDirect3DDevice9 *dev = NULL;
    DWORD vp = soft ? D3DCREATE_SOFTWARE_VERTEXPROCESSING : D3DCREATE_HARDWARE_VERTEXPROCESSING;
    HRESULT hr = IDirect3D9_CreateDevice(d3d, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, win, vp | D3DCREATE_FPU_PRESERVE, &pp, &dev);
    if (FAILED(hr) && !soft) hr = IDirect3D9_CreateDevice(d3d, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, win, D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE, &pp, &dev), soft = 1;
    if (FAILED(hr)) {
        printf("no d3d9 device: %08lx\n", (unsigned long)hr);
        return 1;
    }
    printf("d3d9 hal, %s vertex processing\n", soft ? "software" : "hardware");
    IDirect3DSurface9 *rt, *rb;
    IDirect3DDevice9_CreateRenderTarget(dev, SCENE_W, SCENE_H, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE, 0, FALSE, &rt, NULL);
    IDirect3DDevice9_CreateOffscreenPlainSurface(dev, SCENE_W, SCENE_H, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &rb, NULL);
    IDirect3DDevice9_SetRenderTarget(dev, 0, rt);

    scene s;
    if (scene_open(&s)) return 1;
    if (goome_dx9_init(s.g, dev) != GOOME_OK) {
        printf("goome_dx9_init failed\n");
        return 1;
    }
    for (int frame = 0; frame <= 12; frame++) {
        if (frame == 10) goome_dx9_resize(s.g, 300, 260, 1.25);
        scene_frame(&s, frame);
        IDirect3DDevice9_Clear(dev, 0, NULL, D3DCLEAR_TARGET, SCENE_CLEAR, 1, 0);
        IDirect3DDevice9_SetRenderState(dev, D3DRS_CULLMODE, D3DCULL_CW);
        IDirect3DDevice9_SetRenderState(dev, D3DRS_SRCBLEND, D3DBLEND_ZERO);
        IDirect3DDevice9_SetTexture(dev, 0, NULL);
        IDirect3DDevice9_BeginScene(dev);
        goome_dx9_render(s.g);
        IDirect3DDevice9_EndScene(dev);
        DWORD cull = 0, src = 0;
        IDirect3DBaseTexture9 *t = NULL;
        IDirect3DDevice9_GetRenderState(dev, D3DRS_CULLMODE, &cull);
        IDirect3DDevice9_GetRenderState(dev, D3DRS_SRCBLEND, &src);
        IDirect3DDevice9_GetTexture(dev, 0, &t);
        if (cull != D3DCULL_CW || src != D3DBLEND_ZERO || t) {
            printf("frame %d: host state not restored\n", frame);
            s.bad = 1;
        }
        if (t) IDirect3DBaseTexture9_Release(t);
        if (frame != 0 && frame != 4 && frame != 6 && frame != 9 && frame != 12) continue;
        if (FAILED(IDirect3DDevice9_GetRenderTargetData(dev, rt, rb))) return 1;
        D3DLOCKED_RECT lk;
        if (FAILED(IDirect3DSurface9_LockRect(rb, &lk, NULL, D3DLOCK_READONLY))) return 1;
        char tag[32];
        snprintf(tag, sizeof(tag), "dx9 frame %d", frame);
        scene_check(&s, lk.pBits, lk.Pitch, 0, tag);
        IDirect3DSurface9_UnlockRect(rb);
    }
    goome_dx9_shutdown(s.g);
    printf("check %d volume %.2f window %.0f,%.0f\n", s.check, s.volume, s.wx, s.wy);
    int bad = s.bad;
    scene_close(&s);
    IDirect3DSurface9_Release(rt);
    IDirect3DSurface9_Release(rb);
    ULONG left = IDirect3DDevice9_Release(dev);
    IDirect3D9_Release(d3d);
    printf("%s, %lu device refs left\n", bad ? "FAIL" : "PASS", (unsigned long)left);
    return bad || left;
}

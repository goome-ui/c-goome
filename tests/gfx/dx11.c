#define COBJMACROS
#define GOOME_IMPL_DX11_IMPLEMENTATION
#include "../../backends/goome_dx11.h"

#include "scene.h"

int main(int argc, char **argv)
{
    int warp = argc > 1 && !strcmp(argv[1], "warp");
    ID3D11Device *dev = NULL;
    ID3D11DeviceContext *ctx = NULL;
    D3D_FEATURE_LEVEL fl;
    HRESULT hr = D3D11CreateDevice(NULL, warp ? D3D_DRIVER_TYPE_WARP : D3D_DRIVER_TYPE_HARDWARE, NULL, 0, NULL, 0, D3D11_SDK_VERSION, &dev, &fl, &ctx);
    if (FAILED(hr)) {
        printf("no d3d11 device: %08lx\n", (unsigned long)hr);
        return 1;
    }
    printf("d3d11 %s, feature level %x\n", warp ? "warp" : "hardware", (unsigned)fl);
    D3D11_TEXTURE2D_DESC td;
    memset(&td, 0, sizeof(td));
    td.Width = SCENE_W, td.Height = SCENE_H, td.MipLevels = 1, td.ArraySize = 1, td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    td.SampleDesc.Count = 1, td.Usage = D3D11_USAGE_DEFAULT, td.BindFlags = D3D11_BIND_RENDER_TARGET;
    ID3D11Texture2D *rt, *rb;
    ID3D11RenderTargetView *rtv;
    ID3D11Device_CreateTexture2D(dev, &td, NULL, &rt);
    ID3D11Device_CreateRenderTargetView(dev, (ID3D11Resource *)rt, NULL, &rtv);
    td.Usage = D3D11_USAGE_STAGING, td.BindFlags = 0, td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    ID3D11Device_CreateTexture2D(dev, &td, NULL, &rb);

    scene s;
    if (scene_open(&s)) return 1;
    if (goome_dx11_init(s.g, dev, ctx) != GOOME_OK) {
        printf("goome_dx11_init failed\n");
        return 1;
    }
    float clear[4] = {0x33 / 255.0f, 0x66 / 255.0f, 0x99 / 255.0f, 1};
    D3D11_VIEWPORT host = {7, 9, 100, 50, 0.25f, 0.75f};
    for (int frame = 0; frame <= 12; frame++) {
        if (frame == 10) goome_dx11_resize(s.g, 300, 260, 1.25);
        scene_frame(&s, frame);
        ID3D11DeviceContext_OMSetRenderTargets(ctx, 1, &rtv, NULL);
        ID3D11DeviceContext_ClearRenderTargetView(ctx, rtv, clear);
        ID3D11DeviceContext_RSSetViewports(ctx, 1, &host);
        ID3D11DeviceContext_IASetPrimitiveTopology(ctx, D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
        goome_dx11_render(s.g);
        D3D11_VIEWPORT vp;
        UINT n = 1;
        D3D11_PRIMITIVE_TOPOLOGY topo;
        ID3D11DeviceContext_RSGetViewports(ctx, &n, &vp);
        ID3D11DeviceContext_IAGetPrimitiveTopology(ctx, &topo);
        if (n != 1 || memcmp(&vp, &host, sizeof(vp)) || topo != D3D11_PRIMITIVE_TOPOLOGY_LINELIST) {
            printf("frame %d: host state not restored\n", frame);
            s.bad = 1;
        }
        if (frame != 0 && frame != 4 && frame != 6 && frame != 9 && frame != 12) continue;
        ID3D11DeviceContext_CopyResource(ctx, (ID3D11Resource *)rb, (ID3D11Resource *)rt);
        D3D11_MAPPED_SUBRESOURCE m;
        if (FAILED(ID3D11DeviceContext_Map(ctx, (ID3D11Resource *)rb, 0, D3D11_MAP_READ, 0, &m))) return 1;
        char tag[32];
        snprintf(tag, sizeof(tag), "dx11 frame %d", frame);
        scene_check(&s, m.pData, (int)m.RowPitch, 0, tag);
        ID3D11DeviceContext_Unmap(ctx, (ID3D11Resource *)rb, 0);
    }
    goome_dx11_shutdown(s.g);
    printf("check %d volume %.2f window %.0f,%.0f\n", s.check, s.volume, s.wx, s.wy);
    int bad = s.bad;
    scene_close(&s);
    ID3D11RenderTargetView_Release(rtv);
    ID3D11Texture2D_Release(rt);
    ID3D11Texture2D_Release(rb);
    ID3D11DeviceContext_Release(ctx);
    ULONG left = ID3D11Device_Release(dev);
    printf("%s, %lu device refs left\n", bad ? "FAIL" : "PASS", (unsigned long)left);
    return bad || left;
}

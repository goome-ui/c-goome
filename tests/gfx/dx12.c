#define COBJMACROS
#define GOOME_IMPL_DX12_IMPLEMENTATION
#include "../../backends/goome_dx12.h"

#include <dxgi1_4.h>

#include "scene.h"

static ID3D12Device *dev;
static ID3D12CommandQueue *queue;
static ID3D12CommandAllocator *alloc;
static ID3D12GraphicsCommandList *cl;
static ID3D12Fence *fence;
static UINT64 fval;
static HANDLE ev;

static void barrier(ID3D12Resource *r, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to)
{
    D3D12_RESOURCE_BARRIER b;
    memset(&b, 0, sizeof(b));
    b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Transition.pResource = r, b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    b.Transition.StateBefore = from, b.Transition.StateAfter = to;
    ID3D12GraphicsCommandList_ResourceBarrier(cl, 1, &b);
}

static void submit(void)
{
    ID3D12GraphicsCommandList_Close(cl);
    ID3D12CommandQueue_ExecuteCommandLists(queue, 1, (ID3D12CommandList **)&cl);
    ID3D12CommandQueue_Signal(queue, fence, ++fval);
    if (ID3D12Fence_GetCompletedValue(fence) < fval) {
        ID3D12Fence_SetEventOnCompletion(fence, fval, ev);
        WaitForSingleObject(ev, INFINITE);
    }
}

int main(int argc, char **argv)
{
    int warp = argc > 1 && !strcmp(argv[1], "warp");
    IDXGIFactory4 *fac = NULL;
    IDXGIAdapter *ad = NULL;
    if (warp) {
        if (FAILED(CreateDXGIFactory1(&IID_IDXGIFactory4, (void **)&fac)) || FAILED(IDXGIFactory4_EnumWarpAdapter(fac, &IID_IDXGIAdapter, (void **)&ad))) {
            printf("no warp adapter\n");
            return 1;
        }
    }
    HRESULT hr = D3D12CreateDevice((IUnknown *)ad, D3D_FEATURE_LEVEL_11_0, &IID_ID3D12Device, (void **)&dev);
    if (FAILED(hr)) {
        printf("no d3d12 device: %08lx\n", (unsigned long)hr);
        return 1;
    }
    printf("d3d12 %s\n", warp ? "warp" : "default adapter");
    D3D12_COMMAND_QUEUE_DESC qd;
    memset(&qd, 0, sizeof(qd));
    ID3D12Device_CreateCommandQueue(dev, &qd, &IID_ID3D12CommandQueue, (void **)&queue);
    ID3D12Device_CreateCommandAllocator(dev, D3D12_COMMAND_LIST_TYPE_DIRECT, &IID_ID3D12CommandAllocator, (void **)&alloc);
    ID3D12Device_CreateCommandList(dev, 0, D3D12_COMMAND_LIST_TYPE_DIRECT, alloc, NULL, &IID_ID3D12GraphicsCommandList, (void **)&cl);
    ID3D12GraphicsCommandList_Close(cl);
    ID3D12Device_CreateFence(dev, 0, D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence, (void **)&fence);
    ev = CreateEventA(NULL, FALSE, FALSE, NULL);

    D3D12_HEAP_PROPERTIES hp;
    memset(&hp, 0, sizeof(hp));
    hp.Type = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_DESC rd;
    memset(&rd, 0, sizeof(rd));
    rd.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D, rd.Width = SCENE_W, rd.Height = SCENE_H, rd.DepthOrArraySize = 1, rd.MipLevels = 1;
    rd.Format = DXGI_FORMAT_B8G8R8A8_UNORM, rd.SampleDesc.Count = 1, rd.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    D3D12_CLEAR_VALUE cv = {DXGI_FORMAT_B8G8R8A8_UNORM, {{0x33 / 255.0f, 0x66 / 255.0f, 0x99 / 255.0f, 1}}};
    ID3D12Resource *rt, *rb;
    ID3D12Device_CreateCommittedResource(dev, &hp, D3D12_HEAP_FLAG_NONE, &rd, D3D12_RESOURCE_STATE_RENDER_TARGET, &cv, &IID_ID3D12Resource, (void **)&rt);
    UINT pitch = (SCENE_W * 4 + 255) & ~255u;
    hp.Type = D3D12_HEAP_TYPE_READBACK;
    memset(&rd, 0, sizeof(rd));
    rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER, rd.Width = (UINT64)pitch * SCENE_H, rd.Height = 1, rd.DepthOrArraySize = 1, rd.MipLevels = 1;
    rd.SampleDesc.Count = 1, rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    ID3D12Device_CreateCommittedResource(dev, &hp, D3D12_HEAP_FLAG_NONE, &rd, D3D12_RESOURCE_STATE_COPY_DEST, NULL, &IID_ID3D12Resource, (void **)&rb);

    D3D12_DESCRIPTOR_HEAP_DESC hd = {D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 0};
    ID3D12DescriptorHeap *rtvs, *srvs;
    ID3D12Device_CreateDescriptorHeap(dev, &hd, &IID_ID3D12DescriptorHeap, (void **)&rtvs);
    hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, hd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ID3D12Device_CreateDescriptorHeap(dev, &hd, &IID_ID3D12DescriptorHeap, (void **)&srvs);
    D3D12_CPU_DESCRIPTOR_HANDLE rtv, scpu;
    D3D12_GPU_DESCRIPTOR_HANDLE sgpu;
    rtvs->lpVtbl->GetCPUDescriptorHandleForHeapStart(rtvs, &rtv);
    srvs->lpVtbl->GetCPUDescriptorHandleForHeapStart(srvs, &scpu);
    srvs->lpVtbl->GetGPUDescriptorHandleForHeapStart(srvs, &sgpu);
    ID3D12Device_CreateRenderTargetView(dev, rt, NULL, rtv);

    scene s;
    if (scene_open(&s)) return 1;
    if (goome_dx12_init(s.g, dev, 2, DXGI_FORMAT_B8G8R8A8_UNORM, srvs, scpu, sgpu) != GOOME_OK) {
        printf("goome_dx12_init failed\n");
        return 1;
    }
    for (int frame = 0; frame <= 12; frame++) {
        if (frame == 10) goome_dx12_resize(s.g, 300, 260, 1.25);
        scene_frame(&s, frame);
        ID3D12CommandAllocator_Reset(alloc);
        ID3D12GraphicsCommandList_Reset(cl, alloc, NULL);
        ID3D12GraphicsCommandList_OMSetRenderTargets(cl, 1, &rtv, FALSE, NULL);
        ID3D12GraphicsCommandList_ClearRenderTargetView(cl, rtv, cv.Color, 0, NULL);
        goome_dx12_render(s.g, cl);
        int look = frame == 0 || frame == 4 || frame == 6 || frame == 9 || frame == 12;
        if (look) {
            barrier(rt, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE);
            D3D12_TEXTURE_COPY_LOCATION dst, src;
            memset(&dst, 0, sizeof(dst));
            memset(&src, 0, sizeof(src));
            src.pResource = rt, src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            dst.pResource = rb, dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            dst.PlacedFootprint.Footprint.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
            dst.PlacedFootprint.Footprint.Width = SCENE_W, dst.PlacedFootprint.Footprint.Height = SCENE_H;
            dst.PlacedFootprint.Footprint.Depth = 1, dst.PlacedFootprint.Footprint.RowPitch = pitch;
            ID3D12GraphicsCommandList_CopyTextureRegion(cl, &dst, 0, 0, 0, &src, NULL);
            barrier(rt, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
        }
        submit();
        if (!look) continue;
        void *p = NULL;
        D3D12_RANGE all = {0, (SIZE_T)pitch * SCENE_H}, none = {0, 0};
        if (FAILED(ID3D12Resource_Map(rb, 0, &all, &p))) return 1;
        char tag[32];
        snprintf(tag, sizeof(tag), "dx12 frame %d", frame);
        scene_check(&s, p, (int)pitch, 0, tag);
        ID3D12Resource_Unmap(rb, 0, &none);
    }
    goome_dx12_shutdown(s.g);
    printf("check %d volume %.2f window %.0f,%.0f\n", s.check, s.volume, s.wx, s.wy);
    int bad = s.bad;
    scene_close(&s);
    printf("%s\n", bad ? "FAIL" : "PASS");
    return bad;
}

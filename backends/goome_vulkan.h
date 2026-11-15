#ifndef GOOME_VULKAN_H
#define GOOME_VULKAN_H

#include <vulkan/vulkan.h>

#include "goome.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    VkInstance instance;
    VkPhysicalDevice physical_device;
    VkDevice device;
    uint32_t queue_family;
    VkQueue queue;
    VkRenderPass render_pass;
    uint32_t subpass;
    VkSampleCountFlagBits samples;
    PFN_vkGetInstanceProcAddr get_proc;
} goome_vulkan_info;

int goome_vulkan_init(goome *g, const goome_vulkan_info *info);
void goome_vulkan_render(goome *g, VkCommandBuffer cmd);
void goome_vulkan_resize(goome *g, int w, int h, double scale);
void goome_vulkan_shutdown(goome *g);

#ifdef __cplusplus
}
#endif

#endif

#if defined(GOOME_IMPL_VULKAN_IMPLEMENTATION) && !defined(GOOME_IMPL_VULKAN_DONE)
#define GOOME_IMPL_VULKAN_DONE

#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

static const uint32_t gm_vk_vs[] = {
    0x07230203, 0x00010000, 0x0008000b, 0x00000029, 0x00000000, 0x00020011, 0x00000001, 0x0006000b,
    0x00000001, 0x4c534c47, 0x6474732e, 0x3035342e, 0x00000000, 0x0003000e, 0x00000000, 0x00000001,
    0x0007000f, 0x00000000, 0x00000004, 0x6e69616d, 0x00000000, 0x0000000c, 0x0000001b, 0x00040047,
    0x0000000c, 0x0000000b, 0x0000002a, 0x00030047, 0x00000019, 0x00000002, 0x00050048, 0x00000019,
    0x00000000, 0x0000000b, 0x00000000, 0x00050048, 0x00000019, 0x00000001, 0x0000000b, 0x00000001,
    0x00050048, 0x00000019, 0x00000002, 0x0000000b, 0x00000003, 0x00050048, 0x00000019, 0x00000003,
    0x0000000b, 0x00000004, 0x00020013, 0x00000002, 0x00030021, 0x00000003, 0x00000002, 0x00030016,
    0x00000006, 0x00000020, 0x00040017, 0x00000007, 0x00000006, 0x00000002, 0x00040020, 0x00000008,
    0x00000007, 0x00000007, 0x00040015, 0x0000000a, 0x00000020, 0x00000001, 0x00040020, 0x0000000b,
    0x00000001, 0x0000000a, 0x0004003b, 0x0000000b, 0x0000000c, 0x00000001, 0x0004002b, 0x0000000a,
    0x0000000e, 0x00000001, 0x00040017, 0x00000015, 0x00000006, 0x00000004, 0x00040015, 0x00000016,
    0x00000020, 0x00000000, 0x0004002b, 0x00000016, 0x00000017, 0x00000001, 0x0004001c, 0x00000018,
    0x00000006, 0x00000017, 0x0006001e, 0x00000019, 0x00000015, 0x00000006, 0x00000018, 0x00000018,
    0x00040020, 0x0000001a, 0x00000003, 0x00000019, 0x0004003b, 0x0000001a, 0x0000001b, 0x00000003,
    0x0004002b, 0x0000000a, 0x0000001c, 0x00000000, 0x0004002b, 0x00000006, 0x0000001e, 0x40000000,
    0x0004002b, 0x00000006, 0x00000020, 0x3f800000, 0x0004002b, 0x00000006, 0x00000023, 0x00000000,
    0x00040020, 0x00000027, 0x00000003, 0x00000015, 0x00050036, 0x00000002, 0x00000004, 0x00000000,
    0x00000003, 0x000200f8, 0x00000005, 0x0004003b, 0x00000008, 0x00000009, 0x00000007, 0x0004003d,
    0x0000000a, 0x0000000d, 0x0000000c, 0x000500c7, 0x0000000a, 0x0000000f, 0x0000000d, 0x0000000e,
    0x0004006f, 0x00000006, 0x00000010, 0x0000000f, 0x0004003d, 0x0000000a, 0x00000011, 0x0000000c,
    0x000500c3, 0x0000000a, 0x00000012, 0x00000011, 0x0000000e, 0x0004006f, 0x00000006, 0x00000013,
    0x00000012, 0x00050050, 0x00000007, 0x00000014, 0x00000010, 0x00000013, 0x0003003e, 0x00000009,
    0x00000014, 0x0004003d, 0x00000007, 0x0000001d, 0x00000009, 0x0005008e, 0x00000007, 0x0000001f,
    0x0000001d, 0x0000001e, 0x00050050, 0x00000007, 0x00000021, 0x00000020, 0x00000020, 0x00050083,
    0x00000007, 0x00000022, 0x0000001f, 0x00000021, 0x00050051, 0x00000006, 0x00000024, 0x00000022,
    0x00000000, 0x00050051, 0x00000006, 0x00000025, 0x00000022, 0x00000001, 0x00070050, 0x00000015,
    0x00000026, 0x00000024, 0x00000025, 0x00000023, 0x00000020, 0x00050041, 0x00000027, 0x00000028,
    0x0000001b, 0x0000001c, 0x0003003e, 0x00000028, 0x00000026, 0x000100fd, 0x00010038,
};

static const uint32_t gm_vk_fs[] = {
    0x07230203, 0x00010000, 0x0008000b, 0x0000001a, 0x00000000, 0x00020011, 0x00000001, 0x0006000b,
    0x00000001, 0x4c534c47, 0x6474732e, 0x3035342e, 0x00000000, 0x0003000e, 0x00000000, 0x00000001,
    0x0007000f, 0x00000004, 0x00000004, 0x6e69616d, 0x00000000, 0x00000009, 0x00000010, 0x00030010,
    0x00000004, 0x00000007, 0x00040047, 0x00000009, 0x0000001e, 0x00000000, 0x00040047, 0x0000000d,
    0x00000021, 0x00000000, 0x00040047, 0x0000000d, 0x00000022, 0x00000000, 0x00040047, 0x00000010,
    0x0000000b, 0x0000000f, 0x00020013, 0x00000002, 0x00030021, 0x00000003, 0x00000002, 0x00030016,
    0x00000006, 0x00000020, 0x00040017, 0x00000007, 0x00000006, 0x00000004, 0x00040020, 0x00000008,
    0x00000003, 0x00000007, 0x0004003b, 0x00000008, 0x00000009, 0x00000003, 0x00090019, 0x0000000a,
    0x00000006, 0x00000001, 0x00000000, 0x00000000, 0x00000000, 0x00000001, 0x00000000, 0x0003001b,
    0x0000000b, 0x0000000a, 0x00040020, 0x0000000c, 0x00000000, 0x0000000b, 0x0004003b, 0x0000000c,
    0x0000000d, 0x00000000, 0x00040020, 0x0000000f, 0x00000001, 0x00000007, 0x0004003b, 0x0000000f,
    0x00000010, 0x00000001, 0x00040017, 0x00000011, 0x00000006, 0x00000002, 0x00040015, 0x00000014,
    0x00000020, 0x00000001, 0x00040017, 0x00000015, 0x00000014, 0x00000002, 0x0004002b, 0x00000014,
    0x00000017, 0x00000000, 0x00050036, 0x00000002, 0x00000004, 0x00000000, 0x00000003, 0x000200f8,
    0x00000005, 0x0004003d, 0x0000000b, 0x0000000e, 0x0000000d, 0x0004003d, 0x00000007, 0x00000012,
    0x00000010, 0x0007004f, 0x00000011, 0x00000013, 0x00000012, 0x00000012, 0x00000000, 0x00000001,
    0x0004006e, 0x00000015, 0x00000016, 0x00000013, 0x00040064, 0x0000000a, 0x00000018, 0x0000000e,
    0x0007005f, 0x00000007, 0x00000019, 0x00000018, 0x00000016, 0x00000002, 0x00000017, 0x0003003e,
    0x00000009, 0x00000019, 0x000100fd, 0x00010038,
};

#define GM_VK_FUNCS(X) \
    X(vkCreateImage) X(vkDestroyImage) X(vkGetImageMemoryRequirements) X(vkBindImageMemory) X(vkCreateImageView) X(vkDestroyImageView) \
    X(vkCreateBuffer) X(vkDestroyBuffer) X(vkGetBufferMemoryRequirements) X(vkBindBufferMemory) X(vkAllocateMemory) X(vkFreeMemory) \
    X(vkMapMemory) X(vkCreateSampler) X(vkDestroySampler) X(vkCreateDescriptorSetLayout) X(vkDestroyDescriptorSetLayout) \
    X(vkCreateDescriptorPool) X(vkDestroyDescriptorPool) X(vkAllocateDescriptorSets) X(vkUpdateDescriptorSets) \
    X(vkCreatePipelineLayout) X(vkDestroyPipelineLayout) X(vkCreateShaderModule) X(vkDestroyShaderModule) \
    X(vkCreateGraphicsPipelines) X(vkDestroyPipeline) X(vkCreateCommandPool) X(vkDestroyCommandPool) X(vkAllocateCommandBuffers) \
    X(vkBeginCommandBuffer) X(vkEndCommandBuffer) X(vkResetCommandBuffer) X(vkCmdPipelineBarrier) X(vkCmdCopyBufferToImage) \
    X(vkQueueSubmit) X(vkQueueWaitIdle) X(vkCreateFence) X(vkDestroyFence) X(vkWaitForFences) X(vkResetFences) \
    X(vkCmdBindPipeline) X(vkCmdBindDescriptorSets) X(vkCmdSetViewport) X(vkCmdSetScissor) X(vkCmdDraw)

typedef struct {
    goome *g;
#define X(n) PFN_##n n;
    GM_VK_FUNCS(X)
#undef X
    VkDevice dev;
    VkQueue queue;
    VkPhysicalDeviceMemoryProperties mp;
    VkSampler sampler;
    VkDescriptorSetLayout dsl;
    VkDescriptorPool pool;
    VkDescriptorSet set;
    VkPipelineLayout layout;
    VkPipeline pipe;
    VkCommandPool cpool;
    VkCommandBuffer cb;
    VkFence fence;
    VkImage img;
    VkImageView view;
    VkBuffer buf;
    VkDeviceMemory imem, bmem;
    void *map;
    int tw, th, fresh, pending;
} gm_vk;

static gm_vk gm_vk_all[8];

static gm_vk *gm_vk_find(goome *g)
{
    for (int i = 0; i < 8; i++)
        if (g && gm_vk_all[i].g == g) return &gm_vk_all[i];
    return NULL;
}

static PFN_vkGetInstanceProcAddr gm_vk_loader(void)
{
    void *p = NULL;
#ifdef _WIN32
    HMODULE m = LoadLibraryA("vulkan-1.dll");
    if (m) p = (void *)GetProcAddress(m, "vkGetInstanceProcAddr");
#else
    static const char *const names[] = {"libvulkan.so.1", "libvulkan.1.dylib", "libMoltenVK.dylib", "libvulkan.so"};
    for (int i = 0; i < 4 && !p; i++) {
        void *m = dlopen(names[i], RTLD_NOW | RTLD_LOCAL);
        if (m) p = dlsym(m, "vkGetInstanceProcAddr");
    }
#endif
    PFN_vkGetInstanceProcAddr f;
    memcpy(&f, &p, sizeof(f));
    return f;
}

static int gm_vk_memtype(gm_vk *d, uint32_t bits, VkMemoryPropertyFlags want)
{
    for (uint32_t i = 0; i < d->mp.memoryTypeCount; i++)
        if ((bits & (1u << i)) && (d->mp.memoryTypes[i].propertyFlags & want) == want) return (int)i;
    return -1;
}

static VkDeviceMemory gm_vk_alloc(gm_vk *d, VkMemoryRequirements mr, VkMemoryPropertyFlags want, VkMemoryPropertyFlags need)
{
    int t = gm_vk_memtype(d, mr.memoryTypeBits, want);
    if (t < 0) t = gm_vk_memtype(d, mr.memoryTypeBits, need);
    if (t < 0) return VK_NULL_HANDLE;
    VkMemoryAllocateInfo ai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, mr.size, (uint32_t)t};
    VkDeviceMemory m = VK_NULL_HANDLE;
    return d->vkAllocateMemory(d->dev, &ai, NULL, &m) == VK_SUCCESS ? m : VK_NULL_HANDLE;
}

static void gm_vk_drop_tex(gm_vk *d)
{
    if (d->pending) d->vkWaitForFences(d->dev, 1, &d->fence, VK_TRUE, UINT64_MAX), d->vkResetFences(d->dev, 1, &d->fence), d->pending = 0;
    if (d->img || d->buf) d->vkQueueWaitIdle(d->queue);
    if (d->view) d->vkDestroyImageView(d->dev, d->view, NULL);
    if (d->img) d->vkDestroyImage(d->dev, d->img, NULL);
    if (d->imem) d->vkFreeMemory(d->dev, d->imem, NULL);
    if (d->buf) d->vkDestroyBuffer(d->dev, d->buf, NULL);
    if (d->bmem) d->vkFreeMemory(d->dev, d->bmem, NULL);
    d->view = VK_NULL_HANDLE, d->img = VK_NULL_HANDLE, d->imem = VK_NULL_HANDLE, d->buf = VK_NULL_HANDLE, d->bmem = VK_NULL_HANDLE;
    d->map = NULL, d->tw = d->th = 0;
}

static int gm_vk_make_tex(gm_vk *d, int w, int h)
{
    gm_vk_drop_tex(d);
    VkImageCreateInfo ic;
    memset(&ic, 0, sizeof(ic));
    ic.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, ic.imageType = VK_IMAGE_TYPE_2D, ic.format = VK_FORMAT_B8G8R8A8_UNORM;
    ic.extent.width = (uint32_t)w, ic.extent.height = (uint32_t)h, ic.extent.depth = 1, ic.mipLevels = 1, ic.arrayLayers = 1;
    ic.samples = VK_SAMPLE_COUNT_1_BIT, ic.tiling = VK_IMAGE_TILING_OPTIMAL;
    ic.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, ic.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkBufferCreateInfo bc;
    memset(&bc, 0, sizeof(bc));
    bc.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, bc.size = (VkDeviceSize)w * h * 4, bc.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    VkMemoryRequirements mr;
    int ok = d->vkCreateImage(d->dev, &ic, NULL, &d->img) == VK_SUCCESS;
    if (ok) {
        d->vkGetImageMemoryRequirements(d->dev, d->img, &mr);
        d->imem = gm_vk_alloc(d, mr, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0);
        ok = d->imem && d->vkBindImageMemory(d->dev, d->img, d->imem, 0) == VK_SUCCESS;
    }
    if (ok) ok = d->vkCreateBuffer(d->dev, &bc, NULL, &d->buf) == VK_SUCCESS;
    if (ok) {
        VkMemoryPropertyFlags host = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        d->vkGetBufferMemoryRequirements(d->dev, d->buf, &mr);
        d->bmem = gm_vk_alloc(d, mr, host, host);
        ok = d->bmem && d->vkBindBufferMemory(d->dev, d->buf, d->bmem, 0) == VK_SUCCESS && d->vkMapMemory(d->dev, d->bmem, 0, VK_WHOLE_SIZE, 0, &d->map) == VK_SUCCESS;
    }
    if (ok) {
        VkImageViewCreateInfo vc;
        memset(&vc, 0, sizeof(vc));
        vc.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, vc.image = d->img, vc.viewType = VK_IMAGE_VIEW_TYPE_2D, vc.format = VK_FORMAT_B8G8R8A8_UNORM;
        vc.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, vc.subresourceRange.levelCount = 1, vc.subresourceRange.layerCount = 1;
        ok = d->vkCreateImageView(d->dev, &vc, NULL, &d->view) == VK_SUCCESS;
    }
    if (!ok) {
        gm_vk_drop_tex(d);
        return GOOME_ENOMEM;
    }
    VkDescriptorImageInfo ii = {d->sampler, d->view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkWriteDescriptorSet wr;
    memset(&wr, 0, sizeof(wr));
    wr.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, wr.dstSet = d->set, wr.descriptorCount = 1;
    wr.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, wr.pImageInfo = &ii;
    d->vkUpdateDescriptorSets(d->dev, 1, &wr, 0, NULL);
    d->tw = w, d->th = h, d->fresh = 1;
    return GOOME_OK;
}

static void gm_vk_free(gm_vk *d)
{
    gm_vk_drop_tex(d);
    if (d->pipe) d->vkDestroyPipeline(d->dev, d->pipe, NULL);
    if (d->layout) d->vkDestroyPipelineLayout(d->dev, d->layout, NULL);
    if (d->pool) d->vkDestroyDescriptorPool(d->dev, d->pool, NULL);
    if (d->dsl) d->vkDestroyDescriptorSetLayout(d->dev, d->dsl, NULL);
    if (d->sampler) d->vkDestroySampler(d->dev, d->sampler, NULL);
    if (d->fence) d->vkDestroyFence(d->dev, d->fence, NULL);
    if (d->cpool) d->vkDestroyCommandPool(d->dev, d->cpool, NULL);
    memset(d, 0, sizeof(*d));
}

static int gm_vk_setup(gm_vk *d, const goome_vulkan_info *in)
{
    VkDevice dev = d->dev;
    VkCommandPoolCreateInfo pc = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, NULL, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, in->queue_family};
    if (d->vkCreateCommandPool(dev, &pc, NULL, &d->cpool) != VK_SUCCESS) return 0;
    VkCommandBufferAllocateInfo ca = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, NULL, d->cpool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1};
    if (d->vkAllocateCommandBuffers(dev, &ca, &d->cb) != VK_SUCCESS) return 0;
    VkFenceCreateInfo fc = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, NULL, 0};
    if (d->vkCreateFence(dev, &fc, NULL, &d->fence) != VK_SUCCESS) return 0;
    VkSamplerCreateInfo sc;
    memset(&sc, 0, sizeof(sc));
    sc.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, sc.magFilter = sc.minFilter = VK_FILTER_NEAREST, sc.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    sc.addressModeU = sc.addressModeV = sc.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, sc.maxLod = 1;
    if (d->vkCreateSampler(dev, &sc, NULL, &d->sampler) != VK_SUCCESS) return 0;
    VkDescriptorSetLayoutBinding b = {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, NULL};
    VkDescriptorSetLayoutCreateInfo lc = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, NULL, 0, 1, &b};
    if (d->vkCreateDescriptorSetLayout(dev, &lc, NULL, &d->dsl) != VK_SUCCESS) return 0;
    VkDescriptorPoolSize ps = {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1};
    VkDescriptorPoolCreateInfo dc = {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, NULL, 0, 1, 1, &ps};
    if (d->vkCreateDescriptorPool(dev, &dc, NULL, &d->pool) != VK_SUCCESS) return 0;
    VkDescriptorSetAllocateInfo da = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, NULL, d->pool, 1, &d->dsl};
    if (d->vkAllocateDescriptorSets(dev, &da, &d->set) != VK_SUCCESS) return 0;
    VkPipelineLayoutCreateInfo plc = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, NULL, 0, 1, &d->dsl, 0, NULL};
    if (d->vkCreatePipelineLayout(dev, &plc, NULL, &d->layout) != VK_SUCCESS) return 0;

    VkShaderModule vs = VK_NULL_HANDLE, fs = VK_NULL_HANDLE;
    VkShaderModuleCreateInfo mc = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, NULL, 0, sizeof(gm_vk_vs), gm_vk_vs};
    int ok = d->vkCreateShaderModule(dev, &mc, NULL, &vs) == VK_SUCCESS;
    mc.codeSize = sizeof(gm_vk_fs), mc.pCode = gm_vk_fs;
    ok = ok && d->vkCreateShaderModule(dev, &mc, NULL, &fs) == VK_SUCCESS;
    if (ok) {
        VkPipelineShaderStageCreateInfo st[2];
        memset(st, 0, sizeof(st));
        st[0].sType = st[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        st[0].stage = VK_SHADER_STAGE_VERTEX_BIT, st[0].module = vs, st[0].pName = "main";
        st[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT, st[1].module = fs, st[1].pName = "main";
        VkPipelineVertexInputStateCreateInfo vi;
        memset(&vi, 0, sizeof(vi));
        vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        VkPipelineInputAssemblyStateCreateInfo ia;
        memset(&ia, 0, sizeof(ia));
        ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        VkPipelineViewportStateCreateInfo vp;
        memset(&vp, 0, sizeof(vp));
        vp.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, vp.viewportCount = 1, vp.scissorCount = 1;
        VkPipelineRasterizationStateCreateInfo rs;
        memset(&rs, 0, sizeof(rs));
        rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, rs.polygonMode = VK_POLYGON_MODE_FILL;
        rs.cullMode = VK_CULL_MODE_NONE, rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE, rs.lineWidth = 1;
        VkPipelineMultisampleStateCreateInfo ms;
        memset(&ms, 0, sizeof(ms));
        ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, ms.rasterizationSamples = in->samples ? in->samples : VK_SAMPLE_COUNT_1_BIT;
        VkPipelineDepthStencilStateCreateInfo ds;
        memset(&ds, 0, sizeof(ds));
        ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, ds.depthCompareOp = VK_COMPARE_OP_ALWAYS;
        ds.front.compareOp = ds.back.compareOp = VK_COMPARE_OP_ALWAYS;
        VkPipelineColorBlendAttachmentState ba;
        memset(&ba, 0, sizeof(ba));
        ba.blendEnable = VK_TRUE, ba.srcColorBlendFactor = ba.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        ba.dstColorBlendFactor = ba.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        ba.colorBlendOp = ba.alphaBlendOp = VK_BLEND_OP_ADD;
        ba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        VkPipelineColorBlendStateCreateInfo cb;
        memset(&cb, 0, sizeof(cb));
        cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, cb.attachmentCount = 1, cb.pAttachments = &ba;
        VkDynamicState dyn[2] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dy = {VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, NULL, 0, 2, dyn};
        VkGraphicsPipelineCreateInfo gp;
        memset(&gp, 0, sizeof(gp));
        gp.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, gp.stageCount = 2, gp.pStages = st;
        gp.pVertexInputState = &vi, gp.pInputAssemblyState = &ia, gp.pViewportState = &vp, gp.pRasterizationState = &rs;
        gp.pMultisampleState = &ms, gp.pDepthStencilState = &ds, gp.pColorBlendState = &cb, gp.pDynamicState = &dy;
        gp.layout = d->layout, gp.renderPass = in->render_pass, gp.subpass = in->subpass;
        ok = d->vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &gp, NULL, &d->pipe) == VK_SUCCESS;
    }
    if (vs) d->vkDestroyShaderModule(dev, vs, NULL);
    if (fs) d->vkDestroyShaderModule(dev, fs, NULL);
    return ok;
}

int goome_vulkan_init(goome *g, const goome_vulkan_info *in)
{
    if (!g || !in || !in->instance || !in->physical_device || !in->device || !in->queue || !in->render_pass) return GOOME_EARG;
    if (gm_vk_find(g)) return GOOME_OK;
    gm_vk *d = NULL;
    for (int i = 0; i < 8 && !d; i++)
        if (!gm_vk_all[i].g) d = &gm_vk_all[i];
    if (!d) return GOOME_ENOMEM;
    PFN_vkGetInstanceProcAddr gipa = in->get_proc ? in->get_proc : gm_vk_loader();
    if (!gipa) return GOOME_ENOTSUP;
    PFN_vkGetDeviceProcAddr gdpa = (PFN_vkGetDeviceProcAddr)gipa(in->instance, "vkGetDeviceProcAddr");
    PFN_vkGetPhysicalDeviceMemoryProperties gmp = (PFN_vkGetPhysicalDeviceMemoryProperties)gipa(in->instance, "vkGetPhysicalDeviceMemoryProperties");
    if (!gdpa || !gmp) return GOOME_ENOTSUP;
    memset(d, 0, sizeof(*d));
    int miss = 0;
#define X(n) \
    d->n = (PFN_##n)gdpa(in->device, #n); \
    miss |= !d->n;
    GM_VK_FUNCS(X)
#undef X
    if (miss) {
        memset(d, 0, sizeof(*d));
        return GOOME_ENOTSUP;
    }
    d->dev = in->device, d->queue = in->queue;
    gmp(in->physical_device, &d->mp);
    if (!gm_vk_setup(d, in)) {
        gm_vk_free(d);
        return GOOME_ENOTSUP;
    }
    d->g = g;
    return GOOME_OK;
}

static void gm_vk_barrier(gm_vk *d, VkImageLayout from, VkImageLayout to, VkPipelineStageFlags src, VkAccessFlags sa, VkPipelineStageFlags dst, VkAccessFlags da)
{
    VkImageMemoryBarrier b;
    memset(&b, 0, sizeof(b));
    b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, b.srcAccessMask = sa, b.dstAccessMask = da, b.oldLayout = from, b.newLayout = to;
    b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, b.image = d->img;
    b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, b.subresourceRange.levelCount = 1, b.subresourceRange.layerCount = 1;
    d->vkCmdPipelineBarrier(d->cb, src, dst, 0, 0, NULL, 0, NULL, 1, &b);
}

static int gm_vk_upload(gm_vk *d, goome_rect r)
{
    if (d->pending && d->vkWaitForFences(d->dev, 1, &d->fence, VK_TRUE, UINT64_MAX) != VK_SUCCESS) return 0;
    if (d->pending) d->vkResetFences(d->dev, 1, &d->fence), d->pending = 0;
    if (goome_frame_copy(d->g, d->map, r.w * 4, GOOME_FMT_BGRA_PREMUL, &r) != GOOME_OK) return 0;
    VkCommandBufferBeginInfo bi = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, NULL};
    d->vkResetCommandBuffer(d->cb, 0);
    if (d->vkBeginCommandBuffer(d->cb, &bi) != VK_SUCCESS) return 0;
    if (d->fresh)
        gm_vk_barrier(d, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
    else
        gm_vk_barrier(d, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
    VkBufferImageCopy c;
    memset(&c, 0, sizeof(c));
    c.bufferRowLength = (uint32_t)r.w, c.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, c.imageSubresource.layerCount = 1;
    c.imageOffset.x = r.x, c.imageOffset.y = r.y, c.imageExtent.width = (uint32_t)r.w, c.imageExtent.height = (uint32_t)r.h, c.imageExtent.depth = 1;
    d->vkCmdCopyBufferToImage(d->cb, d->buf, d->img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &c);
    gm_vk_barrier(d, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT);
    if (d->vkEndCommandBuffer(d->cb) != VK_SUCCESS) return 0;
    VkSubmitInfo si;
    memset(&si, 0, sizeof(si));
    si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, si.commandBufferCount = 1, si.pCommandBuffers = &d->cb;
    if (d->vkQueueSubmit(d->queue, 1, &si, d->fence) != VK_SUCCESS) return 0;
    d->pending = 1, d->fresh = 0;
    return 1;
}

void goome_vulkan_render(goome *g, VkCommandBuffer cmd)
{
    gm_vk *d = gm_vk_find(g);
    goome_image f;
    if (!d || !cmd || goome_frame(g, &f) != GOOME_OK || f.w <= 0 || f.h <= 0) return;
    goome_rect r;
    int dirty = goome_frame_dirty(g, &r);
    if ((!d->img || f.w != d->tw || f.h != d->th) && gm_vk_make_tex(d, f.w, f.h) != GOOME_OK) return;
    if (d->fresh) r.x = r.y = 0, r.w = f.w, r.h = f.h, dirty = 1;
    if (dirty && !gm_vk_upload(d, r) && d->fresh) return;
    VkViewport vp = {0, 0, (float)d->tw, (float)d->th, 0, 1};
    VkRect2D sc = {{0, 0}, {(uint32_t)d->tw, (uint32_t)d->th}};
    d->vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, d->pipe);
    d->vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, d->layout, 0, 1, &d->set, 0, NULL);
    d->vkCmdSetViewport(cmd, 0, 1, &vp);
    d->vkCmdSetScissor(cmd, 0, 1, &sc);
    d->vkCmdDraw(cmd, 4, 1, 0, 0);
}

void goome_vulkan_resize(goome *g, int w, int h, double scale)
{
    gm_vk *d = gm_vk_find(g);
    if (goome_resize(g, w, h, scale) == GOOME_OK && d) gm_vk_drop_tex(d);
}

void goome_vulkan_shutdown(goome *g)
{
    gm_vk *d = gm_vk_find(g);
    if (d) gm_vk_free(d);
}

#endif

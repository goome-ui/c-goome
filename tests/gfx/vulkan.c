#define VK_NO_PROTOTYPES
#define GOOME_IMPL_VULKAN_IMPLEMENTATION
#include "../../backends/goome_vulkan.h"

#include "scene.h"

#define INST_FUNCS(X) \
    X(vkEnumeratePhysicalDevices) X(vkGetPhysicalDeviceQueueFamilyProperties) X(vkGetPhysicalDeviceMemoryProperties) \
    X(vkGetPhysicalDeviceProperties) X(vkCreateDevice) X(vkDestroyInstance)

#define DEV_FUNCS(X) \
    X(vkGetDeviceQueue) X(vkCreateImage) X(vkGetImageMemoryRequirements) \
    X(vkAllocateMemory) X(vkBindImageMemory) X(vkCreateImageView) X(vkCreateRenderPass) X(vkCreateFramebuffer) X(vkCreateBuffer) \
    X(vkGetBufferMemoryRequirements) X(vkBindBufferMemory) X(vkMapMemory) X(vkCreateCommandPool) X(vkAllocateCommandBuffers) \
    X(vkBeginCommandBuffer) X(vkEndCommandBuffer) X(vkCmdBeginRenderPass) X(vkCmdEndRenderPass) X(vkCmdCopyImageToBuffer) \
    X(vkQueueSubmit) X(vkQueueWaitIdle) X(vkDeviceWaitIdle) X(vkDestroyDevice)

#define FUNCS(X) INST_FUNCS(X) DEV_FUNCS(X)

#define X(n) static PFN_##n n;
FUNCS(X)
#undef X

static VkPhysicalDeviceMemoryProperties mp;

static VkDeviceMemory memory(VkDevice dev, VkMemoryRequirements mr, VkMemoryPropertyFlags want)
{
    for (uint32_t i = 0; i < mp.memoryTypeCount; i++)
        if ((mr.memoryTypeBits & (1u << i)) && (mp.memoryTypes[i].propertyFlags & want) == want) {
            VkMemoryAllocateInfo ai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, mr.size, i};
            VkDeviceMemory m;
            return vkAllocateMemory(dev, &ai, NULL, &m) == VK_SUCCESS ? m : VK_NULL_HANDLE;
        }
    return VK_NULL_HANDLE;
}

static PFN_vkGetInstanceProcAddr find_vulkan(void)
{
    const char *icd = getenv("GOOME_VK_ICD");
    void *p = NULL;
    if (!icd || !*icd) {
        PFN_vkGetInstanceProcAddr f = gm_vk_loader();
        if (f) printf("vulkan loader\n");
        return f;
    }
#ifdef _WIN32
    HMODULE m = LoadLibraryA(icd);
    void *neg = m ? (void *)GetProcAddress(m, "vk_icdNegotiateLoaderICDInterfaceVersion") : NULL;
    if (m) p = (void *)GetProcAddress(m, "vk_icdGetInstanceProcAddr");
#else
    void *m = dlopen(icd, RTLD_NOW | RTLD_LOCAL);
    void *neg = m ? dlsym(m, "vk_icdNegotiateLoaderICDInterfaceVersion") : NULL;
    if (m) p = dlsym(m, "vk_icdGetInstanceProcAddr");
#endif
    if (neg) {
        VkResult (*nf)(uint32_t *);
        uint32_t v = 5;
        memcpy(&nf, &neg, sizeof(nf));
        nf(&v);
    }
    printf("vulkan icd %s\n", icd);
    PFN_vkGetInstanceProcAddr f;
    memcpy(&f, &p, sizeof(f));
    return f;
}

int main(void)
{
    PFN_vkGetInstanceProcAddr gipa = find_vulkan();
    if (!gipa) {
        printf("no vulkan\n");
        return 1;
    }
    PFN_vkCreateInstance create = (PFN_vkCreateInstance)gipa(NULL, "vkCreateInstance");
    VkApplicationInfo app = {VK_STRUCTURE_TYPE_APPLICATION_INFO, NULL, "gfx_vulkan", 1, "goome", 1, VK_API_VERSION_1_0};
    VkInstanceCreateInfo ic = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, NULL, 0, &app, 0, NULL, 0, NULL};
    VkInstance inst;
    if (!create || create(&ic, NULL, &inst) != VK_SUCCESS) {
        printf("no vulkan instance\n");
        return 1;
    }
    PFN_vkGetDeviceProcAddr gdpa = (PFN_vkGetDeviceProcAddr)gipa(inst, "vkGetDeviceProcAddr");
#define X(n) n = (PFN_##n)gipa(inst, #n);
    FUNCS(X)
#undef X
    uint32_t n = 1;
    VkPhysicalDevice pd;
    if (vkEnumeratePhysicalDevices(inst, &n, &pd) < 0 || !n) {
        printf("no vulkan device\n");
        return 1;
    }
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(pd, &props);
    printf("vulkan device %s\n", props.deviceName);
    vkGetPhysicalDeviceMemoryProperties(pd, &mp);
    VkQueueFamilyProperties qf[16];
    uint32_t nq = 16, fam = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(pd, &nq, qf);
    while (fam < nq && !(qf[fam].queueFlags & VK_QUEUE_GRAPHICS_BIT)) fam++;
    float prio = 1;
    VkDeviceQueueCreateInfo qc = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, NULL, 0, fam, 1, &prio};
    VkDeviceCreateInfo dc = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, NULL, 0, 1, &qc, 0, NULL, 0, NULL, NULL};
    VkDevice dev;
    if (vkCreateDevice(pd, &dc, NULL, &dev) != VK_SUCCESS) {
        printf("no vulkan logical device\n");
        return 1;
    }
#define X(n) \
    if (gdpa(dev, #n)) n = (PFN_##n)gdpa(dev, #n);
    DEV_FUNCS(X)
#undef X
    VkQueue q;
    vkGetDeviceQueue(dev, fam, 0, &q);

    VkImageCreateInfo imc;
    memset(&imc, 0, sizeof(imc));
    imc.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, imc.imageType = VK_IMAGE_TYPE_2D, imc.format = VK_FORMAT_B8G8R8A8_UNORM;
    imc.extent.width = SCENE_W, imc.extent.height = SCENE_H, imc.extent.depth = 1, imc.mipLevels = 1, imc.arrayLayers = 1;
    imc.samples = VK_SAMPLE_COUNT_1_BIT, imc.tiling = VK_IMAGE_TILING_OPTIMAL;
    imc.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    VkImage img;
    VkMemoryRequirements mr;
    vkCreateImage(dev, &imc, NULL, &img);
    vkGetImageMemoryRequirements(dev, img, &mr);
    vkBindImageMemory(dev, img, memory(dev, mr, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT), 0);
    VkImageViewCreateInfo vc;
    memset(&vc, 0, sizeof(vc));
    vc.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, vc.image = img, vc.viewType = VK_IMAGE_VIEW_TYPE_2D, vc.format = VK_FORMAT_B8G8R8A8_UNORM;
    vc.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, vc.subresourceRange.levelCount = 1, vc.subresourceRange.layerCount = 1;
    VkImageView view;
    vkCreateImageView(dev, &vc, NULL, &view);
    VkAttachmentDescription at;
    memset(&at, 0, sizeof(at));
    at.format = VK_FORMAT_B8G8R8A8_UNORM, at.samples = VK_SAMPLE_COUNT_1_BIT, at.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR, at.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    at.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE, at.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    at.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED, at.finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    VkAttachmentReference ref = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription sp;
    memset(&sp, 0, sizeof(sp));
    sp.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS, sp.colorAttachmentCount = 1, sp.pColorAttachments = &ref;
    VkSubpassDependency dep = {0, VK_SUBPASS_EXTERNAL, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                               VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, 0};
    VkRenderPassCreateInfo rc = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, NULL, 0, 1, &at, 1, &sp, 1, &dep};
    VkRenderPass rp;
    vkCreateRenderPass(dev, &rc, NULL, &rp);
    VkFramebufferCreateInfo fc = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, NULL, 0, rp, 1, &view, SCENE_W, SCENE_H, 1};
    VkFramebuffer fb;
    vkCreateFramebuffer(dev, &fc, NULL, &fb);
    VkBufferCreateInfo bc;
    memset(&bc, 0, sizeof(bc));
    bc.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, bc.size = SCENE_W * SCENE_H * 4, bc.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    VkBuffer rb;
    vkCreateBuffer(dev, &bc, NULL, &rb);
    vkGetBufferMemoryRequirements(dev, rb, &mr);
    VkDeviceMemory rbm = memory(dev, mr, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    vkBindBufferMemory(dev, rb, rbm, 0);
    void *px;
    vkMapMemory(dev, rbm, 0, VK_WHOLE_SIZE, 0, &px);
    VkCommandPoolCreateInfo pc = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, NULL, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, fam};
    VkCommandPool pool;
    vkCreateCommandPool(dev, &pc, NULL, &pool);
    VkCommandBufferAllocateInfo ca = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, NULL, pool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1};
    VkCommandBuffer cb;
    vkAllocateCommandBuffers(dev, &ca, &cb);

    scene s;
    if (scene_open(&s)) return 1;
    goome_vulkan_info info = {inst, pd, dev, fam, q, rp, 0, VK_SAMPLE_COUNT_1_BIT, gipa};
    if (goome_vulkan_init(s.g, &info) != GOOME_OK) {
        printf("goome_vulkan_init failed\n");
        return 1;
    }
    for (int frame = 0; frame <= 12; frame++) {
        if (frame == 10) goome_vulkan_resize(s.g, 300, 260, 1.25);
        scene_frame(&s, frame);
        VkCommandBufferBeginInfo bi = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, NULL};
        vkBeginCommandBuffer(cb, &bi);
        VkClearValue clear;
        clear.color.float32[0] = 0x33 / 255.0f, clear.color.float32[1] = 0x66 / 255.0f, clear.color.float32[2] = 0x99 / 255.0f, clear.color.float32[3] = 1;
        VkRenderPassBeginInfo rbi = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, NULL, rp, fb, {{0, 0}, {SCENE_W, SCENE_H}}, 1, &clear};
        vkCmdBeginRenderPass(cb, &rbi, VK_SUBPASS_CONTENTS_INLINE);
        goome_vulkan_render(s.g, cb);
        vkCmdEndRenderPass(cb);
        VkBufferImageCopy cp;
        memset(&cp, 0, sizeof(cp));
        cp.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, cp.imageSubresource.layerCount = 1;
        cp.imageExtent.width = SCENE_W, cp.imageExtent.height = SCENE_H, cp.imageExtent.depth = 1;
        vkCmdCopyImageToBuffer(cb, img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, rb, 1, &cp);
        vkEndCommandBuffer(cb);
        VkSubmitInfo si;
        memset(&si, 0, sizeof(si));
        si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, si.commandBufferCount = 1, si.pCommandBuffers = &cb;
        vkQueueSubmit(q, 1, &si, VK_NULL_HANDLE);
        vkQueueWaitIdle(q);
        if (frame != 0 && frame != 4 && frame != 6 && frame != 9 && frame != 12) continue;
        char tag[32];
        snprintf(tag, sizeof(tag), "vulkan frame %d", frame);
        scene_check(&s, px, SCENE_W * 4, 0, tag);
    }
    goome_vulkan_shutdown(s.g);
    printf("check %d volume %.2f window %.0f,%.0f\n", s.check, s.volume, s.wx, s.wy);
    int bad = s.bad;
    scene_close(&s);
    vkDeviceWaitIdle(dev);
    printf("%s\n", bad ? "FAIL" : "PASS");
    return bad;
}

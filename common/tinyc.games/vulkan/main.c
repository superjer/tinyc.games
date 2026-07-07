#ifndef VULKAN_DEMO_MAIN_C
#define VULKAN_DEMO_MAIN_C

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <time.h>
#include <math.h>
#include <assert.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>

#define STBI_NO_SIMD
#ifdef TINYC_VULKAN_TRIANGLE_MAIN
#define STB_IMAGE_IMPLEMENTATION
#endif
#include "../../nothings/stb_image.h"


#include "instance.c"
#include "physical.c"
#include "queue.c"
#include "device.c"
#include "surface.c"
#include "image.c"
#include "frame.c"
#include "shader.c"
#include "pipeline.c"
#include "command.c"
#include "synchronization.c"

struct vk {
        VkInstance instance;
        uint32_t physicalDeviceCount;
        VkPhysicalDevice *physicalDevices;
        uint32_t bestPhysicalDeviceIndex;
        VkPhysicalDevice *bestPhysicalDevice;
        uint32_t queueFamilyCount;
        VkQueueFamilyProperties *queueFamilyProperties;
        VkDevice device;
        uint32_t bestGraphicsQueueFamilyindex;
        drawAndPresentQueues graphicsQueueMode;
        VkQueue drawingQueue;
        VkQueue presentingQueue;
        SDL_Window *window;
        VkSurfaceKHR surface;
        VkSurfaceCapabilitiesKHR surfaceCapabilities;
        VkSurfaceFormatKHR bestSurfaceFormat;
        VkPresentModeKHR bestPresentMode;
        VkExtent2D bestSwapchainExtent;
        uint32_t imageArrayLayers;
        VkSwapchainKHR swapchain;
        uint32_t swapchainImageCount;
        VkImage *swapchainImages;
        VkImageView *swapchainImageViews;
        VkRenderPass renderPass;
        VkFramebuffer *framebuffers;
        VkCommandPool commandPool;
        VkCommandBuffer *commandBuffers;
        uint32_t maxFrames;
        VkSemaphore *waitSemaphores;
        VkSemaphore *signalSemaphores;
        VkFence *frontFences;
        VkFence *backFences;
        uint32_t imageIndex;
        uint32_t currentFrame;


        int pipelineCount;
        struct pipeline {
                VkPipelineLayout layout;
                VkPipeline pipeline;
                // Metadata for hot-reload
                char vert_src[256];  // GLSL source path (not .spv)
                char geom_src[256];
                char frag_src[256];
                SDL_Time vert_mtime;   // last modification time
                SDL_Time geom_mtime;
                SDL_Time frag_mtime;
                int bindingDescCount;
                int attributeDescCount;
                VkVertexInputBindingDescription bindingDescs[8];
                VkVertexInputAttributeDescription attributeDescs[16];
                VkDescriptorSetLayout *pDescriptorSetLayout;
                VkRenderPass renderPass;  // VK_NULL_HANDLE = use default
                int flags;
        } pipelines[100];

        // Depth buffer
        VkImage depthImage;
        VkDeviceMemory depthMemory;
        VkImageView depthImageView;

        // Shader source directory (for hot-reload)
        char shader_dir[256];

        VkClearColorValue clear_color;
} vk;

void vulkan_create_swapchain();
void vulkan_destroy_swapchain();
void vulkan_recreate_swapchain();

int vulkan_startup(char *window_title, char *icon_file, char *shader_dir, int win_w, int win_h)
{
        SDL_Init(SDL_INIT_VIDEO);

        vk.clear_color = (VkClearColorValue){{0.0f, 0.2f, 0.8f, 0.0f}};

        // Store shader source directory for hot-reload
        if (shader_dir)
                snprintf(vk.shader_dir, sizeof(vk.shader_dir), "%s", shader_dir);
        else
                vk.shader_dir[0] = '\0';

        vk.instance = createInstance();

        vk.physicalDeviceCount = getPhysicalDeviceNumber(&vk.instance);
        vk.physicalDevices = getPhysicalDevices(&vk.instance, vk.physicalDeviceCount);
        vk.bestPhysicalDeviceIndex = getBestPhysicalDeviceIndex(vk.physicalDevices, vk.physicalDeviceCount);
        vk.bestPhysicalDevice = &vk.physicalDevices[vk.bestPhysicalDeviceIndex];

        VkPhysicalDeviceMemoryProperties mem_props;
        vkGetPhysicalDeviceMemoryProperties(*vk.bestPhysicalDevice, &mem_props);

        for (uint32_t i = 0; i < mem_props.memoryHeapCount; i++) {
                if (mem_props.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
                        fprintf(stderr, "Heap %d: Total VRAM: %lu MB\n", i, 
                               mem_props.memoryHeaps[i].size / (1024 * 1024));
                }
        }

        vk.queueFamilyCount = getQueueFamilyNumber(vk.bestPhysicalDevice);
        vk.queueFamilyProperties = getQueueFamilyProperties(vk.bestPhysicalDevice, vk.queueFamilyCount);
        vk.bestGraphicsQueueFamilyindex = getBestGraphicsQueueFamilyIndex(vk.queueFamilyProperties, vk.queueFamilyCount);
        vk.graphicsQueueMode = getGraphicsQueueMode(vk.queueFamilyProperties, vk.bestGraphicsQueueFamilyindex);

        vk.device = createDevice(vk.bestPhysicalDevice, vk.bestGraphicsQueueFamilyindex, vk.queueFamilyProperties);

        // Print GPU info for debugging
        VkPhysicalDeviceProperties deviceProps;
        vkGetPhysicalDeviceProperties(*vk.bestPhysicalDevice, &deviceProps);
        fprintf(stderr, "Vulkan GPU: %s (driver %u.%u.%u)\n", deviceProps.deviceName,
                VK_VERSION_MAJOR(deviceProps.driverVersion),
                VK_VERSION_MINOR(deviceProps.driverVersion),
                VK_VERSION_PATCH(deviceProps.driverVersion));
        fprintf(stderr, "Queue family %u: %u queues, mode=%s\n",
                vk.bestGraphicsQueueFamilyindex,
                vk.queueFamilyProperties[vk.bestGraphicsQueueFamilyindex].queueCount,
                vk.graphicsQueueMode == 0 ? "SINGLE_QUEUE" : "SEPARATE_QUEUES");

        vk.drawingQueue = getDrawingQueue(&vk.device, vk.bestGraphicsQueueFamilyindex);
        vk.presentingQueue = getPresentingQueue(&vk.device, vk.bestGraphicsQueueFamilyindex, vk.graphicsQueueMode);
        deleteQueueFamilyProperties(&vk.queueFamilyProperties);

        vk.window = createVulkanWindow(win_w, win_h, window_title, icon_file);
        vk.surface = createSurface(vk.window, &vk.instance);
        VkBool32 surfaceSupported = getSurfaceSupport(&vk.surface, vk.bestPhysicalDevice, vk.bestGraphicsQueueFamilyindex);
        if (!surfaceSupported)
        {
                fprintf(stderr, "vulkan surface not supported!\n");
                exit(-1);
        }

        vk.bestSurfaceFormat = getBestSurfaceFormat(&vk.surface, vk.bestPhysicalDevice);
        vk.bestPresentMode = getBestPresentMode(&vk.surface, vk.bestPhysicalDevice);
        fprintf(stderr, "Present mode: %s\n",
                vk.bestPresentMode == VK_PRESENT_MODE_MAILBOX_KHR ? "MAILBOX" :
                vk.bestPresentMode == VK_PRESENT_MODE_FIFO_KHR ? "FIFO" :
                vk.bestPresentMode == VK_PRESENT_MODE_IMMEDIATE_KHR ? "IMMEDIATE" : "OTHER");
        vk.imageArrayLayers = 1;

        vk.renderPass = createRenderPass(&vk.device, &vk.bestSurfaceFormat);
        vk.commandPool = createCommandPool(&vk.device, vk.bestGraphicsQueueFamilyindex);

        vulkan_create_swapchain();

        vk.commandBuffers = createCommandBuffers(&vk.device, &vk.commandPool, vk.swapchainImageCount);

        vk.maxFrames = vk.swapchainImageCount;
        vk.waitSemaphores = createSemaphores(&vk.device, vk.swapchainImageCount);
        vk.signalSemaphores = createSemaphores(&vk.device, vk.swapchainImageCount);
        vk.frontFences = createFences(&vk.device, vk.swapchainImageCount);
        vk.backFences = createEmptyFences(vk.swapchainImageCount);
}

// Creates/recreates swapchain, depth buffer, and framebuffers
void vulkan_create_swapchain() {
        vk.surfaceCapabilities = getSurfaceCapabilities(&vk.surface, vk.bestPhysicalDevice);
        vk.bestSwapchainExtent = getBestSwapchainExtent(&vk.surfaceCapabilities, vk.window);

        vk.swapchain = createSwapchain(&vk.device, &vk.surface, &vk.surfaceCapabilities, &vk.bestSurfaceFormat, &vk.bestSwapchainExtent, &vk.bestPresentMode, vk.imageArrayLayers, vk.graphicsQueueMode);

        vk.swapchainImageCount = getSwapchainImageNumber(&vk.device, &vk.swapchain);
        vk.swapchainImages = getSwapchainImages(&vk.device, &vk.swapchain, vk.swapchainImageCount);
        vk.swapchainImageViews = createImageViews(&vk.device, &vk.swapchainImages, &vk.bestSurfaceFormat, vk.swapchainImageCount, vk.imageArrayLayers);

        // Create depth buffer
        VkImageCreateInfo depthImageInfo = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                .imageType = VK_IMAGE_TYPE_2D,
                .format = VK_FORMAT_D32_SFLOAT,
                .extent = { vk.bestSwapchainExtent.width, vk.bestSwapchainExtent.height, 1 },
                .mipLevels = 1,
                .arrayLayers = 1,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .tiling = VK_IMAGE_TILING_OPTIMAL,
                .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        };
        vkCreateImage(vk.device, &depthImageInfo, NULL, &vk.depthImage);

        VkMemoryRequirements depthMemReqs;
        vkGetImageMemoryRequirements(vk.device, vk.depthImage, &depthMemReqs);

        VkPhysicalDeviceMemoryProperties memProps;
        vkGetPhysicalDeviceMemoryProperties(*vk.bestPhysicalDevice, &memProps);
        uint32_t depthMemType = 0;
        for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
                if ((depthMemReqs.memoryTypeBits & (1 << i)) &&
                    (memProps.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
                        depthMemType = i;
                        break;
                }
        }

        VkMemoryAllocateInfo depthAllocInfo = {
                .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                .allocationSize = depthMemReqs.size,
                .memoryTypeIndex = depthMemType,
        };
        vkAllocateMemory(vk.device, &depthAllocInfo, NULL, &vk.depthMemory);
        vkBindImageMemory(vk.device, vk.depthImage, vk.depthMemory, 0);

        VkImageViewCreateInfo depthViewInfo = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .image = vk.depthImage,
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .format = VK_FORMAT_D32_SFLOAT,
                .subresourceRange = {
                        .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                        .baseMipLevel = 0,
                        .levelCount = 1,
                        .baseArrayLayer = 0,
                        .layerCount = 1,
                },
        };
        vkCreateImageView(vk.device, &depthViewInfo, NULL, &vk.depthImageView);

        vk.framebuffers = createFramebuffers(&vk.device, &vk.renderPass, &vk.bestSwapchainExtent, &vk.swapchainImageViews, vk.swapchainImageCount, &vk.depthImageView);
}

void vulkan_destroy_swapchain() {
        for (uint32_t i = 0; i < vk.swapchainImageCount; i++) {
                vkDestroyFramebuffer(vk.device, vk.framebuffers[i], NULL);
                vkDestroyImageView(vk.device, vk.swapchainImageViews[i], NULL);
        }
        free(vk.framebuffers);
        free(vk.swapchainImageViews);
        free(vk.swapchainImages);

        vkDestroyImageView(vk.device, vk.depthImageView, NULL);
        vkDestroyImage(vk.device, vk.depthImage, NULL);
        vkFreeMemory(vk.device, vk.depthMemory, NULL);

        vkDestroySwapchainKHR(vk.device, vk.swapchain, NULL);
}

void vulkan_recreate_swapchain() {
        vkDeviceWaitIdle(vk.device);
        vulkan_destroy_swapchain();
        vulkan_create_swapchain();
        fprintf(stderr, "Swapchain recreated: %dx%d\n", vk.bestSwapchainExtent.width, vk.bestSwapchainExtent.height);
}

// Get file modification time using SDL (cross-platform)
static SDL_Time get_file_mtime(const char *path) {
        SDL_PathInfo info;
        if (SDL_GetPathInfo(path, &info)) {
                return info.modify_time;
        }
        return 0;
}

// Build full source path from shader filename
// "sun.frag" -> "{shader_dir}sun.frag"
static void build_src_path(const char *filename, char *src, size_t src_size) {
        if (vk.shader_dir[0]) {
                snprintf(src, src_size, "%s%s", vk.shader_dir, filename);
        } else {
                snprintf(src, src_size, "%s", filename);
        }
}

// Compile a GLSL shader to SPIR-V using glslangValidator
// Returns 0 on success, non-zero on failure
static int compile_shader(const char *src_path, const char *spv_path) {
        char cmd[1024];
        snprintf(cmd, sizeof(cmd), "glslangValidator --quiet -V \"%s\" -o \"%s\" 2>&1", src_path, spv_path);
        int ret = system(cmd);
        if (ret != 0) {
                fprintf(stderr, "Shader compile failed: %s\n", src_path);
                // Run again without --quiet to show errors
                snprintf(cmd, sizeof(cmd), "glslangValidator -V \"%s\" -o \"%s\"", src_path, spv_path);
                system(cmd);
        }
        return ret;
}

// Internal: create/recreate pipeline at given index using stored metadata
// If index == vk.pipelineCount, this is a new pipeline; otherwise it's a reload
static int vulkan_create_pipeline_at(int index) {
        struct pipeline *p = &vk.pipelines[index];

        // Determine .spv paths from source paths
        char vert_spv[256], geom_spv[256], frag_spv[256];
        snprintf(vert_spv, sizeof(vert_spv), "shaders/%s.spv", strrchr(p->vert_src, '/') + 1);
        if (p->geom_src[0])
                snprintf(geom_spv, sizeof(geom_spv), "shaders/%s.spv", strrchr(p->geom_src, '/') + 1);
        snprintf(frag_spv, sizeof(frag_spv), "shaders/%s.spv", strrchr(p->frag_src, '/') + 1);

        uint32_t vertexShaderSize = 0;
        char *vertexShaderCode = getShaderCode(vert_spv, &vertexShaderSize);
        if (vertexShaderCode == VK_NULL_HANDLE) {
                fprintf(stderr, "vertex shader %s not found\n", vert_spv);
                return -1;
        }
        VkShaderModule vertexShaderModule = createShaderModule(&vk.device, vertexShaderCode, vertexShaderSize);

        VkShaderModule geometryShaderModule = VK_NULL_HANDLE;
        char *geometryShaderCode = NULL;
        if (p->geom_src[0]) {
                uint32_t geometryShaderSize = 0;
                geometryShaderCode = getShaderCode(geom_spv, &geometryShaderSize);
                if (geometryShaderCode == VK_NULL_HANDLE) {
                        fprintf(stderr, "geometry shader %s not found\n", geom_spv);
                        deleteShaderModule(&vk.device, &vertexShaderModule);
                        deleteShaderCode(&vertexShaderCode);
                        return -1;
                }
                geometryShaderModule = createShaderModule(&vk.device, geometryShaderCode, geometryShaderSize);
        }

        uint32_t fragmentShaderSize = 0;
        char *fragmentShaderCode = getShaderCode(frag_spv, &fragmentShaderSize);
        if (fragmentShaderCode == VK_NULL_HANDLE) {
                fprintf(stderr, "fragment shader %s not found\n", frag_spv);
                if (geometryShaderModule) {
                        deleteShaderModule(&vk.device, &geometryShaderModule);
                        deleteShaderCode(&geometryShaderCode);
                }
                deleteShaderModule(&vk.device, &vertexShaderModule);
                deleteShaderCode(&vertexShaderCode);
                return -1;
        }
        VkShaderModule fragmentShaderModule = createShaderModule(&vk.device, fragmentShaderCode, fragmentShaderSize);

        if (p->pDescriptorSetLayout)
                p->layout = createPipelineLayoutWithDescriptors(&vk.device, p->pDescriptorSetLayout);
        else
                p->layout = createPipelineLayout(&vk.device);

        VkRenderPass rp = p->renderPass ? p->renderPass : vk.renderPass;
        p->pipeline = createGraphicsPipeline(
                &vk.device,
                &p->layout,
                &vertexShaderModule,
                geometryShaderModule ? &geometryShaderModule : VK_NULL_HANDLE,
                &fragmentShaderModule,
                &rp,
                &vk.bestSwapchainExtent,
                p->bindingDescCount,
                p->bindingDescs,
                p->attributeDescCount,
                p->attributeDescs,
                p->flags
        );

        deleteShaderModule(&vk.device, &fragmentShaderModule);
        deleteShaderCode(&fragmentShaderCode);
        if (geometryShaderModule) {
                deleteShaderModule(&vk.device, &geometryShaderModule);
                deleteShaderCode(&geometryShaderCode);
        }
        deleteShaderModule(&vk.device, &vertexShaderModule);
        deleteShaderCode(&vertexShaderCode);

        return 0;
}

// Reload a single pipeline by recompiling its shaders
int vulkan_reload_pipeline(int index) {
        if (index < 0 || index >= vk.pipelineCount) {
                fprintf(stderr, "Invalid pipeline index: %d\n", index);
                return -1;
        }

        struct pipeline *p = &vk.pipelines[index];

        // Determine .spv paths
        char vert_spv[256], geom_spv[256], frag_spv[256];
        snprintf(vert_spv, sizeof(vert_spv), "shaders/%s.spv", strrchr(p->vert_src, '/') + 1);
        if (p->geom_src[0])
                snprintf(geom_spv, sizeof(geom_spv), "shaders/%s.spv", strrchr(p->geom_src, '/') + 1);
        snprintf(frag_spv, sizeof(frag_spv), "shaders/%s.spv", strrchr(p->frag_src, '/') + 1);

        // Compile shaders
        if (compile_shader(p->vert_src, vert_spv) != 0) return -1;
        if (p->geom_src[0] && compile_shader(p->geom_src, geom_spv) != 0) return -1;
        if (compile_shader(p->frag_src, frag_spv) != 0) return -1;

        // Wait for GPU to finish using old pipeline
        vkDeviceWaitIdle(vk.device);

        // Destroy old pipeline and layout
        deleteGraphicsPipeline(&vk.device, &p->pipeline);
        deletePipelineLayout(&vk.device, &p->layout);

        // Create new pipeline
        int result = vulkan_create_pipeline_at(index);

        // Update mtimes on success
        if (result == 0) {
                p->vert_mtime = get_file_mtime(p->vert_src);
                p->geom_mtime = p->geom_src[0] ? get_file_mtime(p->geom_src) : 0;
                p->frag_mtime = get_file_mtime(p->frag_src);
        }

        return result;
}

// Check if a pipeline's shaders have changed
static int pipeline_shaders_changed(int index) {
        struct pipeline *p = &vk.pipelines[index];

        SDL_Time vert_now = get_file_mtime(p->vert_src);
        SDL_Time frag_now = get_file_mtime(p->frag_src);
        SDL_Time geom_now = p->geom_src[0] ? get_file_mtime(p->geom_src) : 0;

        return (vert_now != p->vert_mtime) ||
               (frag_now != p->frag_mtime) ||
               (geom_now != p->geom_mtime);
}

// Reload only pipelines whose shaders have changed
void vulkan_reload_all_pipelines() {
        int changed = 0, failed = 0;

        for (int i = 0; i < vk.pipelineCount; i++) {
                if (!pipeline_shaders_changed(i))
                        continue;

                changed++;
                if (vulkan_reload_pipeline(i) != 0) {
                        fprintf(stderr, "Pipeline %d: FAILED\n", i);
                        failed++;
                } else {
                        fprintf(stderr, "Pipeline %d: reloaded\n", i);
                }
        }

        if (changed == 0)
                fprintf(stderr, "No shader changes detected\n");
        else
                fprintf(stderr, "Reloaded %d/%d pipelines\n", changed - failed, changed);
}

// Internal: store pipeline metadata for hot-reload
static void store_pipeline_metadata(int index, char *vert, char *geom, char *frag,
        int bindingDescCount, VkVertexInputBindingDescription *bindingDescs,
        int attributeDescCount, VkVertexInputAttributeDescription *attributeDescs,
        VkDescriptorSetLayout *pDescriptorSetLayout,
        VkRenderPass renderPass,
        int flags
) {
        struct pipeline *p = &vk.pipelines[index];

        // Store full source paths
        build_src_path(vert, p->vert_src, sizeof(p->vert_src));
        if (geom)
                build_src_path(geom, p->geom_src, sizeof(p->geom_src));
        else
                p->geom_src[0] = '\0';
        build_src_path(frag, p->frag_src, sizeof(p->frag_src));

        // Record initial modification times
        p->vert_mtime = get_file_mtime(p->vert_src);
        p->geom_mtime = geom ? get_file_mtime(p->geom_src) : 0;
        p->frag_mtime = get_file_mtime(p->frag_src);

        // Store vertex input info
        p->bindingDescCount = bindingDescCount;
        p->attributeDescCount = attributeDescCount;
        if (bindingDescCount > 0 && bindingDescs)
                memcpy(p->bindingDescs, bindingDescs, bindingDescCount * sizeof(VkVertexInputBindingDescription));
        if (attributeDescCount > 0 && attributeDescs)
                memcpy(p->attributeDescs, attributeDescs, attributeDescCount * sizeof(VkVertexInputAttributeDescription));

        p->pDescriptorSetLayout = pDescriptorSetLayout;
        p->renderPass = renderPass;
        p->flags = flags;
}

int vulkan_make_pipeline(char *vert, char *geom, char *frag,
        int bindingDescCount, VkVertexInputBindingDescription *bindingDescs,
        int attributeDescCount, VkVertexInputAttributeDescription *attributeDescs,
        VkDescriptorSetLayout *pDescriptorSetLayout,
        VkRenderPass renderPass,
        int flags
) {
        assert(vk.pipelineCount < 100);
        int index = vk.pipelineCount;

        // Store metadata for hot-reload (VK_NULL_HANDLE for default render pass)
        store_pipeline_metadata(index, vert, geom, frag,
                bindingDescCount, bindingDescs, attributeDescCount, attributeDescs,
                pDescriptorSetLayout, renderPass, flags);

        // Create the pipeline
        if (vulkan_create_pipeline_at(index) != 0) {
                fprintf(stderr, "Failed to create pipeline %d\n", index);
                return -1;
        }

        return vk.pipelineCount++;
}

void vulkan_start_recording(uint32_t imageIndex)
{
        int i = imageIndex;

        VkCommandBufferBeginInfo commandBufferBeginInfo;
        VkRenderPassBeginInfo renderPassBeginInfo;
        VkRect2D renderArea = {
                {0, 0},
                {vk.bestSwapchainExtent.width, vk.bestSwapchainExtent.height}
        };
        VkClearValue clearValues[2] = {
                {.color = vk.clear_color},
                {.depthStencil = {1.0f, 0}}
        };

        commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        commandBufferBeginInfo.pNext = VK_NULL_HANDLE;
        commandBufferBeginInfo.flags = 0;
        commandBufferBeginInfo.pInheritanceInfo = VK_NULL_HANDLE;

        renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassBeginInfo.pNext = VK_NULL_HANDLE;
        renderPassBeginInfo.renderPass = vk.renderPass;
        renderPassBeginInfo.framebuffer = vk.framebuffers[i];
        renderPassBeginInfo.renderArea = renderArea;
        renderPassBeginInfo.clearValueCount = 2;
        renderPassBeginInfo.pClearValues = clearValues;

        vkResetCommandBuffer(vk.commandBuffers[i], 0);
        vkBeginCommandBuffer(vk.commandBuffers[i], &commandBufferBeginInfo);
        vkCmdBeginRenderPass(vk.commandBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
}

void vulkan_finish_recording(uint32_t imageIndex)
{
        int i = imageIndex;
        vkCmdEndRenderPass(vk.commandBuffers[i]);
        vkEndCommandBuffer(vk.commandBuffers[i]);
}

void vulkan_acquire_next()
{
        // Use currentFrame to select which semaphore to use for acquisition
        vkWaitForFences(vk.device, 1, &vk.frontFences[vk.currentFrame], VK_TRUE, UINT64_MAX);
        vkAcquireNextImageKHR(vk.device, vk.swapchain, UINT64_MAX, vk.waitSemaphores[vk.currentFrame], VK_NULL_HANDLE, &vk.imageIndex);

        // Wait for any previous work on this specific image to complete
        if(vk.backFences[vk.imageIndex] != VK_NULL_HANDLE){
                vkWaitForFences(vk.device, 1, &vk.backFences[vk.imageIndex], VK_TRUE, UINT64_MAX);
        }

        vulkan_start_recording(vk.imageIndex);
}

void vulkan_submit()
{
        vulkan_finish_recording(vk.imageIndex);

        VkPipelineStageFlags pipelineStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo submitInfo = {
                VK_STRUCTURE_TYPE_SUBMIT_INFO,
                VK_NULL_HANDLE,
                1,
                &vk.waitSemaphores[vk.currentFrame],
                &pipelineStage,
                1,
                &vk.commandBuffers[vk.imageIndex],
                1,
                &vk.signalSemaphores[vk.imageIndex]  // Use imageIndex for signal semaphore
        };
        vkResetFences(vk.device, 1, &vk.frontFences[vk.currentFrame]);
        vkQueueSubmit(vk.drawingQueue, 1, &submitInfo, vk.frontFences[vk.currentFrame]);

        // Track which fence is now associated with this image
        vk.backFences[vk.imageIndex] = vk.frontFences[vk.currentFrame];

        VkPresentInfoKHR presentInfo = {
                VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                VK_NULL_HANDLE,
                1,
                &vk.signalSemaphores[vk.imageIndex],  // Use imageIndex for present
                1,
                &vk.swapchain,
                &vk.imageIndex,
                VK_NULL_HANDLE
        };
        vkQueuePresentKHR(vk.presentingQueue, &presentInfo);

        vk.currentFrame = (vk.currentFrame + 1) % vk.maxFrames;
}

void vulkan_shutdown()
{
        vkDeviceWaitIdle(vk.device);
        deleteEmptyFences(&vk.backFences);
        deleteFences(&vk.device, &vk.frontFences, vk.maxFrames);
        deleteSemaphores(&vk.device, &vk.signalSemaphores, vk.maxFrames);
        deleteSemaphores(&vk.device, &vk.waitSemaphores, vk.maxFrames);
        deleteCommandBuffers(&vk.device, vk.commandBuffers, &vk.commandPool, vk.swapchainImageCount);
        deleteCommandPool(&vk.device, &vk.commandPool);
        while (--vk.pipelineCount >= 0)
        {
                deleteGraphicsPipeline(&vk.device, &vk.pipelines[vk.pipelineCount].pipeline);
                deletePipelineLayout(&vk.device, &vk.pipelines[vk.pipelineCount].layout);
        }
        deleteFramebuffers(&vk.device, &vk.framebuffers, vk.swapchainImageCount);
        deleteRenderPass(&vk.device, &vk.renderPass);
        vkDestroyImageView(vk.device, vk.depthImageView, NULL);
        vkDestroyImage(vk.device, vk.depthImage, NULL);
        vkFreeMemory(vk.device, vk.depthMemory, NULL);
        deleteImageViews(&vk.device, &vk.swapchainImageViews, vk.swapchainImageCount);
        deleteSwapchainImages(&vk.swapchainImages);
        deleteSwapchain(&vk.device, &vk.swapchain);
        deleteSurface(&vk.surface, &vk.instance);
        deleteWindow(vk.window);
        deleteDevice(&vk.device);
        deletePhysicalDevices(&vk.physicalDevices);
        deleteInstance(&vk.instance);
}

void myDrawCallback(VkCommandBuffer cmdbuf)
{
        vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipelines[0].pipeline);

        VkViewport viewport = { 0, 0, vk.bestSwapchainExtent.width, vk.bestSwapchainExtent.height, 0, 1 };
        VkRect2D scissor = { {0, 0}, {vk.bestSwapchainExtent.width, vk.bestSwapchainExtent.height} };
        vkCmdSetViewport(cmdbuf, 0, 1, &viewport);
        vkCmdSetScissor(cmdbuf, 0, 1, &scissor);

        vkCmdDraw(cmdbuf, 3, 1, 0, 0);
}

#ifdef TINYC_VULKAN_TRIANGLE_MAIN
int main()
{
        vulkan_startup("TinyC.Games Vulkan test", "../assets/tinyc-icon.png", "./shaders/", 1440, 900);
        int pipe = vulkan_make_pipeline(
                "triangle.vert",
                "triangle.geom",
                "triangle.frag",
                0, NULL, 0, NULL, NULL, VK_NULL_HANDLE, 0
        );

        SDL_Event event;
        bool running = true;

        while (running) {
                while (SDL_PollEvent(&event)) {
                        if (event.type == SDL_EVENT_QUIT)
                                running = false;
                }
                vulkan_acquire_next();
                myDrawCallback(vk.commandBuffers[vk.imageIndex]);
                vulkan_submit();
        }

        vulkan_shutdown();
}
#endif

#endif //VULKAN_DEMO_MAIN_C

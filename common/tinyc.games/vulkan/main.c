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
        } pipelines[100];
} vk;

int vulkan_startup()
{
        SDL_Init(SDL_INIT_VIDEO);

        vk.instance = createInstance();

        vk.physicalDeviceCount = getPhysicalDeviceNumber(&vk.instance);
        vk.physicalDevices = getPhysicalDevices(&vk.instance, vk.physicalDeviceCount);
        vk.bestPhysicalDeviceIndex = getBestPhysicalDeviceIndex(vk.physicalDevices, vk.physicalDeviceCount);
        vk.bestPhysicalDevice = &vk.physicalDevices[vk.bestPhysicalDeviceIndex];

        vk.queueFamilyCount = getQueueFamilyNumber(vk.bestPhysicalDevice);
        vk.queueFamilyProperties = getQueueFamilyProperties(vk.bestPhysicalDevice, vk.queueFamilyCount);
        vk.bestGraphicsQueueFamilyindex = getBestGraphicsQueueFamilyIndex(vk.queueFamilyProperties, vk.queueFamilyCount);
        vk.graphicsQueueMode = getGraphicsQueueMode(vk.queueFamilyProperties, vk.bestGraphicsQueueFamilyindex);

        vk.device = createDevice(vk.bestPhysicalDevice, vk.bestGraphicsQueueFamilyindex, vk.queueFamilyProperties);

        vk.drawingQueue = getDrawingQueue(&vk.device, vk.bestGraphicsQueueFamilyindex);
        vk.presentingQueue = getPresentingQueue(&vk.device, vk.bestGraphicsQueueFamilyindex, vk.graphicsQueueMode);
        deleteQueueFamilyProperties(&vk.queueFamilyProperties);

        char windowTitle[] = "Vulkan Triangle";
        vk.window = createVulkanWindow(1440, 900, windowTitle);
        vk.surface = createSurface(vk.window, &vk.instance);
        VkBool32 surfaceSupported = getSurfaceSupport(&vk.surface, vk.bestPhysicalDevice, vk.bestGraphicsQueueFamilyindex);
        if (!surfaceSupported)
        {
                fprintf(stderr, "vulkan surface not supported!\n");
                exit(-1);
        }

        vk.surfaceCapabilities = getSurfaceCapabilities(&vk.surface, vk.bestPhysicalDevice);
        vk.bestSurfaceFormat = getBestSurfaceFormat(&vk.surface, vk.bestPhysicalDevice);
        vk.bestPresentMode = getBestPresentMode(&vk.surface, vk.bestPhysicalDevice);
        vk.bestSwapchainExtent = getBestSwapchainExtent(&vk.surfaceCapabilities, vk.window);
        vk.imageArrayLayers = 1;
        vk.swapchain = createSwapchain(&vk.device, &vk.surface, &vk.surfaceCapabilities, &vk.bestSurfaceFormat, &vk.bestSwapchainExtent, &vk.bestPresentMode, vk.imageArrayLayers, vk.graphicsQueueMode);

        vk.swapchainImageCount = getSwapchainImageNumber(&vk.device, &vk.swapchain);
        vk.swapchainImages = getSwapchainImages(&vk.device, &vk.swapchain, vk.swapchainImageCount);

        vk.swapchainImageViews = createImageViews(&vk.device, &vk.swapchainImages, &vk.bestSurfaceFormat, vk.swapchainImageCount, vk.imageArrayLayers);

        vk.renderPass = createRenderPass(&vk.device, &vk.bestSurfaceFormat);
        vk.framebuffers = createFramebuffers(&vk.device, &vk.renderPass, &vk.bestSwapchainExtent, &vk.swapchainImageViews, vk.swapchainImageCount);

        vk.commandPool = createCommandPool(&vk.device, vk.bestGraphicsQueueFamilyindex);
        vk.commandBuffers = createCommandBuffers(&vk.device, &vk.commandPool, vk.swapchainImageCount);

        vk.maxFrames = 2;
        vk.waitSemaphores = createSemaphores(&vk.device, vk.maxFrames);
        vk.signalSemaphores = createSemaphores(&vk.device, vk.maxFrames);
        vk.frontFences = createFences(&vk.device, vk.maxFrames);
        vk.backFences = createEmptyFences(vk.swapchainImageCount);
}

int vulkan_make_pipeline(char *vert, char *geom, char *frag, 
        int bindingDescCount, VkVertexInputBindingDescription *bindingDescs,
        int attributeDescCount, VkVertexInputAttributeDescription *attributeDescs
) {
        assert(vk.pipelineCount < 100);

        uint32_t vertexShaderSize = 0;
        char *vertexShaderCode = getShaderCode(vert, &vertexShaderSize);
        if (vertexShaderCode == VK_NULL_HANDLE){
                fprintf(stderr, "vertex shader %s not found\n", vert);
                goto vert_shader_fail;
        }
        VkShaderModule vertexShaderModule = createShaderModule(&vk.device, vertexShaderCode, vertexShaderSize);

        uint32_t geometryShaderSize = 0;
        char *geometryShaderCode;
        VkShaderModule geometryShaderModule;
        if (geom){
                geometryShaderCode = getShaderCode(geom, &geometryShaderSize);
                if (geometryShaderCode == VK_NULL_HANDLE){
                        fprintf(stderr, "geometry shader %s not found\n", geom);
                        goto geom_shader_fail;
                }
                geometryShaderModule = createShaderModule(&vk.device, geometryShaderCode, geometryShaderSize);
        }

        uint32_t fragmentShaderSize = 0;
        char *fragmentShaderCode = getShaderCode(frag, &fragmentShaderSize);
        if(fragmentShaderCode == VK_NULL_HANDLE){
                fprintf(stderr, "fragment shader %s not found\n", frag);
                goto frag_shader_fail;
        }
        VkShaderModule fragmentShaderModule = createShaderModule(&vk.device, fragmentShaderCode, fragmentShaderSize);

        VkVertexInputAttributeDescription attributeDesc = {};
        attributeDesc.location = 0;
        attributeDesc.binding = 0;
        attributeDesc.format = VK_FORMAT_R32G32_SFLOAT; // Matches vec2
        attributeDesc.offset = 0;
        VkVertexInputBindingDescription bindingDesc = {};
        bindingDesc.binding = 0; // Must match attribute binding
        bindingDesc.stride = sizeof(float) * 2; // vec2: 2 floats
        bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        vk.pipelines[vk.pipelineCount].layout = createPipelineLayout(&vk.device);
        vk.pipelines[vk.pipelineCount].pipeline = createGraphicsPipeline(
                &vk.device,
                &vk.pipelines[vk.pipelineCount].layout,
                &vertexShaderModule,
                geom ? &geometryShaderModule : VK_NULL_HANDLE,
                &fragmentShaderModule,
                &vk.renderPass,
                &vk.bestSwapchainExtent,
                bindingDescCount,
                bindingDescs,
                attributeDescCount,
                attributeDescs
        );

        deleteShaderModule(&vk.device, &fragmentShaderModule);
        deleteShaderCode(&fragmentShaderCode);
        frag_shader_fail:
        if (geom) {
                deleteShaderModule(&vk.device, &geometryShaderModule);
                deleteShaderCode(&geometryShaderCode);
        }
        geom_shader_fail:
        deleteShaderModule(&vk.device, &vertexShaderModule);
        deleteShaderCode(&vertexShaderCode);
        vert_shader_fail:

        return vk.pipelineCount++;
}

void vulkan_start_recording(uint32_t imageIndex)
{
        int i = imageIndex;

        // we're not actually going to use 8, this is just an upper bound
        VkCommandBufferBeginInfo commandBufferBeginInfo;
        VkRenderPassBeginInfo renderPassBeginInfo;
        VkRect2D renderArea = {
                {0, 0},
                {vk.bestSwapchainExtent.width, vk.bestSwapchainExtent.height}
        };
        VkClearValue clearValue = {0.0f, 0.2f, 0.8f, 0.0f};

        commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        commandBufferBeginInfo.pNext = VK_NULL_HANDLE;
        commandBufferBeginInfo.flags = 0;
        commandBufferBeginInfo.pInheritanceInfo = VK_NULL_HANDLE;

        renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassBeginInfo.pNext = VK_NULL_HANDLE;
        renderPassBeginInfo.renderPass = vk.renderPass;
        renderPassBeginInfo.framebuffer = vk.framebuffers[i];
        renderPassBeginInfo.renderArea = renderArea;
        renderPassBeginInfo.clearValueCount = 1;
        renderPassBeginInfo.pClearValues = &clearValue;

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
        vkWaitForFences(vk.device, 1, &vk.frontFences[vk.currentFrame], VK_TRUE, UINT64_MAX);
        vkAcquireNextImageKHR(vk.device, vk.swapchain, UINT64_MAX, vk.waitSemaphores[vk.currentFrame], VK_NULL_HANDLE, &vk.imageIndex);
        if(vk.backFences[vk.imageIndex] != VK_NULL_HANDLE){
                vkWaitForFences(vk.device, 1, &vk.backFences[vk.imageIndex], VK_TRUE, UINT64_MAX);
        }
        vk.backFences[vk.imageIndex] = vk.frontFences[vk.currentFrame];

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
                &vk.signalSemaphores[vk.currentFrame]
        };
        vkResetFences(vk.device, 1, &vk.frontFences[vk.currentFrame]);
        vkQueueSubmit(vk.drawingQueue, 1, &submitInfo, vk.frontFences[vk.currentFrame]);

        VkPresentInfoKHR presentInfo = {
                VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                VK_NULL_HANDLE,
                1,
                &vk.signalSemaphores[vk.currentFrame],
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
        vkCmdDraw(cmdbuf, 3, 1, 0, 0);
}

#ifndef TCGVK_SKIP_MAIN
int main()
{
        vulkan_startup();
        int pipe = vulkan_make_pipeline(
                "shaders/triangle_vertex.spv",
                "shaders/triangle_geometry.spv",
                "shaders/triangle_fragment.spv",
                0, NULL, 0, NULL
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

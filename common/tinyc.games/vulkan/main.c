#ifndef VULKAN_DEMO_MAIN_C
#define VULKAN_DEMO_MAIN_C

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <time.h>
#include <math.h>

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

        // need multiple
        VkPipelineLayout pipelineLayout;
        VkPipeline graphicsPipeline;
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

void vulkan_make_pipeline(char *vert, char *geom, char *frag)
{
	uint32_t vertexShaderSize = 0;
	char *vertexShaderCode = getShaderCode(vert, &vertexShaderSize);
	if (vertexShaderCode == VK_NULL_HANDLE){
		fprintf(stderr, "vertex shader %s not found\n", vert);
                goto vert_shader_fail;
	}
	VkShaderModule vertexShaderModule = createShaderModule(&vk.device, vertexShaderCode, vertexShaderSize);

	uint32_t geometryShaderSize = 0;
	char *geometryShaderCode = getShaderCode(geom, &geometryShaderSize);
	if (geometryShaderCode == VK_NULL_HANDLE){
		fprintf(stderr, "geometry shader %s not found\n", geom);
                goto geom_shader_fail;
	}
	VkShaderModule geometryShaderModule = createShaderModule(&vk.device, geometryShaderCode, geometryShaderSize);

	uint32_t fragmentShaderSize = 0;
	char *fragmentShaderCode = getShaderCode(frag, &fragmentShaderSize);
	if(fragmentShaderCode == VK_NULL_HANDLE){
		fprintf(stderr, "fragment shader %s not found\n", frag);
                goto frag_shader_fail;
	}
	VkShaderModule fragmentShaderModule = createShaderModule(&vk.device, fragmentShaderCode, fragmentShaderSize);

	vk.pipelineLayout = createPipelineLayout(&vk.device);
	vk.graphicsPipeline = createGraphicsPipeline(
                &vk.device,
                &vk.pipelineLayout,
                &vertexShaderModule,
                &geometryShaderModule,
                &fragmentShaderModule,
                &vk.renderPass,
                &vk.bestSwapchainExtent
        );

	deleteShaderModule(&vk.device, &fragmentShaderModule);
	deleteShaderCode(&fragmentShaderCode);
        frag_shader_fail:
	deleteShaderModule(&vk.device, &geometryShaderModule);
	deleteShaderCode(&geometryShaderCode);
        geom_shader_fail:
	deleteShaderModule(&vk.device, &vertexShaderModule);
	deleteShaderCode(&vertexShaderCode);
        vert_shader_fail:
        return;
}

void vulkan_record_commands()
{
        // we're not actually going to use 8, this is just an upper bound
	VkCommandBufferBeginInfo commandBufferBeginInfos[8] = {0};
	VkRenderPassBeginInfo renderPassBeginInfos[8] = {0};

	VkRect2D renderArea = {
		{0, 0},
		{vk.bestSwapchainExtent.width, vk.bestSwapchainExtent.height}
	};
	VkClearValue clearValue = {0.9f, 0.2f, 0.8f, 0.0f};

	for (uint32_t i = 0; i < vk.swapchainImageCount; i++){
		commandBufferBeginInfos[i].sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		commandBufferBeginInfos[i].pNext = VK_NULL_HANDLE;
		commandBufferBeginInfos[i].flags = 0;
		commandBufferBeginInfos[i].pInheritanceInfo = VK_NULL_HANDLE;

		renderPassBeginInfos[i].sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassBeginInfos[i].pNext = VK_NULL_HANDLE;
		renderPassBeginInfos[i].renderPass = vk.renderPass;
		renderPassBeginInfos[i].framebuffer = vk.framebuffers[i];
		renderPassBeginInfos[i].renderArea = renderArea;
		renderPassBeginInfos[i].clearValueCount = 1;
		renderPassBeginInfos[i].pClearValues = &clearValue;

		vkBeginCommandBuffer(vk.commandBuffers[i], &commandBufferBeginInfos[i]);
		vkCmdBeginRenderPass(vk.commandBuffers[i], &renderPassBeginInfos[i], VK_SUBPASS_CONTENTS_INLINE);
		vkCmdBindPipeline(vk.commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, vk.graphicsPipeline);
		vkCmdDraw(vk.commandBuffers[i], 3, 1, 0, 0);
		vkCmdEndRenderPass(vk.commandBuffers[i]);
		vkEndCommandBuffer(vk.commandBuffers[i]);
	}
}

void vulkan_present()
{
	static uint32_t currentFrame = 0;

        vkWaitForFences(vk.device, 1, &vk.frontFences[currentFrame], VK_TRUE, UINT64_MAX);
        uint32_t imageIndex = 0;
        vkAcquireNextImageKHR(vk.device, vk.swapchain, UINT64_MAX, vk.waitSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);
        if(vk.backFences[imageIndex] != VK_NULL_HANDLE){
                vkWaitForFences(vk.device, 1, &vk.backFences[imageIndex], VK_TRUE, UINT64_MAX);
        }
        vk.backFences[imageIndex] = vk.frontFences[currentFrame];

        VkPipelineStageFlags pipelineStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

        VkSubmitInfo submitInfo = {
                VK_STRUCTURE_TYPE_SUBMIT_INFO,
                VK_NULL_HANDLE,
                1,
                &vk.waitSemaphores[currentFrame],
                &pipelineStage,
                1,
                &vk.commandBuffers[imageIndex],
                1,
                &vk.signalSemaphores[currentFrame]
        };
        vkResetFences(vk.device, 1, &vk.frontFences[currentFrame]);
        vkQueueSubmit(vk.drawingQueue, 1, &submitInfo, vk.frontFences[currentFrame]);

        VkPresentInfoKHR presentInfo = {
                VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                VK_NULL_HANDLE,
                1,
                &vk.signalSemaphores[currentFrame],
                1,
                &vk.swapchain,
                &imageIndex,
                VK_NULL_HANDLE
        };
        vkQueuePresentKHR(vk.presentingQueue, &presentInfo);

        currentFrame = (currentFrame + 1) % vk.maxFrames;
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
	deleteGraphicsPipeline(&vk.device, &vk.graphicsPipeline);
	deletePipelineLayout(&vk.device, &vk.pipelineLayout);
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

#ifndef TCGVK_SKIP_MAIN
int main()
{
        vulkan_startup();
        vulkan_make_pipeline("shaders/triangle_vertex.spv", "shaders/triangle_geometry.spv", "shaders/triangle_fragment.spv");
        vulkan_record_commands();

        SDL_Event event;
        bool running = true;

	while (running) {
                while (SDL_PollEvent(&event)) {
                        if (event.type == SDL_EVENT_QUIT)
                                running = false;
                }
                vulkan_present();
	}

        vulkan_shutdown();
}
#endif

#endif //VULKAN_DEMO_MAIN_C

#ifndef VULKAN_DEMO_MAIN_C
#define VULKAN_DEMO_MAIN_C

#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "limits.h"
#include "time.h"
#include "math.h"

#include "vulkan/vulkan.h"
#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"

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
#include "present.c"

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
        GLFWwindow *window;
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
} vk;

int main(){
	glfwInit();

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
	vk.window = createVulkanWindow(600, 600, windowTitle);
	vk.surface = createSurface(vk.window, &vk.instance);
	VkBool32 surfaceSupported = getSurfaceSupport(&vk.surface, vk.bestPhysicalDevice, vk.bestGraphicsQueueFamilyindex);
	if (!surfaceSupported)
        {
		fprintf(stderr, "vulkan surface not supported!\n");
                goto surf_not_support;
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

	uint32_t vertexShaderSize = 0;
	char vertexShaderFileName[] = "shaders/triangle_vertex.spv";
	char *vertexShaderCode = getShaderCode(vertexShaderFileName, &vertexShaderSize);
	if (vertexShaderCode == VK_NULL_HANDLE){
		fprintf(stderr, "vertex shader %s not found\n", vertexShaderFileName);
                goto shader_fail;
	}
	VkShaderModule vertexShaderModule = createShaderModule(&vk.device, vertexShaderCode, vertexShaderSize);

	uint32_t fragmentShaderSize = 0;
	char fragmentShaderFileName[] = "shaders/triangle_fragment.spv";
	char *fragmentShaderCode = getShaderCode(fragmentShaderFileName, &fragmentShaderSize);
	if(fragmentShaderCode == VK_NULL_HANDLE){
		fprintf(stderr, "fragment shader %s not found\n", fragmentShaderFileName);
		deleteShaderModule(&vk.device, &vertexShaderModule);
		deleteShaderCode(&vertexShaderCode);
                goto shader_fail;
	}
	VkShaderModule fragmentShaderModule = createShaderModule(&vk.device, fragmentShaderCode, fragmentShaderSize);

	VkPipelineLayout pipelineLayout = createPipelineLayout(&vk.device);
	VkPipeline graphicsPipeline = createGraphicsPipeline(&vk.device, &pipelineLayout, &vertexShaderModule, &fragmentShaderModule, &vk.renderPass, &vk.bestSwapchainExtent);

	deleteShaderModule(&vk.device, &fragmentShaderModule);
	deleteShaderCode(&fragmentShaderCode);
	deleteShaderModule(&vk.device, &vertexShaderModule);
	deleteShaderCode(&vertexShaderCode);

	VkCommandPool commandPool = createCommandPool(&vk.device, vk.bestGraphicsQueueFamilyindex);
	VkCommandBuffer *commandBuffers = createCommandBuffers(&vk.device, &commandPool, vk.swapchainImageCount);
	recordCommandBuffers(commandBuffers, &vk.renderPass, vk.framebuffers, &vk.bestSwapchainExtent, &graphicsPipeline, vk.swapchainImageCount);

	uint32_t maxFrames = 2;
	VkSemaphore *waitSemaphores = createSemaphores(&vk.device, maxFrames);
        VkSemaphore *signalSemaphores = createSemaphores(&vk.device, maxFrames);
	VkFence *frontFences = createFences(&vk.device, maxFrames);
        VkFence *backFences = createEmptyFences(vk.swapchainImageCount);

	presentImage(&vk.device, vk.window, commandBuffers, frontFences, backFences, waitSemaphores, signalSemaphores, &vk.swapchain, &vk.drawingQueue, &vk.presentingQueue, maxFrames);

	deleteEmptyFences(&backFences);
	deleteFences(&vk.device, &frontFences, maxFrames);
	deleteSemaphores(&vk.device, &signalSemaphores, maxFrames);
	deleteSemaphores(&vk.device, &waitSemaphores, maxFrames);
	deleteCommandBuffers(&vk.device, commandBuffers, &commandPool, vk.swapchainImageCount);
	deleteCommandPool(&vk.device, &commandPool);
	deleteGraphicsPipeline(&vk.device, &graphicsPipeline);
	deletePipelineLayout(&vk.device, &pipelineLayout);
        shader_fail:
	deleteFramebuffers(&vk.device, &vk.framebuffers, vk.swapchainImageCount);
	deleteRenderPass(&vk.device, &vk.renderPass);
	deleteImageViews(&vk.device, &vk.swapchainImageViews, vk.swapchainImageCount);
	deleteSwapchainImages(&vk.swapchainImages);
	deleteSwapchain(&vk.device, &vk.swapchain);
        surf_not_support:
	deleteSurface(&vk.surface, &vk.instance);
	deleteWindow(vk.window);
	deleteDevice(&vk.device);
	deletePhysicalDevices(&vk.physicalDevices);
	deleteInstance(&vk.instance);

	glfwTerminate();
	return 0;
}

#endif //VULKAN_DEMO_MAIN_C

#include "main.c"

uint32_t getQueueFamilyNumber(VkPhysicalDevice *pPhysicalDevice){
	uint32_t queueFamilyNumber = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(*pPhysicalDevice, &queueFamilyNumber, NULL);
	return queueFamilyNumber;
}

VkQueueFamilyProperties *getQueueFamilyProperties(VkPhysicalDevice *pPhysicalDevice, uint32_t queueFamilyNumber){
	VkQueueFamilyProperties *queueFamilyProperties = (VkQueueFamilyProperties *)malloc(queueFamilyNumber * sizeof(VkQueueFamilyProperties));
	vkGetPhysicalDeviceQueueFamilyProperties(*pPhysicalDevice, &queueFamilyNumber, queueFamilyProperties);
	return queueFamilyProperties;
}

void deleteQueueFamilyProperties(VkQueueFamilyProperties **ppQueueFamilyProperties){
	free(*ppQueueFamilyProperties);
}

// The first queue family that can both draw and present to the surface.
// Drawing and presenting on one queue is the normal desktop configuration.
uint32_t getBestGraphicsQueueFamilyIndex(VkPhysicalDevice *pPhysicalDevice, VkSurfaceKHR *pSurface, VkQueueFamilyProperties *pQueueFamilyProperties, uint32_t queueFamilyNumber){
	for (uint32_t i = 0; i < queueFamilyNumber; i++){
		if ((pQueueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0)
			continue;

		VkBool32 canPresent = VK_FALSE;
		vkGetPhysicalDeviceSurfaceSupportKHR(*pPhysicalDevice, i, *pSurface, &canPresent);
		if (canPresent)
			return i;
	}

	fprintf(stderr, "no queue family supports both graphics and present\n");
	exit(-1);
}

VkQueue getDrawingQueue(VkDevice *pDevice, uint32_t graphicsQueueFamilyIndex){
	VkQueue drawingQueue = VK_NULL_HANDLE;
	vkGetDeviceQueue(*pDevice, graphicsQueueFamilyIndex, 0, &drawingQueue);
	return drawingQueue;
}

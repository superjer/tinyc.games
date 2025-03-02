#include "main.c"

typedef enum drawAndPresentQueues {
        SINGLE_QUEUE,
        SEPARATE_QUEUES,
} drawAndPresentQueues;

uint32_t getQueueFamilyNumber(VkPhysicalDevice *pPhysicalDevice){
	uint32_t queueFamilyNumber = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(*pPhysicalDevice, &queueFamilyNumber, VK_NULL_HANDLE);
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

uint32_t getBestGraphicsQueueFamilyIndex(VkQueueFamilyProperties *pQueueFamilyProperties, uint32_t queueFamilyNumber){
	uint32_t bestCount = 0;
        uint32_t bestIndex = 0;

	for (uint32_t i = 0; i < queueFamilyNumber; i++){
		if ((pQueueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0){
                        if (pQueueFamilyProperties[i].queueCount > bestCount){
                                bestCount = pQueueFamilyProperties[i].queueCount;
                                bestIndex = i;
                        }
		}
	}

	return bestIndex;
}

drawAndPresentQueues getGraphicsQueueMode(VkQueueFamilyProperties *pQueueFamilyProperties, uint32_t graphicsQueueFamilyIndex){
	if (pQueueFamilyProperties[graphicsQueueFamilyIndex].queueCount == 1){
		return SINGLE_QUEUE;
	}else{
		return SEPARATE_QUEUES;
	}
}

VkQueue getDrawingQueue(VkDevice *pDevice, uint32_t graphicsQueueFamilyIndex){
	VkQueue drawingQueue = VK_NULL_HANDLE;
	vkGetDeviceQueue(*pDevice, graphicsQueueFamilyIndex, 0, &drawingQueue);
	return drawingQueue;
}

VkQueue getPresentingQueue(VkDevice *pDevice, uint32_t graphicsQueueFamilyIndex, uint32_t graphicsQueueMode){
	VkQueue presentingQueue = VK_NULL_HANDLE;
	if (graphicsQueueMode == SINGLE_QUEUE){
		vkGetDeviceQueue(*pDevice, graphicsQueueFamilyIndex, 0, &presentingQueue);
	}else if (graphicsQueueMode == SEPARATE_QUEUES){
		vkGetDeviceQueue(*pDevice, graphicsQueueFamilyIndex, 1, &presentingQueue);
	}
	return presentingQueue;
}

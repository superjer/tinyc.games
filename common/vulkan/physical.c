#include "main.c"

uint32_t getPhysicalDeviceNumber(VkInstance *pInstance){
	uint32_t physicalDeviceNumber = 0;
	vkEnumeratePhysicalDevices(*pInstance, &physicalDeviceNumber, VK_NULL_HANDLE);
	return physicalDeviceNumber;
}

VkPhysicalDevice *getPhysicalDevices(VkInstance *pInstance, uint32_t physicalDeviceNumber){
	VkPhysicalDevice *physicalDevices = (VkPhysicalDevice *)malloc(physicalDeviceNumber * sizeof(VkPhysicalDevice));
	vkEnumeratePhysicalDevices(*pInstance, &physicalDeviceNumber, physicalDevices);

	return physicalDevices;
}

void deletePhysicalDevices(VkPhysicalDevice **ppPhysicalDevices){
	free(*ppPhysicalDevices);
}

uint32_t getPhysicalDeviceTotalMemory(VkPhysicalDeviceMemoryProperties *pPhysicalDeviceMemoryProperties){
	uint32_t physicalDeviceTotalMemory = 0;
	for(int i = 0; i < pPhysicalDeviceMemoryProperties->memoryHeapCount; i++){
		if((pPhysicalDeviceMemoryProperties->memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) != 0){
			physicalDeviceTotalMemory += pPhysicalDeviceMemoryProperties->memoryHeaps[i].size;
		}
	}
	return physicalDeviceTotalMemory;
}

uint32_t getBestPhysicalDeviceIndex(VkPhysicalDevice *pPhysicalDevices, uint32_t physicalDeviceNumber){
	VkPhysicalDeviceProperties *physicalDeviceProperties = (VkPhysicalDeviceProperties *)malloc(physicalDeviceNumber * sizeof(VkPhysicalDeviceProperties));
	VkPhysicalDeviceMemoryProperties *physicalDeviceMemoryProperties = (VkPhysicalDeviceMemoryProperties *)malloc(physicalDeviceNumber * sizeof(VkPhysicalDeviceMemoryProperties));

	uint32_t discreteGPUNumber = 0, integratedGPUNumber = 0, *discreteGPUIndices = (uint32_t *)malloc(physicalDeviceNumber * sizeof(uint32_t)), *integratedGPUIndices = (uint32_t *)malloc(physicalDeviceNumber * sizeof(uint32_t));

	for(uint32_t i = 0; i < physicalDeviceNumber; i++){
		vkGetPhysicalDeviceProperties(pPhysicalDevices[i], &physicalDeviceProperties[i]);
		vkGetPhysicalDeviceMemoryProperties(pPhysicalDevices[i], &physicalDeviceMemoryProperties[i]);

		if(physicalDeviceProperties[i].deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU){
			discreteGPUIndices[discreteGPUNumber] = i;
			discreteGPUNumber++;
		}
		if(physicalDeviceProperties[i].deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU){
			integratedGPUIndices[integratedGPUNumber] = i;
			integratedGPUNumber++;
		}
	}

	uint32_t bestPhysicalDeviceIndex = 0;
	VkDeviceSize bestPhysicalDeviceMemory = 0;

	if(discreteGPUNumber != 0){
		for(uint32_t i = 0; i < discreteGPUNumber; i++){
			if(bestPhysicalDeviceMemory < getPhysicalDeviceTotalMemory(&physicalDeviceMemoryProperties[discreteGPUIndices[i]])){
				bestPhysicalDeviceMemory = getPhysicalDeviceTotalMemory(&physicalDeviceMemoryProperties[discreteGPUIndices[i]]);
				bestPhysicalDeviceIndex = discreteGPUIndices[i];
			}
		}
	}else if(integratedGPUNumber != 0){
		for(uint32_t i = 0; i < integratedGPUNumber; i++){
			if(bestPhysicalDeviceMemory < getPhysicalDeviceTotalMemory(&physicalDeviceMemoryProperties[integratedGPUIndices[i]])){
				bestPhysicalDeviceMemory = getPhysicalDeviceTotalMemory(&physicalDeviceMemoryProperties[integratedGPUIndices[i]]);
				bestPhysicalDeviceIndex = integratedGPUIndices[i];
			}
		}
	}

	free(discreteGPUIndices);
	free(integratedGPUIndices);
	free(physicalDeviceMemoryProperties);
	free(physicalDeviceProperties);

	return bestPhysicalDeviceIndex;
}

#include "main.c"

VkSemaphore *createSemaphores(VkDevice *pDevice, uint32_t maxFrames){
	VkSemaphoreCreateInfo semaphoreCreateInfo = {
		VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
		VK_NULL_HANDLE,
		0
	};

	VkSemaphore *semaphore = (VkSemaphore *)malloc(maxFrames * sizeof(VkSemaphore));
	for(uint32_t i = 0; i < maxFrames; i++){
		vkCreateSemaphore(*pDevice, &semaphoreCreateInfo, VK_NULL_HANDLE, &semaphore[i]);
	}
	return semaphore;
}

void deleteSemaphores(VkDevice *pDevice, VkSemaphore **ppSemaphores, uint32_t maxFrames){
	for(uint32_t i = 0; i < maxFrames; i++){
		vkDestroySemaphore(*pDevice, (*ppSemaphores)[i], VK_NULL_HANDLE);
	}
	free(*ppSemaphores);
}

VkFence *createFences(VkDevice *pDevice, uint32_t maxFrames){
	VkFenceCreateInfo fenceCreateInfo = {
		VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
		VK_NULL_HANDLE,
		VK_FENCE_CREATE_SIGNALED_BIT
	};

	VkFence *fences = (VkFence *)malloc(maxFrames * sizeof(VkFence));
	for(uint32_t i = 0; i < maxFrames; i++){
		vkCreateFence(*pDevice, &fenceCreateInfo, VK_NULL_HANDLE, &fences[i]);
	}
	return fences;
}

void deleteFences(VkDevice *pDevice, VkFence **ppFences, uint32_t maxFrames){
	for(uint32_t i = 0; i < maxFrames; i++){
		vkDestroyFence(*pDevice, (*ppFences)[i], VK_NULL_HANDLE);
	}
	free(*ppFences);
}

VkFence *createEmptyFences(uint32_t maxFrames){
	VkFence *fences = (VkFence *)malloc(maxFrames * sizeof(VkFence));
	for(uint32_t i = 0; i < maxFrames; i++){
		fences[i] = VK_NULL_HANDLE;
	}
	return fences;
}

void deleteEmptyFences(VkFence **ppFences){
	free(*ppFences);
}

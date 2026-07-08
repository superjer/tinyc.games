#include "main.c"

VkSemaphore *createSemaphores(VkDevice *pDevice, uint32_t count){
	VkSemaphoreCreateInfo semaphoreCreateInfo = {
		VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
		NULL,
		0
	};

	VkSemaphore *semaphore = (VkSemaphore *)malloc(count * sizeof(VkSemaphore));
	for(uint32_t i = 0; i < count; i++){
		VKCHECK(vkCreateSemaphore(*pDevice, &semaphoreCreateInfo, NULL, &semaphore[i]));
	}
	return semaphore;
}

void deleteSemaphores(VkDevice *pDevice, VkSemaphore **ppSemaphores, uint32_t count){
	for(uint32_t i = 0; i < count; i++){
		vkDestroySemaphore(*pDevice, (*ppSemaphores)[i], NULL);
	}
	free(*ppSemaphores);
}

VkFence *createFences(VkDevice *pDevice, uint32_t count){
	VkFenceCreateInfo fenceCreateInfo = {
		VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
		NULL,
		VK_FENCE_CREATE_SIGNALED_BIT
	};

	VkFence *fences = (VkFence *)malloc(count * sizeof(VkFence));
	for(uint32_t i = 0; i < count; i++){
		VKCHECK(vkCreateFence(*pDevice, &fenceCreateInfo, NULL, &fences[i]));
	}
	return fences;
}

void deleteFences(VkDevice *pDevice, VkFence **ppFences, uint32_t count){
	for(uint32_t i = 0; i < count; i++){
		vkDestroyFence(*pDevice, (*ppFences)[i], NULL);
	}
	free(*ppFences);
}

VkFence *createEmptyFences(uint32_t count){
	VkFence *fences = (VkFence *)malloc(count * sizeof(VkFence));
	for(uint32_t i = 0; i < count; i++){
		fences[i] = VK_NULL_HANDLE;
	}
	return fences;
}

void deleteEmptyFences(VkFence **ppFences){
	free(*ppFences);
}

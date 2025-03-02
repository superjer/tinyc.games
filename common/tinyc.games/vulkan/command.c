#include "main.c"

VkCommandPool createCommandPool(VkDevice *pDevice, uint32_t queueFamilyIndex){
	VkCommandPoolCreateInfo commandPoolCreateInfo = {
		VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		VK_NULL_HANDLE,
		0,
		queueFamilyIndex
	};

	VkCommandPool commandPool;
	vkCreateCommandPool(*pDevice, &commandPoolCreateInfo, VK_NULL_HANDLE, &commandPool);
	return commandPool;
}

void deleteCommandPool(VkDevice *pDevice, VkCommandPool *pCommandPool){
	vkDestroyCommandPool(*pDevice, *pCommandPool, VK_NULL_HANDLE);
}

VkCommandBuffer *createCommandBuffers(VkDevice *pDevice, VkCommandPool *pCommandPool, uint32_t commandBufferNumber){
	VkCommandBufferAllocateInfo commandBufferAllocateInfo = {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		VK_NULL_HANDLE,
		*pCommandPool,
		VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		commandBufferNumber
	};

	VkCommandBuffer *commandBuffers = (VkCommandBuffer *)malloc(commandBufferNumber * sizeof(VkCommandBuffer));
	vkAllocateCommandBuffers(*pDevice, &commandBufferAllocateInfo, commandBuffers);
	return commandBuffers;
}

void deleteCommandBuffers(VkDevice *pDevice, VkCommandBuffer *pCommandBuffers, VkCommandPool *pCommandPool, uint32_t commandBufferNumber){
	vkFreeCommandBuffers(*pDevice, *pCommandPool, commandBufferNumber, pCommandBuffers);
	free(pCommandBuffers);
}

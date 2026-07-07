#include "main.c"

VkCommandPool createCommandPool(VkDevice *pDevice, uint32_t queueFamilyIndex){
	VkCommandPoolCreateInfo commandPoolCreateInfo = {
		VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		NULL,
                VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		queueFamilyIndex
	};

	VkCommandPool commandPool;
	VKCHECK(vkCreateCommandPool(*pDevice, &commandPoolCreateInfo, NULL, &commandPool));
	return commandPool;
}

void deleteCommandPool(VkDevice *pDevice, VkCommandPool *pCommandPool){
	vkDestroyCommandPool(*pDevice, *pCommandPool, NULL);
}

VkCommandBuffer *createCommandBuffers(VkDevice *pDevice, VkCommandPool *pCommandPool, uint32_t commandBufferCount){
	VkCommandBufferAllocateInfo commandBufferAllocateInfo = {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		NULL,
		*pCommandPool,
		VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		commandBufferCount
	};

	VkCommandBuffer *commandBuffers = (VkCommandBuffer *)malloc(commandBufferCount * sizeof(VkCommandBuffer));
	VKCHECK(vkAllocateCommandBuffers(*pDevice, &commandBufferAllocateInfo, commandBuffers));
	return commandBuffers;
}

void deleteCommandBuffers(VkDevice *pDevice, VkCommandBuffer *pCommandBuffers, VkCommandPool *pCommandPool, uint32_t commandBufferNumber){
	vkFreeCommandBuffers(*pDevice, *pCommandPool, commandBufferNumber, pCommandBuffers);
	free(pCommandBuffers);
}

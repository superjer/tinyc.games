#include "main.c"

VkRenderPass createRenderPass(VkDevice *pDevice, VkSurfaceFormatKHR *pFormat){
	VkAttachmentDescription attachmentDescription = {
		0,
		pFormat->format,
		VK_SAMPLE_COUNT_1_BIT,
		VK_ATTACHMENT_LOAD_OP_CLEAR,
		VK_ATTACHMENT_STORE_OP_STORE,
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		VK_ATTACHMENT_STORE_OP_DONT_CARE,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
	};

	VkAttachmentReference attachmentReference = {
		0,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
	};

	VkSubpassDescription subpassDescription = {
		0,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		0,
		VK_NULL_HANDLE,
		1,
		&attachmentReference,
		VK_NULL_HANDLE,
		VK_NULL_HANDLE,
		0,
		VK_NULL_HANDLE
	};

	VkSubpassDependency subpassDependency = {
		VK_SUBPASS_EXTERNAL,
		0,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		0,
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		0
	};

	VkRenderPassCreateInfo renderPassCreateInfo = {
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		VK_NULL_HANDLE,
		0,
		1,
		&attachmentDescription,
		1,
		&subpassDescription,
		1,
		&subpassDependency
	};

	VkRenderPass renderPass;
	vkCreateRenderPass(*pDevice, &renderPassCreateInfo, VK_NULL_HANDLE, &renderPass);
	return renderPass;
}

void deleteRenderPass(VkDevice *pDevice, VkRenderPass *pRenderPass){
	vkDestroyRenderPass(*pDevice, *pRenderPass, VK_NULL_HANDLE);
}

VkFramebuffer *createFramebuffers(VkDevice *pDevice, VkRenderPass *pRenderPass, VkExtent2D *pExtent, VkImageView **ppImageViews, uint32_t imageViewNumber){
	VkFramebufferCreateInfo *framebufferCreateInfo = (VkFramebufferCreateInfo *)malloc(imageViewNumber * sizeof(VkFramebufferCreateInfo));
	VkFramebuffer *framebuffers = (VkFramebuffer *)malloc(imageViewNumber * sizeof(VkFramebuffer));

	for(uint32_t i = 0; i < imageViewNumber; i++){
		framebufferCreateInfo[i].sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferCreateInfo[i].pNext = VK_NULL_HANDLE;
		framebufferCreateInfo[i].flags = 0;
		framebufferCreateInfo[i].renderPass = *pRenderPass;
		framebufferCreateInfo[i].attachmentCount = 1;
		framebufferCreateInfo[i].pAttachments = &(*ppImageViews)[i];
		framebufferCreateInfo[i].width = pExtent->width;
		framebufferCreateInfo[i].height = pExtent->height;
		framebufferCreateInfo[i].layers = 1;

		vkCreateFramebuffer(*pDevice, &framebufferCreateInfo[i], VK_NULL_HANDLE, &framebuffers[i]);
	}

	free(framebufferCreateInfo);
	return framebuffers;
}

void deleteFramebuffers(VkDevice *pDevice, VkFramebuffer **ppFramebuffers, uint32_t framebufferNumber){
	for(uint32_t i = 0; i < framebufferNumber; i++){
		vkDestroyFramebuffer(*pDevice, (*ppFramebuffers)[i], VK_NULL_HANDLE);
	}
	free(*ppFramebuffers);
}

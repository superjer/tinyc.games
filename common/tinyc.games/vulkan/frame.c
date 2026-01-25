#include "main.c"

VkRenderPass createRenderPass(VkDevice *pDevice, VkSurfaceFormatKHR *pFormat){
	VkAttachmentDescription attachments[2] = {
		// Color attachment
		{
			0,
			pFormat->format,
			VK_SAMPLE_COUNT_1_BIT,
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			VK_ATTACHMENT_STORE_OP_STORE,
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			VK_ATTACHMENT_STORE_OP_DONT_CARE,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
		},
		// Depth attachment
		{
			0,
			VK_FORMAT_D32_SFLOAT,
			VK_SAMPLE_COUNT_1_BIT,
			VK_ATTACHMENT_LOAD_OP_CLEAR,
			VK_ATTACHMENT_STORE_OP_DONT_CARE,
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			VK_ATTACHMENT_STORE_OP_DONT_CARE,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
		}
	};

	VkAttachmentReference colorReference = {
		0,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
	};

	VkAttachmentReference depthReference = {
		1,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
	};

	VkSubpassDescription subpassDescription = {
		0,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		0,
		VK_NULL_HANDLE,
		1,
		&colorReference,
		VK_NULL_HANDLE,
		&depthReference,
		0,
		VK_NULL_HANDLE
	};

	VkSubpassDependency subpassDependency = {
		VK_SUBPASS_EXTERNAL,
		0,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
		0,
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
		0
	};

	VkRenderPassCreateInfo renderPassCreateInfo = {
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		VK_NULL_HANDLE,
		0,
		2,
		attachments,
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

VkFramebuffer *createFramebuffers(VkDevice *pDevice, VkRenderPass *pRenderPass, VkExtent2D *pExtent, VkImageView **ppImageViews, uint32_t imageViewNumber, VkImageView *pDepthImageView){
	VkFramebuffer *framebuffers = (VkFramebuffer *)malloc(imageViewNumber * sizeof(VkFramebuffer));

	for(uint32_t i = 0; i < imageViewNumber; i++){
		VkImageView attachments[2] = {
			(*ppImageViews)[i],
			*pDepthImageView
		};

		VkFramebufferCreateInfo framebufferCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			.pNext = VK_NULL_HANDLE,
			.flags = 0,
			.renderPass = *pRenderPass,
			.attachmentCount = 2,
			.pAttachments = attachments,
			.width = pExtent->width,
			.height = pExtent->height,
			.layers = 1,
		};

		vkCreateFramebuffer(*pDevice, &framebufferCreateInfo, VK_NULL_HANDLE, &framebuffers[i]);
	}

	return framebuffers;
}

void deleteFramebuffers(VkDevice *pDevice, VkFramebuffer **ppFramebuffers, uint32_t framebufferNumber){
	for(uint32_t i = 0; i < framebufferNumber; i++){
		vkDestroyFramebuffer(*pDevice, (*ppFramebuffers)[i], VK_NULL_HANDLE);
	}
	free(*ppFramebuffers);
}

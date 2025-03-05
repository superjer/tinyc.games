#include "main.c"

VkPipelineLayout createPipelineLayout(VkDevice *pDevice){
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		VK_NULL_HANDLE,
		0,
		0,
		VK_NULL_HANDLE,
		0,
		VK_NULL_HANDLE
	};

	VkPipelineLayout pipelineLayout;
	vkCreatePipelineLayout(*pDevice, &pipelineLayoutCreateInfo, VK_NULL_HANDLE, &pipelineLayout);
	return pipelineLayout;
}

void deletePipelineLayout(VkDevice *pDevice, VkPipelineLayout *pPipelineLayout){
	vkDestroyPipelineLayout(*pDevice, *pPipelineLayout, VK_NULL_HANDLE);
}

VkPipelineShaderStageCreateInfo configureShaderStageCreateInfo(VkShaderStageFlagBits bits, VkShaderModule *shaderModule, const char *entryName){
	VkPipelineShaderStageCreateInfo fragmentShaderStageCreateInfo = {
		VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		VK_NULL_HANDLE,
		0,
		bits,
		*shaderModule,
		entryName,
		VK_NULL_HANDLE
	};

	return fragmentShaderStageCreateInfo;
}

VkPipelineVertexInputStateCreateInfo configureVertexInputStateCreateInfo(){
	VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		VK_NULL_HANDLE,
		0,
		0,
		VK_NULL_HANDLE,
		0,
		VK_NULL_HANDLE
	};

	return vertexInputStateCreateInfo;
}

VkPipelineInputAssemblyStateCreateInfo configureInputAssemblyStateCreateInfo(){
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo = {
		VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		VK_NULL_HANDLE,
		0,
		VK_PRIMITIVE_TOPOLOGY_POINT_LIST, // was TRIANGLE
		VK_FALSE
	};

	return inputAssemblyStateCreateInfo;
}

VkViewport configureViewport(VkExtent2D *pExtent){
	VkViewport viewport = {
		1.0f,
		1.0f,
		pExtent->width,
		pExtent->height,
		0.0f,
		1.0f
	};

	return viewport;
}

VkRect2D configureScissor(VkExtent2D *pExtent, uint32_t left, uint32_t right, uint32_t up, uint32_t down){
	if(left > pExtent->width){
		left = pExtent->width;
	}
	if(right > pExtent->width){
		right = pExtent->width;
	}
	if(up > pExtent->height){
		up = pExtent->height;
	}
	if(down > pExtent->height){
		down = pExtent->height;
	}
	VkOffset2D offset = {
		left,
		up
	};
	VkExtent2D extent = {
		pExtent->width - left - right,
		pExtent->height - up - down
	};
	VkRect2D scissor = {
		offset,
		extent
	};
	return scissor;
}

VkPipelineViewportStateCreateInfo configureViewportStateCreateInfo(VkViewport *pViewport, VkRect2D *pScissor){
	VkPipelineViewportStateCreateInfo viewportStateCreateInfo = {
		VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		VK_NULL_HANDLE,
		0,
		1,
		pViewport,
		1,
		pScissor
	};

	return viewportStateCreateInfo;
}

VkPipelineRasterizationStateCreateInfo configureRasterizationStateCreateInfo(){
	VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo = {
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		VK_NULL_HANDLE,
		0,
		VK_FALSE,
		VK_FALSE,
		VK_POLYGON_MODE_FILL,
		VK_CULL_MODE_BACK_BIT,
		VK_FRONT_FACE_CLOCKWISE,
		VK_FALSE,
		0.0f,
		0.0f,
		0.0f,
		1.0f
	};

	return rasterizationStateCreateInfo;
}

VkPipelineMultisampleStateCreateInfo configureMultisampleStateCreateInfo(){
	VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo = {
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		VK_NULL_HANDLE,
		0,
		VK_SAMPLE_COUNT_1_BIT,
		VK_FALSE,
		1.0f,
		VK_NULL_HANDLE,
		VK_FALSE,
		VK_FALSE
	};

	return multisampleStateCreateInfo;
}

VkPipelineColorBlendAttachmentState configureColorBlendAttachmentState(){
	VkPipelineColorBlendAttachmentState colorBlendAttachmentState = {
		VK_FALSE,
		VK_BLEND_FACTOR_ONE,
		VK_BLEND_FACTOR_ZERO,
		VK_BLEND_OP_ADD,
		VK_BLEND_FACTOR_ONE,
		VK_BLEND_FACTOR_ZERO,
		VK_BLEND_OP_ADD,
		VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
	};

	return colorBlendAttachmentState;
}

VkPipelineColorBlendStateCreateInfo configureColorBlendStateCreateInfo(VkPipelineColorBlendAttachmentState *pColorBlendAttachmentState){
	VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo = {
		VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		VK_NULL_HANDLE,
		0,
		VK_FALSE,
		VK_LOGIC_OP_COPY,
		1,
		pColorBlendAttachmentState,
		{0.0f, 0.0f, 0.0f, 0.0f}
	};

	return colorBlendStateCreateInfo;
}

VkPipeline createGraphicsPipeline(VkDevice *pDevice, VkPipelineLayout *pPipelineLayout, VkShaderModule *pVertexShaderModule, VkShaderModule *pGeometryShaderModule, VkShaderModule *pFragmentShaderModule, VkRenderPass *pRenderPass, VkExtent2D *pExtent){
	char entryName[] = "main";

	VkPipelineShaderStageCreateInfo shaderStageCreateInfo[] = {
		configureShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT, pVertexShaderModule, entryName),
		configureShaderStageCreateInfo(VK_SHADER_STAGE_GEOMETRY_BIT, pGeometryShaderModule, entryName),
		configureShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT, pFragmentShaderModule, entryName)
	};
	VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = configureVertexInputStateCreateInfo();
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo = configureInputAssemblyStateCreateInfo();
	VkViewport viewport = configureViewport(pExtent);
	VkRect2D scissor = configureScissor(pExtent, 0, 0, 0, 0);
	VkPipelineViewportStateCreateInfo viewportStateCreateInfo = configureViewportStateCreateInfo(&viewport, &scissor);
	VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo = configureRasterizationStateCreateInfo();
	VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo = configureMultisampleStateCreateInfo();
	VkPipelineColorBlendAttachmentState colorBlendAttachmentState = configureColorBlendAttachmentState();
	VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo = configureColorBlendStateCreateInfo(&colorBlendAttachmentState);

	VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo = {
		VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		VK_NULL_HANDLE,
		0,
		3,
		shaderStageCreateInfo,
		&vertexInputStateCreateInfo,
		&inputAssemblyStateCreateInfo,
		VK_NULL_HANDLE,
		&viewportStateCreateInfo,
		&rasterizationStateCreateInfo,
		&multisampleStateCreateInfo,
		VK_NULL_HANDLE,
		&colorBlendStateCreateInfo,
		VK_NULL_HANDLE,
		*pPipelineLayout,
		*pRenderPass,
		0,
		VK_NULL_HANDLE,
		-1
	};

	VkPipeline graphicsPipeline;
	vkCreateGraphicsPipelines(*pDevice, VK_NULL_HANDLE, 1, &graphicsPipelineCreateInfo, VK_NULL_HANDLE, &graphicsPipeline);
	return graphicsPipeline;
}

void deleteGraphicsPipeline(VkDevice *pDevice, VkPipeline *pGraphicsPipeline){
	vkDestroyPipeline(*pDevice, *pGraphicsPipeline, VK_NULL_HANDLE);
}

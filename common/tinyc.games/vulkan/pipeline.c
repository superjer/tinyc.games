#include "main.c"

VkPipelineLayout createPipelineLayout(VkDevice *pDevice){
        VkPushConstantRange pushConstantRange = {0};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pushConstantRange.offset = 0;
        pushConstantRange.size = 128;

	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		NULL,
		0,
		0,
		NULL,
		1,
		&pushConstantRange
	};

	VkPipelineLayout pipelineLayout;
	VKCHECK(vkCreatePipelineLayout(*pDevice, &pipelineLayoutCreateInfo, NULL, &pipelineLayout));
	return pipelineLayout;
}

VkPipelineLayout createPipelineLayoutWithDescriptors(VkDevice *pDevice, VkDescriptorSetLayout *pDescriptorSetLayout){
        VkPushConstantRange pushConstantRange = {0};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pushConstantRange.offset = 0;
        pushConstantRange.size = 128;

	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		NULL,
		0,
		1,
		pDescriptorSetLayout,
		1,
		&pushConstantRange
	};

	VkPipelineLayout pipelineLayout;
	VKCHECK(vkCreatePipelineLayout(*pDevice, &pipelineLayoutCreateInfo, NULL, &pipelineLayout));
	return pipelineLayout;
}

void deletePipelineLayout(VkDevice *pDevice, VkPipelineLayout *pPipelineLayout){
	vkDestroyPipelineLayout(*pDevice, *pPipelineLayout, NULL);
}

VkPipelineShaderStageCreateInfo configureShaderStageCreateInfo(VkShaderStageFlagBits bits, VkShaderModule *shaderModule, const char *entryName){
	VkPipelineShaderStageCreateInfo fragmentShaderStageCreateInfo = {
		VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		NULL,
		0,
		bits,
		*shaderModule,
		entryName,
		NULL
	};

	return fragmentShaderStageCreateInfo;
}

VkPipelineVertexInputStateCreateInfo configureVertexInputStateCreateInfo(
        int bindingDescCount, VkVertexInputBindingDescription *bindingDescs,
        int attributeDescCount, VkVertexInputAttributeDescription *attributeDescs){

	VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		NULL,
		0,
		bindingDescCount,
		bindingDescs,
		attributeDescCount,
		attributeDescs
	};

	return vertexInputStateCreateInfo;
}

VkPipelineInputAssemblyStateCreateInfo configureInputAssemblyStateCreateInfo(VkPrimitiveTopology primTop){
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo = {
		VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		NULL,
		0,
		primTop, // VK_PRIMITIVE_TOPOLOGY_POINT_LIST, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
		VK_FALSE
	};

	return inputAssemblyStateCreateInfo;
}

// Placeholder values only: viewport and scissor are dynamic state in every
// pipeline this layer creates, so each draw sets the real ones with
// vkCmdSetViewport/vkCmdSetScissor.
VkViewport configureViewport(VkExtent2D *pExtent){
	VkViewport viewport = {
		0.0f,
		0.0f,
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
		NULL,
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
		NULL,
		0,
		VK_FALSE,
		VK_FALSE,
		VK_POLYGON_MODE_FILL,
		VK_CULL_MODE_BACK_BIT,
		VK_FRONT_FACE_COUNTER_CLOCKWISE,
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
		NULL,
		0,
		VK_SAMPLE_COUNT_1_BIT,
		VK_FALSE,
		1.0f,
		NULL,
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
		NULL,
		0,
		VK_FALSE,
		VK_LOGIC_OP_COPY,
		1,
		pColorBlendAttachmentState,
		{0.0f, 0.0f, 0.0f, 0.0f}
	};

	return colorBlendStateCreateInfo;
}

#define PIPE_NO_DEPTH_WRITE   1
#define PIPE_BLEND            2
#define PIPE_NO_DEPTH_TEST    4
#define PIPE_NO_CULL          8
#define PIPE_FRONT_CULL       16
#define PIPE_DEPTH_BIAS       32
#define PIPE_DEPTH_LESS_EQUAL 64
#define PIPE_POINTS           128 // vertex shader must write gl_PointSize
#define PIPE_TRIANGLE_STRIP   256

VkPipeline createGraphicsPipeline(VkDevice *pDevice, VkPipelineLayout *pPipelineLayout, VkShaderModule *pVertexShaderModule, VkShaderModule *pGeometryShaderModule, VkShaderModule *pFragmentShaderModule, VkRenderPass *pRenderPass, VkExtent2D *pExtent,
        int bindingDescCount, VkVertexInputBindingDescription *bindingDescs,
        int attributeDescCount, VkVertexInputAttributeDescription *attributeDescs,
        int flags
){
	char entryName[] = "main";

	VkPipelineShaderStageCreateInfo shaderStageCreateInfo[3] = {0};
        int i = 0;
        shaderStageCreateInfo[i++] = configureShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT, pVertexShaderModule, entryName);
        if (pGeometryShaderModule)
                shaderStageCreateInfo[i++] = configureShaderStageCreateInfo(VK_SHADER_STAGE_GEOMETRY_BIT, pGeometryShaderModule, entryName);
        shaderStageCreateInfo[i++] = configureShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT, pFragmentShaderModule, entryName);

	VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = configureVertexInputStateCreateInfo(
                bindingDescCount, bindingDescs, attributeDescCount, attributeDescs
        );
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo = configureInputAssemblyStateCreateInfo(
		(flags & PIPE_TRIANGLE_STRIP) ? VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP :
		(pGeometryShaderModule || (flags & PIPE_POINTS)) ? VK_PRIMITIVE_TOPOLOGY_POINT_LIST : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
        );
	VkViewport viewport = configureViewport(pExtent);
	VkRect2D scissor = configureScissor(pExtent, 0, 0, 0, 0);
	VkPipelineViewportStateCreateInfo viewportStateCreateInfo = configureViewportStateCreateInfo(&viewport, &scissor);
	VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo = configureRasterizationStateCreateInfo();
	if (flags & PIPE_NO_CULL) {
		rasterizationStateCreateInfo.cullMode = VK_CULL_MODE_NONE;
	}
	if (flags & PIPE_FRONT_CULL) {
		rasterizationStateCreateInfo.cullMode = VK_CULL_MODE_FRONT_BIT;
	}
	if (flags & PIPE_DEPTH_BIAS) {
		rasterizationStateCreateInfo.depthBiasEnable = VK_TRUE;
		rasterizationStateCreateInfo.depthBiasConstantFactor = 1.5f;
		rasterizationStateCreateInfo.depthBiasSlopeFactor = 1.5f;
	}
	VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo = configureMultisampleStateCreateInfo();
	VkPipelineColorBlendAttachmentState colorBlendAttachmentState;
	if (flags & PIPE_BLEND) {
		colorBlendAttachmentState = (VkPipelineColorBlendAttachmentState){
			VK_TRUE,
			VK_BLEND_FACTOR_SRC_ALPHA,
			VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
			VK_BLEND_OP_ADD,
			VK_BLEND_FACTOR_ONE,
			VK_BLEND_FACTOR_ZERO,
			VK_BLEND_OP_ADD,
			VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
		};
	} else {
		colorBlendAttachmentState = configureColorBlendAttachmentState();
	}
	VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo = configureColorBlendStateCreateInfo(&colorBlendAttachmentState);

	VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		.depthTestEnable = (flags & PIPE_NO_DEPTH_TEST) ? VK_FALSE : VK_TRUE,
		.depthWriteEnable = (flags & (PIPE_NO_DEPTH_WRITE | PIPE_NO_DEPTH_TEST)) ? VK_FALSE : VK_TRUE,
		.depthCompareOp = (flags & PIPE_DEPTH_LESS_EQUAL) ? VK_COMPARE_OP_LESS_OR_EQUAL : VK_COMPARE_OP_LESS,
		.depthBoundsTestEnable = VK_FALSE,
		.stencilTestEnable = VK_FALSE,
	};

	VkDynamicState dynamicStates[] = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR,
		VK_DYNAMIC_STATE_DEPTH_BIAS
	};
	VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.dynamicStateCount = (flags & PIPE_DEPTH_BIAS) ? 3 : 2,
		.pDynamicStates = dynamicStates,
	};

	VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo = {
		VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		NULL,
		0,
		i,
		shaderStageCreateInfo,
		&vertexInputStateCreateInfo,
		&inputAssemblyStateCreateInfo,
		NULL,
		&viewportStateCreateInfo,
		&rasterizationStateCreateInfo,
		&multisampleStateCreateInfo,
		&depthStencilStateCreateInfo,
		&colorBlendStateCreateInfo,
		&dynamicStateCreateInfo,
		*pPipelineLayout,
		*pRenderPass,
		0,
		VK_NULL_HANDLE,
		-1
	};

	// Failure is not fatal here: shader hot-reload wants to keep running
	// with the old pipeline when a rebuilt one doesn't come together.
	VkPipeline graphicsPipeline;
	VkResult result = vkCreateGraphicsPipelines(*pDevice, VK_NULL_HANDLE, 1, &graphicsPipelineCreateInfo, NULL, &graphicsPipeline);
	if (result != VK_SUCCESS){
		fprintf(stderr, "vkCreateGraphicsPipelines failed (%d)\n", result);
		return VK_NULL_HANDLE;
	}
	return graphicsPipeline;
}

void deleteGraphicsPipeline(VkDevice *pDevice, VkPipeline *pGraphicsPipeline){
	vkDestroyPipeline(*pDevice, *pGraphicsPipeline, NULL);
}

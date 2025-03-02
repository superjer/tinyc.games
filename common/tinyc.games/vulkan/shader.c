#include "main.c"

char *getShaderCode(const char *fileName, uint32_t *pShaderSize){
	if (pShaderSize == VK_NULL_HANDLE){
		return VK_NULL_HANDLE;
	}
	FILE *fp = VK_NULL_HANDLE;
	fp = fopen(fileName, "rb+");
	if (fp == VK_NULL_HANDLE){
		return VK_NULL_HANDLE;
	}
	fseek(fp, 0l, SEEK_END);
	*pShaderSize = (uint32_t)ftell(fp);
	rewind(fp);

	char *shaderCode = (char *)malloc((*pShaderSize) * sizeof(char));
	fread(shaderCode, 1, *pShaderSize, fp);

	fclose(fp);
	return shaderCode;
}

void deleteShaderCode(char **ppShaderCode){
	free(*ppShaderCode);
}

VkShaderModule createShaderModule(VkDevice *pDevice, char *pShaderCode, uint32_t shaderSize){
	VkShaderModuleCreateInfo shaderModuleCreateInfo = {
		VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		VK_NULL_HANDLE,
		0,
		shaderSize,
		(const uint32_t *)pShaderCode
	};

	VkShaderModule shaderModule;
	vkCreateShaderModule(*pDevice, &shaderModuleCreateInfo, VK_NULL_HANDLE, &shaderModule);
	return shaderModule;
}

void deleteShaderModule(VkDevice *pDevice, VkShaderModule *pShaderModule){
	vkDestroyShaderModule(*pDevice, *pShaderModule, VK_NULL_HANDLE);
}

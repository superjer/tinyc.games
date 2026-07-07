#include "main.c"

char *getShaderCode(const char *fileName, uint32_t *pShaderSize){
	if (pShaderSize == NULL){
		return NULL;
	}
	FILE *fp = fopen(fileName, "rb");
	if (fp == NULL){
		return NULL;
	}
	fseek(fp, 0l, SEEK_END);
	long fileSize = ftell(fp);
	rewind(fp);

	// SPIR-V is a stream of 32-bit words; anything else is not a shader
	if (fileSize <= 0 || fileSize % 4 != 0){
		fprintf(stderr, "%s is not valid SPIR-V (size %ld)\n", fileName, fileSize);
		fclose(fp);
		return NULL;
	}
	*pShaderSize = (uint32_t)fileSize;

	char *shaderCode = (char *)malloc(*pShaderSize);
	if (fread(shaderCode, 1, *pShaderSize, fp) != *pShaderSize){
		fprintf(stderr, "could not read all of %s\n", fileName);
		free(shaderCode);
		fclose(fp);
		return NULL;
	}

	fclose(fp);
	return shaderCode;
}

void deleteShaderCode(char **ppShaderCode){
	free(*ppShaderCode);
}

VkShaderModule createShaderModule(VkDevice *pDevice, char *pShaderCode, uint32_t shaderSize){
	VkShaderModuleCreateInfo shaderModuleCreateInfo = {
		VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		NULL,
		0,
		shaderSize,
		(const uint32_t *)pShaderCode
	};

	VkShaderModule shaderModule;
	VKCHECK(vkCreateShaderModule(*pDevice, &shaderModuleCreateInfo, NULL, &shaderModule));
	return shaderModule;
}

void deleteShaderModule(VkDevice *pDevice, VkShaderModule *pShaderModule){
	vkDestroyShaderModule(*pDevice, *pShaderModule, NULL);
}

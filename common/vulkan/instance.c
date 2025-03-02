#include "main.c"

VkInstance createInstance(){
	VkApplicationInfo applicationInfo = {
		VK_STRUCTURE_TYPE_APPLICATION_INFO,
		VK_NULL_HANDLE,
		VK_NULL_HANDLE,
		0,
		VK_NULL_HANDLE,
		0,
		VK_API_VERSION_1_0
	};

	const char layerList[][VK_MAX_EXTENSION_NAME_SIZE] = {
		"VK_LAYER_KHRONOS_validation"
	};
	const char *layers[] = {
		layerList[0]
	};

	uint32_t extensionNumber = 0;
	const char *const *extensions = glfwGetRequiredInstanceExtensions(&extensionNumber);
	//const char *const *extensions = SDL_Vulkan_GetInstanceExtensions(&extensionNumber);

	VkInstanceCreateInfo instanceCreateInfo = {
		VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		VK_NULL_HANDLE,
		0,
		&applicationInfo,
		1,
		layers,
		extensionNumber,
		extensions
	};

	VkInstance instance;
	vkCreateInstance(&instanceCreateInfo, VK_NULL_HANDLE, &instance);
	return instance;
}

void deleteInstance(VkInstance *pInstance){
	vkDestroyInstance(*pInstance, VK_NULL_HANDLE);
}

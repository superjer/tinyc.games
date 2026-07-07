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

	// enable the validation layer only if it is installed (it ships with
	// the Vulkan SDK; machines without the SDK usually don't have it, and
	// requesting a missing layer makes vkCreateInstance fail outright)
	const char *layers[] = {
		"VK_LAYER_KHRONOS_validation"
	};
	uint32_t layerNumber = 0;
	{
		uint32_t count = 0;
		vkEnumerateInstanceLayerProperties(&count, VK_NULL_HANDLE);
		VkLayerProperties *props = calloc(count, sizeof *props);
		vkEnumerateInstanceLayerProperties(&count, props);
		for (uint32_t i = 0; i < count; i++)
			if (strcmp(props[i].layerName, layers[0]) == 0)
				layerNumber = 1;
		free(props);
		if (!layerNumber)
			fprintf(stderr, "%s not available, continuing without it\n", layers[0]);
	}

	uint32_t extensionNumber = 0;
	const char *const *sdlExtensions = SDL_Vulkan_GetInstanceExtensions(&extensionNumber);

	// SDL's required extensions, with room for portability enumeration
	const char *extensions[16];
	VkInstanceCreateFlags flags = 0;
	if (extensionNumber > 15) extensionNumber = 15;
	for (uint32_t i = 0; i < extensionNumber; i++)
		extensions[i] = sdlExtensions[i];

	// MoltenVK (Mac) is a "portability" implementation: Vulkan loaders
	// since 1.3.216 hide it unless this extension and flag are enabled
	#ifdef VK_KHR_portability_enumeration
	{
		uint32_t count = 0;
		vkEnumerateInstanceExtensionProperties(VK_NULL_HANDLE, &count, VK_NULL_HANDLE);
		VkExtensionProperties *props = calloc(count, sizeof *props);
		vkEnumerateInstanceExtensionProperties(VK_NULL_HANDLE, &count, props);
		for (uint32_t i = 0; i < count; i++)
			if (strcmp(props[i].extensionName, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME) == 0)
			{
				extensions[extensionNumber++] = VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME;
				flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
				break;
			}
		free(props);
	}
	#endif

	VkInstanceCreateInfo instanceCreateInfo = {
		VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		VK_NULL_HANDLE,
		flags,
		&applicationInfo,
		layerNumber,
		layers,
		extensionNumber,
		extensions
	};

	VkInstance instance;
	VkResult result = vkCreateInstance(&instanceCreateInfo, VK_NULL_HANDLE, &instance);
	if (result != VK_SUCCESS)
	{
		fprintf(stderr, "vkCreateInstance failed (%d) - is a Vulkan driver installed?\n", result);
		exit(-1);
	}
	return instance;
}

void deleteInstance(VkInstance *pInstance){
	vkDestroyInstance(*pInstance, VK_NULL_HANDLE);
}

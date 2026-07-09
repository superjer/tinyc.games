#include "main.c"

SDL_Window *createVulkanWindow(int width, int height, const char *title, const char *icon_file){
        SDL_Window *window = SDL_CreateWindow(title, width, height, SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN);
        if (!window)
        {
                fprintf(stderr, "could not create SDL window: %s\n", SDL_GetError());
                exit(-1);
        }

        // Load and set window icon
        int icon_w, icon_h, icon_channels;
        unsigned char *icon_pixels = stbi_load(icon_file, &icon_w, &icon_h, &icon_channels, 4);
        if (icon_pixels) {
                SDL_Surface *icon = SDL_CreateSurfaceFrom(icon_w, icon_h, SDL_PIXELFORMAT_RGBA32, icon_pixels, icon_w * 4);
                if (icon) {
                        SDL_SetWindowIcon(window, icon);
                        SDL_DestroySurface(icon);
                }
                stbi_image_free(icon_pixels);
        } else {
                fprintf(stderr, "Could not load window icon: %s\n", icon_file);
        }

	return window;
}

void deleteWindow(SDL_Window *pWindow){
	SDL_DestroyWindow(pWindow);
}

VkSurfaceKHR createSurface(SDL_Window *pWindow, VkInstance *pInstance){
	VkSurfaceKHR surface = VK_NULL_HANDLE;
        if (!SDL_Vulkan_CreateSurface(pWindow, *pInstance, NULL, &surface))
        {
                fprintf(stderr, "could not create Vulkan surface: %s\n", SDL_GetError());
                exit(-1);
        }
	return surface;
}

void deleteSurface(VkSurfaceKHR *pSurface, VkInstance *pInstance){
	vkDestroySurfaceKHR(*pInstance, *pSurface, NULL);
}

VkSurfaceCapabilitiesKHR getSurfaceCapabilities(VkSurfaceKHR *pSurface, VkPhysicalDevice *pPhysicalDevice){
	VkSurfaceCapabilitiesKHR surfaceCapabilities;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(*pPhysicalDevice, *pSurface, &surfaceCapabilities);
	return surfaceCapabilities;
}

VkSurfaceFormatKHR getBestSurfaceFormat(VkSurfaceKHR *pSurface, VkPhysicalDevice *pPhysicalDevice){
	uint32_t surfaceFormatNumber = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(*pPhysicalDevice, *pSurface, &surfaceFormatNumber, NULL);
	VkSurfaceFormatKHR *surfaceFormats = (VkSurfaceFormatKHR *)malloc(surfaceFormatNumber * sizeof(VkSurfaceFormatKHR));
	vkGetPhysicalDeviceSurfaceFormatsKHR(*pPhysicalDevice, *pSurface, &surfaceFormatNumber, surfaceFormats);

	// Prefer a plain 8-bit UNORM format so shader output reaches the display
	// without an implicit linear->sRGB conversion (an SRGB swapchain would
	// over-brighten colors that are already display-ready). Fall back to
	// whatever the driver lists first if that format isn't offered.
	VkSurfaceFormatKHR bestSurfaceFormat = surfaceFormats[0];
	for(uint32_t i = 0; i < surfaceFormatNumber; i++){
		if((surfaceFormats[i].format == VK_FORMAT_B8G8R8A8_UNORM ||
		    surfaceFormats[i].format == VK_FORMAT_R8G8B8A8_UNORM) &&
		   surfaceFormats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR){
			bestSurfaceFormat = surfaceFormats[i];
			break;
		}
	}

	free(surfaceFormats);
	return bestSurfaceFormat;
}

VkPresentModeKHR getBestPresentMode(VkSurfaceKHR *pSurface, VkPhysicalDevice *pPhysicalDevice){
	// FIFO (vsync) is the only mode the spec guarantees, and it's what we
	// want anyway: no tearing, and frame pacing tied to the display.
	return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D getBestSwapchainExtent(VkSurfaceCapabilitiesKHR *pSurfaceCapabilities, SDL_Window *window){
	// A defined currentExtent means the surface dictates the size exactly;
	// we must match it and are not allowed to substitute our own dimensions.
	if(pSurfaceCapabilities->currentExtent.width != 0xFFFFFFFF){
		return pSurfaceCapabilities->currentExtent;
	}

	// Otherwise we choose, but must clamp to the surface's allowed range.
	int FramebufferWidth = 0, FramebufferHeight = 0;
	SDL_GetWindowSizeInPixels(window, &FramebufferWidth, &FramebufferHeight);

	VkExtent2D bestSwapchainExtent = {(uint32_t)FramebufferWidth, (uint32_t)FramebufferHeight};

	if(bestSwapchainExtent.width < pSurfaceCapabilities->minImageExtent.width)
		bestSwapchainExtent.width = pSurfaceCapabilities->minImageExtent.width;
	if(bestSwapchainExtent.width > pSurfaceCapabilities->maxImageExtent.width)
		bestSwapchainExtent.width = pSurfaceCapabilities->maxImageExtent.width;
	if(bestSwapchainExtent.height < pSurfaceCapabilities->minImageExtent.height)
		bestSwapchainExtent.height = pSurfaceCapabilities->minImageExtent.height;
	if(bestSwapchainExtent.height > pSurfaceCapabilities->maxImageExtent.height)
		bestSwapchainExtent.height = pSurfaceCapabilities->maxImageExtent.height;

	return bestSwapchainExtent;
}

VkSwapchainKHR createSwapchain(VkDevice *pDevice, VkSurfaceKHR *pSurface, VkSurfaceCapabilitiesKHR *pSurfaceCapabilities, VkSurfaceFormatKHR *pSurfaceFormat, VkExtent2D *pSwapchainExtent, VkPresentModeKHR *pPresentMode, uint32_t imageArrayLayers){
	// One more than the minimum lets us keep working on the next frame while
	// one image is being presented, but must not exceed the driver's maximum
	// (maxImageCount == 0 means "no maximum").
	uint32_t imageCount = pSurfaceCapabilities->minImageCount + 1;
	if(pSurfaceCapabilities->maxImageCount > 0 && imageCount > pSurfaceCapabilities->maxImageCount){
		imageCount = pSurfaceCapabilities->maxImageCount;
	}

	// OPAQUE is what we want (ignore window alpha) but not every compositor
	// offers it; INHERIT (compositor decides) is the usual alternative.
	// TRANSFER_SRC lets us copy a presented image back to the CPU for
	// screenshots (vulkan_screenshot). Every real driver supports it, but
	// only add it when the surface advertises it so creation can't fail.
	VkImageUsageFlags imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	if (pSurfaceCapabilities->supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
		imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

	VkCompositeAlphaFlagBitsKHR compositeAlpha =
		(pSurfaceCapabilities->supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR) ? VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR :
		(pSurfaceCapabilities->supportedCompositeAlpha & VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR) ? VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR :
		(pSurfaceCapabilities->supportedCompositeAlpha & VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR) ? VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR :
		VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR;

	VkSwapchainCreateInfoKHR swapchainCreateInfo = {
		VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		NULL,
		0,
		*pSurface,
		imageCount,
		pSurfaceFormat->format,
		pSurfaceFormat->colorSpace,
		*pSwapchainExtent,
		imageArrayLayers,
		imageUsage,
		VK_SHARING_MODE_EXCLUSIVE,
		0,
		NULL,
		pSurfaceCapabilities->currentTransform,
		compositeAlpha,
		*pPresentMode,
		VK_TRUE,
		VK_NULL_HANDLE
	};

	VkSwapchainKHR swapchain;
	VKCHECK(vkCreateSwapchainKHR(*pDevice, &swapchainCreateInfo, NULL, &swapchain));
	return swapchain;
}

void deleteSwapchain(VkDevice *pDevice, VkSwapchainKHR *pSwapchain){
	vkDestroySwapchainKHR(*pDevice, *pSwapchain, NULL);
}

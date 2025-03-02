#include "main.c"

uint32_t getSwapchainImageNumber(VkDevice *pDevice, VkSwapchainKHR *pSwapchain){
	uint32_t swapchainImageNumber;
	vkGetSwapchainImagesKHR(*pDevice, *pSwapchain, &swapchainImageNumber, VK_NULL_HANDLE);
	return swapchainImageNumber;
}

VkImage *getSwapchainImages(VkDevice *pDevice, VkSwapchainKHR *pSwapchain, uint32_t swapchainImageNumber){
	VkImage *swapchainImages = (VkImage *)malloc(swapchainImageNumber * sizeof(VkImage));
	vkGetSwapchainImagesKHR(*pDevice, *pSwapchain, &swapchainImageNumber, swapchainImages);
	return swapchainImages;
}

void deleteSwapchainImages(VkImage **ppImages){
	free(*ppImages);
}

VkImageView *createImageViews(VkDevice *pDevice, VkImage **ppImages, VkSurfaceFormatKHR *pFormat, uint32_t imageNumber, uint32_t imageArrayLayers){
	VkComponentMapping componentMapping = {
		VK_COMPONENT_SWIZZLE_IDENTITY,
		VK_COMPONENT_SWIZZLE_IDENTITY,
		VK_COMPONENT_SWIZZLE_IDENTITY,
		VK_COMPONENT_SWIZZLE_IDENTITY
	};

	VkImageSubresourceRange imageSubresourceRange = {
		VK_IMAGE_ASPECT_COLOR_BIT,
		0,
		1,
		0,
		imageArrayLayers
	};

	VkImageViewCreateInfo *imageViewCreateInfo = (VkImageViewCreateInfo *)malloc(imageNumber * sizeof(VkImageViewCreateInfo));
	VkImageView *imageViews = (VkImageView *)malloc(imageNumber * sizeof(VkImageView));

	for(uint32_t i = 0; i < imageNumber; i++){
		imageViewCreateInfo[i].sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewCreateInfo[i].pNext = VK_NULL_HANDLE;
		imageViewCreateInfo[i].flags = 0;
		imageViewCreateInfo[i].image = (*ppImages)[i];
		imageViewCreateInfo[i].viewType = VK_IMAGE_VIEW_TYPE_2D;
		imageViewCreateInfo[i].format = pFormat->format;
		imageViewCreateInfo[i].components = componentMapping;
		imageViewCreateInfo[i].subresourceRange = imageSubresourceRange;

		vkCreateImageView(*pDevice, &(imageViewCreateInfo[i]), VK_NULL_HANDLE, &(imageViews[i]));
	}

	free(imageViewCreateInfo);
	return imageViews;
}

void deleteImageViews(VkDevice *pDevice, VkImageView **ppImageViews, uint32_t imageViewNumber){
	for(uint32_t i = 0; i < imageViewNumber; i++){
		vkDestroyImageView(*pDevice, (*ppImageViews)[i], VK_NULL_HANDLE);
	}
}

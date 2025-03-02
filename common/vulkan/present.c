#include "main.c"

void presentImage(VkDevice *pDevice, SDL_Window *window, VkCommandBuffer *pCommandBuffers, VkFence *pFrontFences, VkFence *pBackFences, VkSemaphore *pWaitSemaphores, VkSemaphore *pSignalSemaphores, VkSwapchainKHR *pSwapchain, VkQueue *pDrawingQueue, VkQueue *pPresentingQueue, uint32_t maxFrames){
	uint32_t currentFrame = 0;
        SDL_Event event;
        bool running = true;

	while (running) {
                while (SDL_PollEvent(&event)) {
                        if (event.type == SDL_EVENT_QUIT)
                                running = false;
                }

		vkWaitForFences(*pDevice, 1, &pFrontFences[currentFrame], VK_TRUE, UINT64_MAX);
		uint32_t imageIndex = 0;
		vkAcquireNextImageKHR(*pDevice, *pSwapchain, UINT64_MAX, pWaitSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);
		if(pBackFences[imageIndex] != VK_NULL_HANDLE){
			vkWaitForFences(*pDevice, 1, &pBackFences[imageIndex], VK_TRUE, UINT64_MAX);
		}
		pBackFences[imageIndex] = pFrontFences[currentFrame];

		VkPipelineStageFlags pipelineStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

		VkSubmitInfo submitInfo = {
			VK_STRUCTURE_TYPE_SUBMIT_INFO,
			VK_NULL_HANDLE,
			1,
			&pWaitSemaphores[currentFrame],
			&pipelineStage,
			1,
			&pCommandBuffers[imageIndex],
			1,
			&pSignalSemaphores[currentFrame]
		};
		vkResetFences(*pDevice, 1, &pFrontFences[currentFrame]);
		vkQueueSubmit(*pDrawingQueue, 1, &submitInfo, pFrontFences[currentFrame]);

		VkPresentInfoKHR presentInfo = {
			VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
			VK_NULL_HANDLE,
			1,
			&pSignalSemaphores[currentFrame],
			1,
			&(*pSwapchain),
			&imageIndex,
			VK_NULL_HANDLE
		};
		vkQueuePresentKHR(*pPresentingQueue, &presentInfo);

		currentFrame = (currentFrame + 1) % maxFrames;
	}
	vkDeviceWaitIdle(*pDevice);
}

#include "main.c"

VkDevice createDevice(VkPhysicalDevice *pPhysicalDevice, uint32_t queueFamilyIndex, VkQueueFamilyProperties *pQueueFamilyProperties){
        float priorities[] = {1.0, 1.0};
        uint32_t queueCount = (pQueueFamilyProperties[queueFamilyIndex].queueCount == 2 ? 2 : 1);

        VkDeviceQueueCreateInfo deviceQueueCreateInfo = {
                VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                VK_NULL_HANDLE,
                0,
                queueFamilyIndex,
                queueCount,
                priorities,
        };

        const char* extensions[] = {
                VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        };
        VkPhysicalDeviceFeatures physicalDeviceFeatures;
        vkGetPhysicalDeviceFeatures(*pPhysicalDevice, &physicalDeviceFeatures);

        VkDeviceCreateInfo deviceCreateInfo = {
                VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                VK_NULL_HANDLE,
                0,
                1,
                &deviceQueueCreateInfo,
                0,
                VK_NULL_HANDLE,
                1,
                extensions,
                &physicalDeviceFeatures
        };

        VkDevice device;
        vkCreateDevice(*pPhysicalDevice, &deviceCreateInfo, VK_NULL_HANDLE, &device);

        return device;
}

void deleteDevice(VkDevice *pDevice){
        vkDestroyDevice(*pDevice, VK_NULL_HANDLE);
}

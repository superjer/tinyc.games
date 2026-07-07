#include "main.c"

VkDevice createDevice(VkPhysicalDevice *pPhysicalDevice, uint32_t queueFamilyIndex, VkQueueFamilyProperties *pQueueFamilyProperties){
        float priorities[] = {1.0, 1.0};
        uint32_t queueCount = (pQueueFamilyProperties[queueFamilyIndex].queueCount >= 2 ? 2 : 1);

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
                "VK_KHR_portability_subset", // only enabled if advertised
        };
        uint32_t extensionNumber = 1;

        // the spec requires VK_KHR_portability_subset to be enabled on
        // devices that advertise it (e.g. MoltenVK on Mac)
        {
                uint32_t count = 0;
                vkEnumerateDeviceExtensionProperties(*pPhysicalDevice, VK_NULL_HANDLE, &count, VK_NULL_HANDLE);
                VkExtensionProperties *props = calloc(count, sizeof *props);
                vkEnumerateDeviceExtensionProperties(*pPhysicalDevice, VK_NULL_HANDLE, &count, props);
                for (uint32_t i = 0; i < count; i++)
                        if (strcmp(props[i].extensionName, extensions[1]) == 0)
                                extensionNumber = 2;
                free(props);
        }

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
                extensionNumber,
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

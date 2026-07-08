#include "main.c"

VkDevice createDevice(VkPhysicalDevice *pPhysicalDevice, uint32_t queueFamilyIndex){
        float priority = 1.0f;

        VkDeviceQueueCreateInfo deviceQueueCreateInfo = {
                VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                NULL,
                0,
                queueFamilyIndex,
                1,
                &priority,
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
                vkEnumerateDeviceExtensionProperties(*pPhysicalDevice, NULL, &count, NULL);
                VkExtensionProperties *props = calloc(count, sizeof *props);
                vkEnumerateDeviceExtensionProperties(*pPhysicalDevice, NULL, &count, props);
                for (uint32_t i = 0; i < count; i++)
                        if (strcmp(props[i].extensionName, extensions[1]) == 0)
                                extensionNumber = 2;
                free(props);
        }

        // Enable only the features we actually use. Enabling everything the
        // GPU supports is legal but can cost performance (robustBufferAccess)
        // and hides portability problems by letting shaders quietly rely on
        // features another platform (e.g. MoltenVK) may lack.
        VkPhysicalDeviceFeatures supportedFeatures;
        vkGetPhysicalDeviceFeatures(*pPhysicalDevice, &supportedFeatures);
        VkPhysicalDeviceFeatures enabledFeatures = {0};
        enabledFeatures.geometryShader = supportedFeatures.geometryShader; // optional pipeline geometry stage

        VkDeviceCreateInfo deviceCreateInfo = {
                VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                NULL,
                0,
                1,
                &deviceQueueCreateInfo,
                0,
                NULL,
                extensionNumber,
                extensions,
                &enabledFeatures
        };

        VkDevice device;
        VKCHECK(vkCreateDevice(*pPhysicalDevice, &deviceCreateInfo, NULL, &device));

        return device;
}

void deleteDevice(VkDevice *pDevice){
        vkDestroyDevice(*pDevice, NULL);
}

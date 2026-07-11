#include "main.c"
#ifndef VULKAN_DEMO_BUFFER_C
#define VULKAN_DEMO_BUFFER_C

// buffer.c - memory-type lookup, generic buffer creation, and the simple
// host-visible vertex buffer bundle games rebuild every frame

// a vertex buffer with its backing memory, kept together so it can be
// allocated, refilled, and freed as one thing
struct allocation {
        VkDeviceMemory mem;
        VkBuffer buf;
        VkBufferCreateInfo buf_info;
};

uint32_t find_memory_type(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(*vk.bestPhysicalDevice, &memProperties);

        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
                if ((typeFilter & (1 << i)) &&
                        (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                        return i;
                }
        }

        fprintf(stderr, "Failed to find a suitable memory type!\n");
        exit(-12);
}

// create a buffer and allocate + bind memory for it in one call
void vulkan_create_buffer(VkDeviceSize size, VkBufferUsageFlags usage,
                  VkMemoryPropertyFlags properties, VkBuffer* buffer, VkDeviceMemory* bufferMemory) {
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VKCHECK(vkCreateBuffer(vk.device, &bufferInfo, NULL, buffer));

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(vk.device, *buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = find_memory_type(memRequirements.memoryTypeBits, properties);

    VKCHECK(vkAllocateMemory(vk.device, &allocInfo, NULL, bufferMemory));

    vkBindBufferMemory(vk.device, *buffer, *bufferMemory, 0);
}

void vulkan_allocate_vertex_buffer(size_t sz, struct allocation *alloc)
{
        alloc->buf_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        alloc->buf_info.size = sz;
        alloc->buf_info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        alloc->buf_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        vkCreateBuffer(vk.device, &alloc->buf_info, NULL, &alloc->buf);

        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(vk.device, alloc->buf, &memRequirements);

        VkMemoryAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = find_memory_type(memRequirements.memoryTypeBits,
                                                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        vkAllocateMemory(vk.device, &allocInfo, NULL, &alloc->mem);
        vkBindBufferMemory(vk.device, alloc->buf, alloc->mem, 0);
}

void vulkan_populate_vertex_buffer(void *buf, size_t sz, struct allocation *alloc)
{
        void *data;
        vkMapMemory(vk.device, alloc->mem, 0, alloc->buf_info.size, 0, &data);
        memcpy(data, buf, sz);
        vkUnmapMemory(vk.device, alloc->mem);
}

void vulkan_free_vertex_buffer(struct allocation *alloc)
{
        vkFreeMemory(vk.device, alloc->mem, NULL);
}

// one-time command buffer for setup work (uploads, layout transitions):
// begin, record into the returned buffer, then end to submit and wait
VkCommandBuffer vulkan_begin_commands()
{
        VkCommandBuffer cmd;
        VkCommandBufferAllocateInfo cmd_alloc = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .commandPool = vk.commandPool,
                .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                .commandBufferCount = 1,
        };
        vkAllocateCommandBuffers(vk.device, &cmd_alloc, &cmd);

        VkCommandBufferBeginInfo begin_info = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        };
        vkBeginCommandBuffer(cmd, &begin_info);
        return cmd;
}

void vulkan_end_commands(VkCommandBuffer cmd)
{
        vkEndCommandBuffer(cmd);

        VkSubmitInfo submit = { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                .commandBufferCount = 1, .pCommandBuffers = &cmd };
        vkQueueSubmit(vk.drawingQueue, 1, &submit, VK_NULL_HANDLE);
        vkQueueWaitIdle(vk.drawingQueue);
        vkFreeCommandBuffers(vk.device, vk.commandPool, 1, &cmd);
}

#endif // VULKAN_DEMO_BUFFER_C

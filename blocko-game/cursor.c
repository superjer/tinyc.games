#include "blocko.c"
#ifndef BLOCKO_CURSOR_C_INCLUDED
#define BLOCKO_CURSOR_C_INCLUDED

unsigned int cursor_prog_id;
unsigned int cursor_vbo, cursor_vao;

int cursor_screenw;
int cursor_screenh;

float cursor_buf[200];
float *cursor_buf_p = cursor_buf;

int cursor_pipe;

void cursor_rect(int x0, int y0, int x1, int y1)
{
        *cursor_buf_p++ = x0;
        *cursor_buf_p++ = y0;
        *cursor_buf_p++ = x1;
        *cursor_buf_p++ = y1;
        *cursor_buf_p++ = x1;
        *cursor_buf_p++ = y0;

        *cursor_buf_p++ = x1;
        *cursor_buf_p++ = y1;
        *cursor_buf_p++ = x0;
        *cursor_buf_p++ = y0;
        *cursor_buf_p++ = x0;
        *cursor_buf_p++ = y1;
}

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

void cursor(VkCommandBuffer cmdbuf)
{
        vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipelines[cursor_pipe].pipeline);

        struct allocation allocation = {};
        int w = vk.bestSwapchainExtent.width;
        int h = vk.bestSwapchainExtent.height;
        int w2 = w/2;
        int h2 = h/2;
        static int outer_n;
        static int inner_n;

        if (!allocation.buf)
        {
                cursor_buf_p = cursor_buf;

                cursor_rect(w2 - 25 + 1, h2 -  1 + 1, w2 -  9 + 1, h2 +  1 + 1);
                cursor_rect(w2 +  9 + 1, h2 -  1 + 1, w2 + 25 + 1, h2 +  1 + 1);
                cursor_rect(w2 -  1 + 1, h2 - 25 + 1, w2 +  1 + 1, h2 -  9 + 1);
                cursor_rect(w2 -  1 + 1, h2 +  9 + 1, w2 +  1 + 1, h2 + 25 + 1);

                outer_n = (cursor_buf_p - cursor_buf) / 2;

                cursor_rect(w2 - 25, h2 -  1, w2 -  9, h2 +  1);
                cursor_rect(w2 +  9, h2 -  1, w2 + 25, h2 +  1);
                cursor_rect(w2 -  1, h2 - 25, w2 +  1, h2 -  9);
                cursor_rect(w2 -  1, h2 +  9, w2 +  1, h2 + 25);

                inner_n = (cursor_buf_p - cursor_buf) / 2;

                vulkan_allocate_vertex_buffer(sizeof cursor_buf, &allocation);
                vulkan_populate_vertex_buffer(cursor_buf, sizeof cursor_buf, &allocation);
        }

        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(cmdbuf, 0, 1, &allocation.buf, offsets);

        //push constants
        float near = -100.f;
        float far = 100.f;
        float x = 1.f / (w / 2.f);
        float y = -1.f / (h / 2.f);
        float z = -1.f / ((far - near) / 2.f);
        float tz = -(far + near) / (far - near);
        struct {
                float ortho[16];
                float incolor[3];
                float padding;
        } push = {
                {
                        x, 0, 0,  0,
                        0, y, 0,  0,
                        0, 0, z,  0,
                       -1, 1, tz, 1
                },
                { 0.0f, 0.0f, 0.0f }
        };

        vkCmdPushConstants(
                cmdbuf,
                vk.pipelines[cursor_pipe].layout,
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                0,
                sizeof(push),
                &push
        );
        vkCmdDraw(cmdbuf, outer_n, 1, 0, 0);

        push.incolor[0] = 1.0f;
        push.incolor[1] = 1.0f;
        push.incolor[2] = 1.0f;
        vkCmdPushConstants(
                cmdbuf,
                vk.pipelines[cursor_pipe].layout,
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                0,
                sizeof(push),
                &push
        );
        vkCmdDraw(cmdbuf, inner_n - outer_n, 1, outer_n, 0);
}

void cursor_init()
{                         
        VkVertexInputAttributeDescription attributeDesc = {};
        attributeDesc.location = 0;
        attributeDesc.binding = 0;
        attributeDesc.format = VK_FORMAT_R32G32_SFLOAT; // Matches vec2
        attributeDesc.offset = 0;
        VkVertexInputBindingDescription bindingDesc = {};
        bindingDesc.binding = 0; // Must match attribute binding
        bindingDesc.stride = sizeof(float) * 2; // vec2: 2 floats
        bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        cursor_pipe = vulkan_make_pipeline(
                "shaders/cursor.vert.spv", NULL, "shaders/cursor.frag.spv",
                1, &bindingDesc, 1, &attributeDesc
        );
}

#endif // BLOCKO_CURSOR_C_INCLUDED

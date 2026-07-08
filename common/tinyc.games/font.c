#ifndef TINYCGAMES_FONT_C_INCLUDED
#define TINYCGAMES_FONT_C_INCLUDED

#include "utils.c"

#define FONT_CH_W 8
#define FONT_CH_H 12
#define FONT_PITCH 128
#define FONT_LINES 128
#define FONT_BUFLEN 16000

// Vulkan resources for font rendering
VkImage font_image;
VkDeviceMemory font_image_memory;
VkImageView font_image_view;
VkSampler font_sampler;
VkDescriptorSetLayout font_descriptor_set_layout;
VkDescriptorPool font_descriptor_pool;
VkDescriptorSet font_descriptor_set;
VkPipelineLayout font_pipeline_layout;
VkPipeline font_pipeline;
VkBuffer font_vertex_buffer;
VkDeviceMemory font_vertex_memory;

int font_screenw;
int font_screenh;

float font_buf[FONT_BUFLEN + 100];
float *font_buf_limit = font_buf + FONT_BUFLEN;
float *font_buf_p = font_buf;

// Track GPU buffer offset for multiple font_end() calls per frame
size_t font_gpu_offset = 0;

int font_spacing[256] = { 6,3,7,7,7,6,7,3,5,5,6,7,3,6,3,6,
                          6,6,6,6,6,6,6,6,6,6,3,3,5,6,5,7,
                          7,6,6,6,6,6,6,6,6,5,6,6,6,7,7,6,
                          6,6,6,6,7,6,7,7,7,7,6,5,6,5,7,7,
                          4,6,6,6,6,6,5,6,6,4,5,6,4,7,6,6,
                          6,6,5,6,5,6,6,7,6,6,6,5,3,5,6,6 };

char font_kerning[(256 - ' ') * 256];

void font_init()
{
        char font_data[FONT_PITCH * FONT_LINES] =
"................................................................................................................................"
"................................................................................................................................"
"|       |O      |OO OO  |       |  O    |       |       |O      |  O    |O      |       |       |       |       |       |   O   "
"|       |O      | O O   | O O   | OOO   |       | OO    |O      | O     | O     |       |  O    |       |       |       |   O   "
"|       |O      |       |OOOOO  |O O    |       |O      |       |O      |  O    | O O   |  O    |       |       |       |  O    "
"|       |O      |       | O O   | OOO   |O  O   | O     |       |O      |  O    |  O    |OOOOO  |       |OOOO   |       |  O    "
"|       |O      |       | O O   |  O O  |  O    |O O O  |       |O      |  O    | O O   |  O    |       |       |       | O     "
"|       |       |       |OOOOO  | OOO   | O     |O  O   |       | O     | O     |       |  O    |       |       |       | O     "
"|       |O      |       | O O   |  O    |O  O   | OO O  |       |  O    |O      |       |       |O      |       |O      |O      "
".................................................................................................O.......................O......"
"................................................................................................................................"
"................................................................................................................................"
"................................................................................................................................"
"................................................................................................................................"
"| OO    |  O    | OO    |OOO    |O      |OOOO   | OO    |OOOO   | OO    | OO    |       |       |       |       |       | OOO   "
"|O  O   | OO    |O  O   |   O   |O  O   |O      |O      |   O   |O  O   |O  O   |       |       |  O    |       |O      |O   O  "
"|O OO   |  O    |   O   |   O   |O  O   |OOO    |O      |   O   |O  O   |O  O   |       |       | O     |OOOO   | O     |    O  "
"|OO O   |  O    |  O    | OO    |O  O   |   O   |OOO    |  O    | OO    | OOO   |O      |O      |O      |       |  O    |   O   "
"|O  O   |  O    | O     |   O   |OOOO   |   O   |O  O   |  O    |O  O   |   O   |       |       | O     |OOOO   | O     |  O    "
"|O  O   |  O    |O      |   O   |   O   |O  O   |O  O   | O     |O  O   |   O   |       |       |  O    |       |O      |       "
"| OO    | OOO   |OOOO   |OOO    |   O   | OO    | OO    | O     | OO    | OO    |O      |O      |       |       |       |  O    "
".........................................................................................O......................................"
"................................................................................................................................"
"................................................................................................................................"
"................................................................................................................................"
"................................................................................................................................"
"| OOO   | OO    |OOO    | OO    |OOO    |OOOO   |OOOO   | OO    |O  O   |OOO    |   O   |O  O   |O      |O   O  |O   O  | OO    "
"|O   O  |O  O   |O  O   |O  O   |O  O   |O      |O      |O  O   |O  O   | O     |   O   |O  O   |O      |OO OO  |OO  O  |O  O   "
"|O OOO  |O  O   |O  O   |O      |O  O   |O      |O      |O      |O  O   | O     |   O   |O O    |O      |O O O  |O O O  |O  O   "
"|O O O  |OOOO   |OOO    |O      |O  O   |OOO    |OOO    |O OO   |OOOO   | O     |   O   |OO     |O      |O O O  |O  OO  |O  O   "
"|O OOO  |O  O   |O  O   |O      |O  O   |O      |O      |O  O   |O  O   | O     |   O   |O O    |O      |O   O  |O   O  |O  O   "
"|O      |O  O   |O  O   |O  O   |O  O   |O      |O      |O  O   |O  O   | O     |O  O   |O  O   |O      |O   O  |O   O  |O  O   "
"| OOO   |O  O   |OOO    | OO    |OOO    |OOOO   |O      | OOO   |O  O   |OOO    | OO    |O  O   |OOOO   |O   O  |O   O  | OO    "
"................................................................................................................................"
"................................................................................................................................"
"................................................................................................................................"
"................................................................................................................................"
"................................................................................................................................"
"|OOO    | OO    |OOO    | OOO   |OOOOO  |O  O   |O   O  |O   O  |O   O  |O   O  |OOOO   |OOO    |O      |OOO    |  O    |       "
"|O  O   |O  O   |O  O   |O      |  O    |O  O   |O   O  |O   O  |O   O  |O   O  |   O   |O      |O      |  O    | O O   |       "
"|O  O   |O  O   |O  O   |O      |  O    |O  O   |O   O  |O   O  | O O   |O   O  |  O    |O      | O     |  O    |O   O  |       "
"|OOO    |O  O   |OOO    | OO    |  O    |O  O   |O   O  |O O O  |  O    | OOO   | O     |O      | O     |  O    |       |       "
"|O      |O  O   |O O    |   O   |  O    |O  O   | O O   |O O O  | O O   |  O    |O      |O      |  O    |  O    |       |       "
"|O      |O  O   |O  O   |   O   |  O    |O  O   | O O   |O O O  |O   O  |  O    |O      |O      |  O    |  O    |       |       "
"|O      | OO    |O  O   |OOO    |  O    | OO    |  O    | O O   |O   O  |  O    |OOOO   |OOO    |   O   |OOO    |       |       "
"............O.......................................................................................O....................OOOOO.."
"................................................................................................................................"
"................................................................................................................................"
"................................................................................................................................"
"................................................................................................................................"
"|O      |       |O      |       |   O   |       |  O    |       |O      | O     |  O    |O      |OO     |       |       |       "
"| O     |       |O      |       |   O   |       | O     |       |O      |       |       |O      | O     |       |       |       "
"|       | OO    |OOO    | OOO   | OOO   | OO    |OOO    | OOO   |OOO    |OO     | OO    |O  O   | O     |OOOO   |OOO    | OO    "
"|       |   O   |O  O   |O      |O  O   |O  O   | O     |O  O   |O  O   | O     |  O    |O O    | O     |O O O  |O  O   |O  O   "
"|       | OOO   |O  O   |O      |O  O   |OOOO   | O     |O  O   |O  O   | O     |  O    |OO     | O     |O O O  |O  O   |O  O   "
"|       |O  O   |O  O   |O      |O  O   |O      | O     |O  O   |O  O   | O     |  O    |O O    | O     |O O O  |O  O   |O  O   "
"|       | OOO   |OOO    | OOO   | OOO   | OOO   | O     | OOO   |O  O   | O     |  O    |O  O   | O     |O O O  |O  O   | OO    "
"............................................................O......................O............................................"
"..........................................................OO.....................OO............................................."
"................................................................................................................................"
"................................................................................................................................"
"................................................................................................................................"
"|       |       |       |       |       |       |       |       |       |       |       |  O    |O      |O      | O O   |       "
"|       |       |       |       | O     |       |       |       |       |       |       | O     |O      | O     |O O    |       "
"|OOO    | OOO   |O O    | OOO   |OOO    |O  O   |O  O   |O O O  |O  O   |O  O   |OOOO   | O     |O      | O     |       |       "
"|O  O   |O  O   |OO     |O      | O     |O  O   |O  O   |O O O  |O  O   |O  O   |   O   |O      |O      |  O    |       |       "
"|O  O   |O  O   |O      | OO    | O     |O  O   |O  O   |O O O  | OO    |O  O   | OO    | O     |       | O     |       |       "
"|O  O   |O  O   |O      |   O   | O     |O  O   |O O    |O O O  |O  O   |O  O   |O      | O     |O      | O     |       |       "
"|OOO    | OOO   |O      |OOO    | OO    | OOO   |OO     | OOOO  |O  O   | OOO   |OOOO   |  O    |O      |O      |       |       "
".O..........O...............................................................O....................O.............................."
".O..........O.............................................................OO.....................O.............................."
"................................................................................................................................";

        for (int i = 0; i < FONT_PITCH * FONT_LINES; i++)
                font_data[i] = (font_data[i] == 'O') ? 0xFF : 0;

        // compute kerning
        for (int a = 0; a < '~' - ' '; a++) for (int b = 0; b < '~' - ' '; b++)
        {
                int achar = (a + ' ') | ('A' ^ 'a');
                int bchar = (b + ' ') | ('A' ^ 'a');
                if (achar < 'a' || achar > 'z' || bchar < 'a' || bchar > 'z')
                        continue;

                int overlap;
                int abase = (a % 16) * FONT_CH_W + (a / 16) * FONT_PITCH * FONT_CH_H + 1;
                int bbase = (b % 16) * FONT_CH_W + (b / 16) * FONT_PITCH * FONT_CH_H + 1;
                for (overlap = 1; overlap < 5; overlap++)
                {
                        int boff = font_spacing[a] - overlap;

                        for (int acol = 0; acol < FONT_CH_W; acol++)
                        {
                                int bcol = acol - boff;
                                if (bcol < 0) continue; // b is out of bounds

                                for (int row = 1; row < FONT_CH_H - 1; row++)
                                {
                                        int r0 = (row-1) * FONT_PITCH;
                                        int r1 = (row+0) * FONT_PITCH;
                                        int r2 = (row+1) * FONT_PITCH;
                                        int a0_on = font_data[abase + acol + r0] ? 1 : 0;
                                        int a1_on = font_data[abase + acol + r1] ? 1 : 0;
                                        int a2_on = font_data[abase + acol + r2] ? 1 : 0;
                                        int b_on  = font_data[bbase + bcol + r1] ? 1 : 0;
                                        if ((a0_on && b_on) || (a1_on && b_on) || (a2_on && b_on))
                                                goto collide;
                                }
                        }
                }

                collide:
                font_kerning[(a << 8) | b] = overlap - 3;
        }

        // === Vulkan font texture creation ===
        VkDeviceSize image_size = FONT_PITCH * FONT_LINES;

        // Create staging buffer
        VkBuffer staging_buffer;
        VkDeviceMemory staging_memory;
        VkBufferCreateInfo staging_info = {
                .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                .size = image_size,
                .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        };
        vkCreateBuffer(vk.device, &staging_info, NULL, &staging_buffer);

        VkMemoryRequirements mem_reqs;
        vkGetBufferMemoryRequirements(vk.device, staging_buffer, &mem_reqs);

        VkPhysicalDeviceMemoryProperties mem_props;
        vkGetPhysicalDeviceMemoryProperties(*vk.bestPhysicalDevice, &mem_props);
        uint32_t mem_type = 0;
        for (uint32_t i = 0; i < mem_props.memoryTypeCount; i++) {
                if ((mem_reqs.memoryTypeBits & (1 << i)) &&
                    (mem_props.memoryTypes[i].propertyFlags & (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))) {
                        mem_type = i;
                        break;
                }
        }

        VkMemoryAllocateInfo alloc_info = {
                .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                .allocationSize = mem_reqs.size,
                .memoryTypeIndex = mem_type,
        };
        vkAllocateMemory(vk.device, &alloc_info, NULL, &staging_memory);
        vkBindBufferMemory(vk.device, staging_buffer, staging_memory, 0);

        void *data;
        vkMapMemory(vk.device, staging_memory, 0, image_size, 0, &data);
        memcpy(data, font_data, image_size);
        vkUnmapMemory(vk.device, staging_memory);

        // Create font image (R8 format)
        VkImageCreateInfo image_info = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                .imageType = VK_IMAGE_TYPE_2D,
                .format = VK_FORMAT_R8_UNORM,
                .extent = { FONT_PITCH, FONT_LINES, 1 },
                .mipLevels = 1,
                .arrayLayers = 1,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .tiling = VK_IMAGE_TILING_OPTIMAL,
                .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        };
        vkCreateImage(vk.device, &image_info, NULL, &font_image);

        vkGetImageMemoryRequirements(vk.device, font_image, &mem_reqs);
        for (uint32_t i = 0; i < mem_props.memoryTypeCount; i++) {
                if ((mem_reqs.memoryTypeBits & (1 << i)) &&
                    (mem_props.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
                        mem_type = i;
                        break;
                }
        }
        alloc_info.allocationSize = mem_reqs.size;
        alloc_info.memoryTypeIndex = mem_type;
        vkAllocateMemory(vk.device, &alloc_info, NULL, &font_image_memory);
        vkBindImageMemory(vk.device, font_image, font_image_memory, 0);

        // Transition and copy
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

        VkImageMemoryBarrier barrier = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = font_image,
                .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
                .srcAccessMask = 0,
                .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        };
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                0, 0, NULL, 0, NULL, 1, &barrier);

        VkBufferImageCopy region = {
                .imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
                .imageExtent = { FONT_PITCH, FONT_LINES, 1 },
        };
        vkCmdCopyBufferToImage(cmd, staging_buffer, font_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                0, 0, NULL, 0, NULL, 1, &barrier);

        vkEndCommandBuffer(cmd);

        VkSubmitInfo submit = { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount = 1, .pCommandBuffers = &cmd };
        vkQueueSubmit(vk.drawingQueue, 1, &submit, VK_NULL_HANDLE);
        vkQueueWaitIdle(vk.drawingQueue);
        vkFreeCommandBuffers(vk.device, vk.commandPool, 1, &cmd);

        vkDestroyBuffer(vk.device, staging_buffer, NULL);
        vkFreeMemory(vk.device, staging_memory, NULL);

        // Create image view
        VkImageViewCreateInfo view_info = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .image = font_image,
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .format = VK_FORMAT_R8_UNORM,
                .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
        };
        vkCreateImageView(vk.device, &view_info, NULL, &font_image_view);

        // Create sampler
        VkSamplerCreateInfo sampler_info = {
                .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                .magFilter = VK_FILTER_NEAREST,
                .minFilter = VK_FILTER_NEAREST,
                .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        };
        vkCreateSampler(vk.device, &sampler_info, NULL, &font_sampler);

        // === Create descriptor set layout ===
        VkDescriptorSetLayoutBinding binding = {
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        };
        VkDescriptorSetLayoutCreateInfo layout_info = {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                .bindingCount = 1,
                .pBindings = &binding,
        };
        vkCreateDescriptorSetLayout(vk.device, &layout_info, NULL, &font_descriptor_set_layout);

        // === Create descriptor pool ===
        VkDescriptorPoolSize pool_size = { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 };
        VkDescriptorPoolCreateInfo pool_info = {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                .maxSets = 1,
                .poolSizeCount = 1,
                .pPoolSizes = &pool_size,
        };
        vkCreateDescriptorPool(vk.device, &pool_info, NULL, &font_descriptor_pool);

        // === Allocate descriptor set ===
        VkDescriptorSetAllocateInfo desc_alloc = {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                .descriptorPool = font_descriptor_pool,
                .descriptorSetCount = 1,
                .pSetLayouts = &font_descriptor_set_layout,
        };
        vkAllocateDescriptorSets(vk.device, &desc_alloc, &font_descriptor_set);

        // Update descriptor set
        VkDescriptorImageInfo image_desc = {
                .sampler = font_sampler,
                .imageView = font_image_view,
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        };
        VkWriteDescriptorSet write = {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = font_descriptor_set,
                .dstBinding = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .pImageInfo = &image_desc,
        };
        vkUpdateDescriptorSets(vk.device, 1, &write, 0, NULL);

        // === Create vertex buffer ===
        VkBufferCreateInfo vb_info = {
                .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                .size = sizeof(font_buf),
                .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        };
        vkCreateBuffer(vk.device, &vb_info, NULL, &font_vertex_buffer);

        vkGetBufferMemoryRequirements(vk.device, font_vertex_buffer, &mem_reqs);
        for (uint32_t i = 0; i < mem_props.memoryTypeCount; i++) {
                if ((mem_reqs.memoryTypeBits & (1 << i)) &&
                    (mem_props.memoryTypes[i].propertyFlags & (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))) {
                        mem_type = i;
                        break;
                }
        }
        alloc_info.allocationSize = mem_reqs.size;
        alloc_info.memoryTypeIndex = mem_type;
        vkAllocateMemory(vk.device, &alloc_info, NULL, &font_vertex_memory);
        vkBindBufferMemory(vk.device, font_vertex_buffer, font_vertex_memory, 0);

        // === Create pipeline layout ===
        VkPushConstantRange push_range = {
                .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                .offset = 0,
                .size = 80,  // mat4 (64) + vec3 (12) + padding (4)
        };
        VkPipelineLayoutCreateInfo pl_layout_info = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                .setLayoutCount = 1,
                .pSetLayouts = &font_descriptor_set_layout,
                .pushConstantRangeCount = 1,
                .pPushConstantRanges = &push_range,
        };
        vkCreatePipelineLayout(vk.device, &pl_layout_info, NULL, &font_pipeline_layout);

        // === Create font pipeline with alpha blending ===
        uint32_t vert_size = 0, frag_size = 0;
        char *vert_code = getShaderCode(TINYC_SPV_DIR "font.vert.spv", &vert_size);
        char *frag_code = getShaderCode(TINYC_SPV_DIR "font.frag.spv", &frag_size);
        if (!vert_code || !frag_code)
        {
                fprintf(stderr, "font shaders not found in " TINYC_SPV_DIR "\n");
                exit(-1);
        }

        VkShaderModule vert_module = createShaderModule(&vk.device, vert_code, vert_size);
        VkShaderModule frag_module = createShaderModule(&vk.device, frag_code, frag_size);

        VkPipelineShaderStageCreateInfo stages[] = {
                { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, NULL, 0, VK_SHADER_STAGE_VERTEX_BIT, vert_module, "main", NULL },
                { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, NULL, 0, VK_SHADER_STAGE_FRAGMENT_BIT, frag_module, "main", NULL },
        };

        VkVertexInputBindingDescription bind_desc = { 0, 4 * sizeof(float), VK_VERTEX_INPUT_RATE_VERTEX };
        VkVertexInputAttributeDescription attr_desc = { 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 0 };

        VkPipelineVertexInputStateCreateInfo vertex_input = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
                .vertexBindingDescriptionCount = 1,
                .pVertexBindingDescriptions = &bind_desc,
                .vertexAttributeDescriptionCount = 1,
                .pVertexAttributeDescriptions = &attr_desc,
        };

        VkPipelineInputAssemblyStateCreateInfo input_assembly = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
                .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        };

        // Use dynamic viewport/scissor so we don't need to recreate pipeline on resize
        VkPipelineViewportStateCreateInfo viewport_state = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
                .viewportCount = 1,
                .scissorCount = 1,
        };

        VkDynamicState dynamic_states[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
        VkPipelineDynamicStateCreateInfo dynamic_state = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
                .dynamicStateCount = 2,
                .pDynamicStates = dynamic_states,
        };

        VkPipelineRasterizationStateCreateInfo rasterizer = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
                .polygonMode = VK_POLYGON_MODE_FILL,
                .cullMode = VK_CULL_MODE_NONE,
                .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
                .lineWidth = 1.0f,
        };

        VkPipelineMultisampleStateCreateInfo multisampling = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
                .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        };

        VkPipelineColorBlendAttachmentState blend_attachment = {
                .blendEnable = VK_TRUE,
                .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
                .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                .colorBlendOp = VK_BLEND_OP_ADD,
                .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
                .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
                .alphaBlendOp = VK_BLEND_OP_ADD,
                .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
        };

        VkPipelineColorBlendStateCreateInfo color_blend = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
                .attachmentCount = 1,
                .pAttachments = &blend_attachment,
        };

        VkPipelineDepthStencilStateCreateInfo depth_stencil = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
                .depthTestEnable = VK_FALSE,
                .depthWriteEnable = VK_FALSE,
        };

        VkGraphicsPipelineCreateInfo pipeline_info = {
                .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                .stageCount = 2,
                .pStages = stages,
                .pVertexInputState = &vertex_input,
                .pInputAssemblyState = &input_assembly,
                .pViewportState = &viewport_state,
                .pRasterizationState = &rasterizer,
                .pMultisampleState = &multisampling,
                .pDepthStencilState = &depth_stencil,
                .pColorBlendState = &color_blend,
                .pDynamicState = &dynamic_state,
                .layout = font_pipeline_layout,
                .renderPass = vk.renderPass,
                .subpass = 0,
        };
        vkCreateGraphicsPipelines(vk.device, VK_NULL_HANDLE, 1, &pipeline_info, NULL, &font_pipeline);

        vkDestroyShaderModule(vk.device, vert_module, NULL);
        vkDestroyShaderModule(vk.device, frag_module, NULL);
        free(vert_code);
        free(frag_code);

        fprintf(stderr, "Font Vulkan resources initialized\n");
}

void font_frame_reset()
{
        font_gpu_offset = 0;
}

void font_begin(int w, int h)
{
        font_screenw = w;
        font_screenh = h;
        font_buf_p = font_buf;
}

void font_add_text(char *s, int inx, int iny, float scale)
{
        int x = inx;
        int y = iny;

        if (!scale)
                scale = MIN(roundf(font_screenw / 600.f), roundf(font_screenh / 400.f));

        for (; *s && font_buf_p < font_buf_limit; s++)
        {
                if (*s == '\n')
                {
                        x = inx;
                        y += FONT_CH_H * scale;
                        continue;
                }

                int c = *s - ' ';
                if (c < 0) c = 0;
                float u = (c * FONT_CH_W) % FONT_PITCH;
                float v = (c / (FONT_PITCH / FONT_CH_W)) * FONT_CH_H;

                if (c) // don't render spaces
                {
                        *font_buf_p++ = x;
                        *font_buf_p++ = y;
                        *font_buf_p++ = u / FONT_PITCH;
                        *font_buf_p++ = v / FONT_LINES;

                        *font_buf_p++ = x + font_spacing[c] * scale;
                        *font_buf_p++ = y;
                        *font_buf_p++ = (u + font_spacing[c]) / FONT_PITCH;
                        *font_buf_p++ = v / FONT_LINES;

                        *font_buf_p++ = x;
                        *font_buf_p++ = y + FONT_CH_H * scale;
                        *font_buf_p++ = u / FONT_PITCH;
                        *font_buf_p++ = (v + FONT_CH_H) / FONT_LINES;

                        *font_buf_p++ = x + font_spacing[c] * scale;
                        *font_buf_p++ = y;
                        *font_buf_p++ = (u + font_spacing[c]) / FONT_PITCH;
                        *font_buf_p++ = v / FONT_LINES;

                        *font_buf_p++ = x;
                        *font_buf_p++ = y + FONT_CH_H * scale;
                        *font_buf_p++ = u / FONT_PITCH;
                        *font_buf_p++ = (v + FONT_CH_H) / FONT_LINES;

                        *font_buf_p++ = x + font_spacing[c] * scale;
                        *font_buf_p++ = y + FONT_CH_H * scale;
                        *font_buf_p++ = (u + font_spacing[c]) / FONT_PITCH;
                        *font_buf_p++ = (v + FONT_CH_H) / FONT_LINES;
                }

                int kern = font_spacing[c] - 1 - font_kerning[(c << 8) | (s[1] - ' ')];
                x += kern * scale;
        }
}

void font_end(float r, float g, float b)
{
        int n = (font_buf_p - font_buf);  // number of floats
        int vertex_count = n / 4;  // 4 floats per vertex

        if (vertex_count == 0) return;

        // Check if we have space in the buffer
        size_t bytes_needed = n * sizeof(float);
        if (font_gpu_offset + bytes_needed > sizeof(font_buf)) {
                fprintf(stderr, "Font buffer overflow!\n");
                return;
        }

        // Upload vertex data at current offset
        void *data;
        vkMapMemory(vk.device, font_vertex_memory, font_gpu_offset, bytes_needed, 0, &data);
        memcpy(data, font_buf, bytes_needed);
        vkUnmapMemory(vk.device, font_vertex_memory);

        // Build orthographic projection matrix for Vulkan (Y=0 at top, Y increases downward)
        float px = 2.f / font_screenw;
        float py = 2.f / font_screenh;  // Positive for Vulkan's Y-down convention

        // Push constant data: mat4 proj (64 bytes) + vec3 color (12 bytes) + padding (4 bytes)
        struct {
                float proj[16];
                float color[3];
                float _pad;
        } push = {
                .proj = {
                        px, 0,  0,  0,
                        0,  py, 0,  0,
                        0,  0,  1,  0,
                       -1, -1,  0,  1,
                },
                .color = { r, g, b },
        };

        VkCommandBuffer cmd = vk.commandBuffers[vk.imageIndex];

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, font_pipeline);

        // Set dynamic viewport and scissor to current screen size
        VkViewport viewport = { 0, 0, font_screenw, font_screenh, 0, 1 };
        VkRect2D scissor = { {0, 0}, {font_screenw, font_screenh} };
        vkCmdSetViewport(cmd, 0, 1, &viewport);
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, font_pipeline_layout, 0, 1, &font_descriptor_set, 0, NULL);
        vkCmdPushConstants(cmd, font_pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push), &push);

        VkDeviceSize vb_offset = font_gpu_offset;
        vkCmdBindVertexBuffers(cmd, 0, 1, &font_vertex_buffer, &vb_offset);
        vkCmdDraw(cmd, vertex_count, 1, 0, 0);

        // Advance offset for next font_end() call this frame
        font_gpu_offset += bytes_needed;
}

#endif // TINYCGAMES_FONT_C_INCLUDED

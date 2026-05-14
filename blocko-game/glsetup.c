#include "blocko.c"
#ifndef BLOCKO_GLSETUP_C_INCLUDED
#define BLOCKO_GLSETUP_C_INCLUDED

void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                  VkMemoryPropertyFlags properties, VkBuffer* buffer, VkDeviceMemory* bufferMemory) {
    
    // 1. Create the buffer
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(vk.device, &bufferInfo, NULL, buffer) != VK_SUCCESS) {
        printf("Failed to create buffer!\n");
        exit(1);
    }

    // 2. Get memory requirements
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(vk.device, *buffer, &memRequirements);

    // 3. Allocate memory
    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = find_memory_type(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(vk.device, &allocInfo, NULL, bufferMemory) != VK_SUCCESS) {
        printf("Failed to allocate buffer memory!\n");
        exit(1);
    }

    // 4. Bind buffer to allocated memory
    vkBindBufferMemory(vk.device, *buffer, *bufferMemory, 0);
}

void createUniformBuffer(VkBuffer* buffer, VkDeviceMemory* bufferMemory) {
    VkDeviceSize bufferSize = sizeof(struct main_ubo);

    createBuffer(bufferSize, 
                 VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 buffer, bufferMemory);
}

void createDescriptorSetLayout(VkDescriptorSetLayout* descriptorSetLayout) {
    // Binding 0: UBO, Binding 1: texture array, Bindings 2-7: shadow maps
    VkDescriptorSetLayoutBinding bindings[2 + SHADOW_COUNT] = {0};

    // UBO binding
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    // Texture array binding
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    // Shadow map bindings
    for (int i = 0; i < SHADOW_COUNT; i++) {
        bindings[2 + i].binding = 2 + i;
        bindings[2 + i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[2 + i].descriptorCount = 1;
        bindings[2 + i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    }

    VkDescriptorSetLayoutCreateInfo layoutInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 2 + SHADOW_COUNT,
        .pBindings = bindings,
    };

    vkCreateDescriptorSetLayout(vk.device, &layoutInfo, NULL, descriptorSetLayout);
}

void create_texture_array(char **files, int file_count) {
    // Load first texture to get dimensions
    int tex_w, tex_h, tex_n;
    unsigned char *first = stbi_load(files[0], &tex_w, &tex_h, &tex_n, 4);
    if (!first) { fprintf(stderr, "Failed to load texture %s\n", files[0]); exit(1); }
    stbi_image_free(first);

    // Calculate mip levels
    uint32_t mip_levels = (uint32_t)floor(log2(tex_w > tex_h ? tex_w : tex_h)) + 1;

    VkDeviceSize layer_size = tex_w * tex_h * 4;
    VkDeviceSize total_size = layer_size * file_count;

    // Create staging buffer
    VkBuffer staging_buffer;
    VkDeviceMemory staging_memory;
    VkBufferCreateInfo staging_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = total_size,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
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

    // Load all textures into staging buffer
    void *data;
    vkMapMemory(vk.device, staging_memory, 0, total_size, 0, &data);
    for (int i = 0; i < file_count; i++) {
        int w, h, n;
        unsigned char *texels = stbi_load(files[i], &w, &h, &n, 4);  // Force RGBA
        if (!texels) { fprintf(stderr, "Failed to load texture %s\n", files[i]); continue; }
        memcpy((char*)data + i * layer_size, texels, layer_size);
        stbi_image_free(texels);
    }
    vkUnmapMemory(vk.device, staging_memory);

    // Create texture image with mip levels
    VkImageCreateInfo image_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .extent = { tex_w, tex_h, 1 },
        .mipLevels = mip_levels,
        .arrayLayers = file_count,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    vkCreateImage(vk.device, &image_info, NULL, &texture_image);

    vkGetImageMemoryRequirements(vk.device, texture_image, &mem_reqs);
    for (uint32_t i = 0; i < mem_props.memoryTypeCount; i++) {
        if ((mem_reqs.memoryTypeBits & (1 << i)) &&
            (mem_props.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
            mem_type = i;
            break;
        }
    }
    alloc_info.allocationSize = mem_reqs.size;
    alloc_info.memoryTypeIndex = mem_type;
    vkAllocateMemory(vk.device, &alloc_info, NULL, &texture_memory);
    vkBindImageMemory(vk.device, texture_image, texture_memory, 0);

    // Transition and copy using a one-time command buffer
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

    // Transition all mip levels to TRANSFER_DST
    VkImageMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = texture_image,
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, mip_levels, 0, file_count },
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
    };
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, NULL, 0, NULL, 1, &barrier);

    // Copy each layer
    VkBufferImageCopy *regions = malloc(file_count * sizeof(VkBufferImageCopy));
    for (int i = 0; i < file_count; i++) {
        regions[i] = (VkBufferImageCopy){
            .bufferOffset = i * layer_size,
            .bufferRowLength = 0,
            .bufferImageHeight = 0,
            .imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, i, 1 },
            .imageOffset = { 0, 0, 0 },
            .imageExtent = { tex_w, tex_h, 1 },
        };
    }
    vkCmdCopyBufferToImage(cmd, staging_buffer, texture_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, file_count, regions);
    free(regions);

    // Generate mipmaps via blitting
    int32_t mip_w = tex_w, mip_h = tex_h;
    for (uint32_t mip = 1; mip < mip_levels; mip++) {
        // Transition previous mip level to TRANSFER_SRC
        barrier.subresourceRange.baseMipLevel = mip - 1;
        barrier.subresourceRange.levelCount = 1;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            0, 0, NULL, 0, NULL, 1, &barrier);

        // Blit from previous mip to current mip for all layers
        VkImageBlit blit = {
            .srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, mip - 1, 0, file_count },
            .srcOffsets = { {0, 0, 0}, {mip_w, mip_h, 1} },
            .dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, mip, 0, file_count },
            .dstOffsets = { {0, 0, 0}, {mip_w > 1 ? mip_w / 2 : 1, mip_h > 1 ? mip_h / 2 : 1, 1} },
        };
        vkCmdBlitImage(cmd, texture_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            texture_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

        // Transition previous mip to SHADER_READ_ONLY
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0, 0, NULL, 0, NULL, 1, &barrier);

        mip_w = mip_w > 1 ? mip_w / 2 : 1;
        mip_h = mip_h > 1 ? mip_h / 2 : 1;
    }

    // Transition last mip level to SHADER_READ_ONLY
    barrier.subresourceRange.baseMipLevel = mip_levels - 1;
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

    // Cleanup staging
    vkDestroyBuffer(vk.device, staging_buffer, NULL);
    vkFreeMemory(vk.device, staging_memory, NULL);

    // Create image view with all mip levels
    VkImageViewCreateInfo view_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = texture_image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, mip_levels, 0, file_count },
    };
    vkCreateImageView(vk.device, &view_info, NULL, &texture_image_view);

    // Create sampler with mipmap support
    VkSamplerCreateInfo sampler_info = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_NEAREST,
        .minFilter = VK_FILTER_LINEAR,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .minLod = 0.0f,
        .maxLod = (float)mip_levels,
    };
    vkCreateSampler(vk.device, &sampler_info, NULL, &texture_sampler);

    fprintf(stderr, "Created texture array: %dx%d, %d layers, %d mip levels\n", tex_w, tex_h, file_count, mip_levels);
}

void create_shadow_render_pass() {
    VkAttachmentDescription depthAttachment = {
        .format = VK_FORMAT_D32_SFLOAT,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };

    VkAttachmentReference depthAttachmentRef = {
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    };

    VkSubpassDescription subpass = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 0,
        .pColorAttachments = NULL,
        .pDepthStencilAttachment = &depthAttachmentRef,
    };

    // Dependency for transitioning depth image to shader-readable after render pass
    VkSubpassDependency dependencies[2] = {
        {
            .srcSubpass = VK_SUBPASS_EXTERNAL,
            .dstSubpass = 0,
            .srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
            .srcAccessMask = VK_ACCESS_SHADER_READ_BIT,
            .dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
        },
        {
            .srcSubpass = 0,
            .dstSubpass = VK_SUBPASS_EXTERNAL,
            .srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            .srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
            .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
        },
    };

    VkRenderPassCreateInfo renderPassInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &depthAttachment,
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 2,
        .pDependencies = dependencies,
    };

    if (vkCreateRenderPass(vk.device, &renderPassInfo, NULL, &shadow_render_pass) != VK_SUCCESS) {
        fprintf(stderr, "Failed to create shadow render pass!\n");
        exit(1);
    }
    fprintf(stderr, "Created shadow render pass\n");
}

void create_shadow_maps() {
    VkPhysicalDeviceMemoryProperties mem_props;
    vkGetPhysicalDeviceMemoryProperties(*vk.bestPhysicalDevice, &mem_props);

    // Create shadow depth images
    VkImageCreateInfo imageInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_D32_SFLOAT,
        .extent = { SHADOW_SZ, SHADOW_SZ, 1 },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    // Create all shadow map images and allocate memory
    for (int i = 0; i < SHADOW_COUNT; i++) {
        vkCreateImage(vk.device, &imageInfo, NULL, &shadow[i].image);
        shadow[i].slot = -1;  // Initialize slot to uninitialized
    }

    // Allocate memory for shadow images
    VkMemoryRequirements mem_reqs;
    vkGetImageMemoryRequirements(vk.device, shadow[0].image, &mem_reqs);

    uint32_t mem_type = 0;
    for (uint32_t i = 0; i < mem_props.memoryTypeCount; i++) {
        if ((mem_reqs.memoryTypeBits & (1 << i)) &&
            (mem_props.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
            mem_type = i;
            break;
        }
    }

    VkMemoryAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = mem_reqs.size,
        .memoryTypeIndex = mem_type,
    };

    for (int i = 0; i < SHADOW_COUNT; i++) {
        vkAllocateMemory(vk.device, &allocInfo, NULL, &shadow[i].memory);
        vkBindImageMemory(vk.device, shadow[i].image, shadow[i].memory, 0);
    }

    // Create image views
    VkImageViewCreateInfo viewInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = VK_FORMAT_D32_SFLOAT,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
    };

    for (int i = 0; i < SHADOW_COUNT; i++) {
        viewInfo.image = shadow[i].image;
        vkCreateImageView(vk.device, &viewInfo, NULL, &shadow[i].image_view);
    }

    // Create shadow sampler with depth comparison
    VkSamplerCreateInfo samplerInfo = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
        .mipLodBias = 0.0f,
        .minLod = 0.0f,
        .maxLod = 1.0f,
        .compareEnable = VK_TRUE,
        .compareOp = VK_COMPARE_OP_LESS,
        .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE, // Outside shadow = lit
    };
    vkCreateSampler(vk.device, &samplerInfo, NULL, &shadow_sampler);

    // Transition shadow images to SHADER_READ_ONLY_OPTIMAL so they can be sampled
    // even when shadow mapping is disabled
    VkCommandBuffer cmd;
    VkCommandBufferAllocateInfo cmdAllocInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = vk.commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    vkAllocateCommandBuffers(vk.device, &cmdAllocInfo, &cmd);

    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    vkBeginCommandBuffer(cmd, &beginInfo);

    VkImageMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
    };

    for (int i = 0; i < SHADOW_COUNT; i++) {
        barrier.image = shadow[i].image;
        vkCmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0, 0, NULL, 0, NULL, 1, &barrier);
    }

    vkEndCommandBuffer(cmd);

    VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd,
    };
    vkQueueSubmit(vk.drawingQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(vk.drawingQueue);
    vkFreeCommandBuffers(vk.device, vk.commandPool, 1, &cmd);

    fprintf(stderr, "Created shadow maps: %dx%d x %d\n", SHADOW_SZ, SHADOW_SZ, SHADOW_COUNT);
}

void create_shadow_framebuffers() {
    VkFramebufferCreateInfo framebufferInfo = {
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .renderPass = shadow_render_pass,
        .attachmentCount = 1,
        .width = SHADOW_SZ,
        .height = SHADOW_SZ,
        .layers = 1,
    };

    for (int i = 0; i < SHADOW_COUNT; i++) {
        framebufferInfo.pAttachments = &shadow[i].image_view;
        if (vkCreateFramebuffer(vk.device, &framebufferInfo, NULL, &shadow[i].framebuffer) != VK_SUCCESS) {
            fprintf(stderr, "Failed to create shadow[%d] framebuffer!\n", i);
            exit(1);
        }
    }

    fprintf(stderr, "Created %d shadow framebuffers\n", SHADOW_COUNT);
}

void create_descriptor_pool_and_set() {
    // Create descriptor pool (sized for MAX_FRAMES_IN_FLIGHT descriptor sets)
    // 1 + SHADOW_COUNT samplers: texture array + shadow maps
    VkDescriptorPoolSize pool_sizes[] = {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, MAX_FRAMES_IN_FLIGHT },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, (1 + SHADOW_COUNT) * MAX_FRAMES_IN_FLIGHT },
    };
    VkDescriptorPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = MAX_FRAMES_IN_FLIGHT,
        .poolSizeCount = 2,
        .pPoolSizes = pool_sizes,
    };
    vkCreateDescriptorPool(vk.device, &pool_info, NULL, &descriptor_pool);

    // Allocate per-frame descriptor sets
    VkDescriptorSetLayout layouts[MAX_FRAMES_IN_FLIGHT];
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        layouts[i] = main_descriptor_set_layout;

    VkDescriptorSetAllocateInfo set_alloc = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = descriptor_pool,
        .descriptorSetCount = MAX_FRAMES_IN_FLIGHT,
        .pSetLayouts = layouts,
    };
    vkAllocateDescriptorSets(vk.device, &set_alloc, main_descriptor_set);

    // Prepare image infos
    VkDescriptorImageInfo texture_info = {
        .sampler = texture_sampler,
        .imageView = texture_image_view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    VkDescriptorImageInfo shadow_info[SHADOW_COUNT];
    for (int i = 0; i < SHADOW_COUNT; i++) {
        shadow_info[i] = (VkDescriptorImageInfo){
            .sampler = shadow_sampler,
            .imageView = shadow[i].image_view,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        };
    }

    // Update each per-frame descriptor set
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorBufferInfo buffer_info = {
            .buffer = main_buffer[i],
            .offset = 0,
            .range = sizeof(struct main_ubo),
        };

        // 2 base writes (UBO + texture) + SHADOW_COUNT shadow writes
        VkWriteDescriptorSet writes[2 + SHADOW_COUNT] = {
            {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = main_descriptor_set[i],
                .dstBinding = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .pBufferInfo = &buffer_info,
            },
            {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = main_descriptor_set[i],
                .dstBinding = 1,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .pImageInfo = &texture_info,
            },
        };

        for (int j = 0; j < SHADOW_COUNT; j++) {
            writes[2 + j] = (VkWriteDescriptorSet){
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = main_descriptor_set[i],
                .dstBinding = 2 + j,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .pImageInfo = &shadow_info[j],
            };
        }

        vkUpdateDescriptorSets(vk.device, 2 + SHADOW_COUNT, writes, 0, NULL);
    }
}

void allocate_world()
{
        world_buf_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        world_buf_info.size = sizeof vbuf;
        world_buf_info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        world_buf_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        for (int i = 0; i < VAOS; i++)
                vkCreateBuffer(vk.device, &world_buf_info, NULL, &world_buf[i]);

        VkMemoryRequirements mem_reqs;
        vkGetBufferMemoryRequirements(vk.device, world_buf[0], &mem_reqs);

        world_aligned_sz = ALIGN_UP(mem_reqs.size, mem_reqs.alignment);

        world_mem_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        world_mem_info.allocationSize = world_aligned_sz * VAOD;
        world_mem_info.memoryTypeIndex = find_memory_type(
                mem_reqs.memoryTypeBits, 
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );

        size_t mem_use = 0;
        int alloc_failures = 0;
        for (int j = 0; j < VAOW; j++)
        {
                mem_use += world_mem_info.allocationSize;
                VkResult result = vkAllocateMemory(vk.device, &world_mem_info, NULL, &world_mem[j]);
                if (result != VK_SUCCESS) {
                        fprintf(stderr, "ERROR: vkAllocateMemory failed for world_mem[%d]: %d\n", j, result);
                        alloc_failures++;
                        continue;
                }
                for (int k = 0; k < VAOD; k++)
                {
                        int buffer_idx = j * VAOD + k;
                        int offset = k * world_aligned_sz;
                        vkBindBufferMemory(vk.device, world_buf[buffer_idx], world_mem[j], offset);
                        //fprintf(stderr, "Buffer %d in allocation %d at offset %d / %lu\n", buffer_idx, j, offset, world_mem_info.allocationSize);
                }
                // Persistently map the memory (stays mapped for lifetime of app)
                vkMapMemory(vk.device, world_mem[j], 0, world_mem_info.allocationSize, 0, &world_mapped[j]);
        }

        fprintf(stderr, "World VRAM usage: %luMB (allocation size: %luMB x %d)\n",
                mem_use / 1024 / 1024,
                world_mem_info.allocationSize / 1024 / 1024,
                VAOW);

        if (alloc_failures > 0) {
                fprintf(stderr, "WARNING: %d memory allocations failed!\n", alloc_failures);
        }
}

//initial setup to get the window and rendering going
void glsetup()
{
        vulkan_startup("Blocko", TINYC_DIR "/blocko-game/assets/tinyc-icon.png", TINYC_DIR "/blocko-game/shaders/");

        // Ensure swapchain image count doesn't exceed our descriptor set array size
        if (vk.maxFrames > MAX_FRAMES_IN_FLIGHT) {
                fprintf(stderr, "ERROR: vk.maxFrames (%u) > MAX_FRAMES_IN_FLIGHT (%d). Increase MAX_FRAMES_IN_FLIGHT.\n",
                        vk.maxFrames, MAX_FRAMES_IN_FLIGHT);
                exit(1);
        }
        fprintf(stderr, "Vulkan: maxFrames=%u, swapchainImageCount=%u\n", vk.maxFrames, vk.swapchainImageCount);

        // Main terrain pipeline with vertex inputs matching struct vbufv
        VkVertexInputBindingDescription mainBindingDesc = {
                .binding = 0,
                .stride = sizeof(struct vbufv),
                .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
        };
        VkVertexInputAttributeDescription mainAttrDescs[] = {
                {.location = 0, .binding = 0, .format = VK_FORMAT_R32_SFLOAT, .offset = offsetof(struct vbufv, tex)},
                {.location = 1, .binding = 0, .format = VK_FORMAT_R32_SFLOAT, .offset = offsetof(struct vbufv, orient)},
                {.location = 2, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(struct vbufv, x)},
                {.location = 3, .binding = 0, .format = VK_FORMAT_R32G32B32A32_SFLOAT, .offset = offsetof(struct vbufv, illum0)},
                {.location = 4, .binding = 0, .format = VK_FORMAT_R32G32B32A32_SFLOAT, .offset = offsetof(struct vbufv, glow0)},
                {.location = 5, .binding = 0, .format = VK_FORMAT_R32_SFLOAT, .offset = offsetof(struct vbufv, alpha)},
                {.location = 6, .binding = 0, .format = VK_FORMAT_R32_SFLOAT, .offset = offsetof(struct vbufv, scale)},
        };

        // Must create descriptor set layout BEFORE using it in pipeline creation
        createDescriptorSetLayout(&main_descriptor_set_layout);
        main_pipe = vulkan_make_pipeline("main.vert", "main.geom", "main.frag",
                1, &mainBindingDesc, 7, mainAttrDescs, &main_descriptor_set_layout, VK_NULL_HANDLE, 0);
        water_pipe = vulkan_make_pipeline("main.vert", "main.geom", "main.frag",
                1, &mainBindingDesc, 7, mainAttrDescs, &main_descriptor_set_layout, VK_NULL_HANDLE, PIPE_BLEND);

        allocate_world();

        // Create per-frame UBOs to avoid race conditions
        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
                createUniformBuffer(&main_buffer[i], &main_memory[i]);

        SDL_SetWindowRelativeMouseMode(vk.window, true);

        char *texture_files[] = {
                TINYC_DIR "/blocko-game/assets/grass_top.png",       //  0
                TINYC_DIR "/blocko-game/assets/grass_side.png",      //  1
                TINYC_DIR "/blocko-game/assets/dirt.png",            //  2
                TINYC_DIR "/blocko-game/assets/grass_grow1_top.png", //  3
                TINYC_DIR "/blocko-game/assets/grass_grow2_top.png", //  4
                TINYC_DIR "/blocko-game/assets/stone.png",           //  5
                TINYC_DIR "/blocko-game/assets/sand.png",            //  6
                TINYC_DIR "/blocko-game/assets/water.png",           //  7
                TINYC_DIR "/blocko-game/assets/water2.png",          //  8
                TINYC_DIR "/blocko-game/assets/water3.png",          //  9
                TINYC_DIR "/blocko-game/assets/water4.png",          // 10
                TINYC_DIR "/blocko-game/assets/ore.png",             // 11
                TINYC_DIR "/blocko-game/assets/ore_hint.png",        // 12
                TINYC_DIR "/blocko-game/assets/hard.png",            // 13
                TINYC_DIR "/blocko-game/assets/wood_side.png",       // 14
                TINYC_DIR "/blocko-game/assets/granite.png",         // 15
                TINYC_DIR "/blocko-game/assets/leaves_red.png",      // 16
                TINYC_DIR "/blocko-game/assets/leaves_gold.png",     // 17
                TINYC_DIR "/blocko-game/assets/mushlite.png",        // 18
                TINYC_DIR "/blocko-game/assets/0.png",               // 19
                TINYC_DIR "/blocko-game/assets/1.png",
                TINYC_DIR "/blocko-game/assets/2.png",
                TINYC_DIR "/blocko-game/assets/3.png",
                TINYC_DIR "/blocko-game/assets/4.png",
                TINYC_DIR "/blocko-game/assets/5.png",
                TINYC_DIR "/blocko-game/assets/6.png",
                TINYC_DIR "/blocko-game/assets/7.png",
                TINYC_DIR "/blocko-game/assets/8.png",
                TINYC_DIR "/blocko-game/assets/9.png",
                TINYC_DIR "/blocko-game/assets/A.png",
                TINYC_DIR "/blocko-game/assets/B.png",
                TINYC_DIR "/blocko-game/assets/C.png",
                TINYC_DIR "/blocko-game/assets/D.png",
                TINYC_DIR "/blocko-game/assets/E.png",
                TINYC_DIR "/blocko-game/assets/F.png",               // 34
        };
        int texture_count = sizeof(texture_files) / sizeof(texture_files[0]);
        create_texture_array(texture_files, texture_count);

        // Create shadow mapping resources (must happen before create_descriptor_pool_and_set)
        create_shadow_render_pass();
        create_shadow_maps();
        create_shadow_framebuffers();

        // Create shadow pipeline with same vertex layout as main pipeline
        // Use main_descriptor_set_layout to access texture for alpha testing leaves
        shadow_pipe = vulkan_make_pipeline(
                "shadow.vert", "shadow.geom", "shadow.frag",
                1, &mainBindingDesc, 7, mainAttrDescs,
                &main_descriptor_set_layout, shadow_render_pass,
                PIPE_DEPTH_BIAS | PIPE_NO_CULL);

        create_descriptor_pool_and_set();

        // Create GPU timestamp query pool
        VkQueryPoolCreateInfo query_pool_info = {
                .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
                .queryType = VK_QUERY_TYPE_TIMESTAMP,
                .queryCount = GPU_TIMESTAMP_COUNT,
        };
        if (vkCreateQueryPool(vk.device, &query_pool_info, NULL, &gpu_timestamp_pool) != VK_SUCCESS) {
                fprintf(stderr, "Failed to create GPU timestamp query pool\n");
        } else {
                // Get timestamp period from device properties
                VkPhysicalDeviceProperties props;
                vkGetPhysicalDeviceProperties(*vk.bestPhysicalDevice, &props);
                gpu_timestamp_period = props.limits.timestampPeriod;
                fprintf(stderr, "GPU timestamp period: %.2f ns/tick\n", gpu_timestamp_period);
        }
}

#endif // BLOCKO_GLSETUP_C_INCLUDED

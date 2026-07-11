#include "blocko.c"
#ifndef BLOCKO_VKSETUP_C_INCLUDED
#define BLOCKO_VKSETUP_C_INCLUDED

void createUniformBuffer(VkBuffer* buffer, VkDeviceMemory* bufferMemory) {
    VkDeviceSize bufferSize = sizeof(struct main_ubo);

    vulkan_create_buffer(bufferSize,
                 VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 buffer, bufferMemory);
}

static int texture_w, texture_h, texture_mips; // for runtime layer updates

// extra_rgba appends extra_count generated layers (same dimensions, RGBA8)
// after the files - the player model face tiles ride along this way
void create_texture_array(char **files, int file_count, unsigned char *extra_rgba, int extra_count) {
    // Load first texture to get dimensions
    int tex_w, tex_h, tex_n;
    unsigned char *first = stbi_load(files[0], &tex_w, &tex_h, &tex_n, 4);
    if (!first) { fprintf(stderr, "Failed to load texture %s\n", files[0]); exit(1); }
    stbi_image_free(first);

    // Calculate mip levels
    uint32_t mip_levels = (uint32_t)floor(log2(tex_w > tex_h ? tex_w : tex_h)) + 1;

    int layer_count = file_count + extra_count;
    VkDeviceSize layer_size = tex_w * tex_h * 4;
    VkDeviceSize total_size = layer_size * layer_count;

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
    if (extra_count)
        memcpy((char*)data + file_count * layer_size, extra_rgba, extra_count * layer_size);
    vkUnmapMemory(vk.device, staging_memory);

    // Create texture image with mip levels
    VkImageCreateInfo image_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .extent = { tex_w, tex_h, 1 },
        .mipLevels = mip_levels,
        .arrayLayers = layer_count,
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
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, mip_levels, 0, layer_count },
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
    };
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, NULL, 0, NULL, 1, &barrier);

    // Copy each layer
    VkBufferImageCopy *regions = malloc(layer_count * sizeof(VkBufferImageCopy));
    for (int i = 0; i < layer_count; i++) {
        regions[i] = (VkBufferImageCopy){
            .bufferOffset = i * layer_size,
            .bufferRowLength = 0,
            .bufferImageHeight = 0,
            .imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, i, 1 },
            .imageOffset = { 0, 0, 0 },
            .imageExtent = { tex_w, tex_h, 1 },
        };
    }
    vkCmdCopyBufferToImage(cmd, staging_buffer, texture_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, layer_count, regions);
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
            .srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, mip - 1, 0, layer_count },
            .srcOffsets = { {0, 0, 0}, {mip_w, mip_h, 1} },
            .dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, mip, 0, layer_count },
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
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, mip_levels, 0, layer_count },
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

    texture_w = tex_w;
    texture_h = tex_h;
    texture_mips = mip_levels;
    fprintf(stderr, "Created texture array: %dx%d, %d layers, %d mip levels\n", tex_w, tex_h, layer_count, mip_levels);
}

// overwrite a range of texture array layers at runtime (a player model
// arriving over the net). Rare event, so a full device idle is fine.
void update_texture_layers(int first_layer, int layer_count, unsigned char *rgba)
{
    if (!texture_image) return; // not created yet: startup path covers it

    vkDeviceWaitIdle(vk.device); // frames in flight are sampling this image

    VkDeviceSize layer_size = texture_w * texture_h * 4;
    VkDeviceSize total_size = layer_size * layer_count;

    VkBuffer staging_buffer;
    VkDeviceMemory staging_memory;
    vulkan_create_buffer(total_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &staging_buffer, &staging_memory);
    void *data;
    vkMapMemory(vk.device, staging_memory, 0, total_size, 0, &data);
    memcpy(data, rgba, total_size);
    vkUnmapMemory(vk.device, staging_memory);

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

    // all mips of the affected layers: shader-read -> transfer-dst
    VkImageMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = texture_image,
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, texture_mips, first_layer, layer_count },
        .srcAccessMask = VK_ACCESS_SHADER_READ_BIT,
        .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
    };
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, NULL, 0, NULL, 1, &barrier);

    VkBufferImageCopy region = {
        .imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, first_layer, layer_count },
        .imageExtent = { texture_w, texture_h, 1 },
    };
    vkCmdCopyBufferToImage(cmd, staging_buffer, texture_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    // regenerate the mip chain for just these layers (same dance as creation)
    int32_t mip_w = texture_w, mip_h = texture_h;
    for (int mip = 1; mip < texture_mips; mip++) {
        barrier.subresourceRange.baseMipLevel = mip - 1;
        barrier.subresourceRange.levelCount = 1;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            0, 0, NULL, 0, NULL, 1, &barrier);

        VkImageBlit blit = {
            .srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, mip - 1, first_layer, layer_count },
            .srcOffsets = { {0, 0, 0}, {mip_w, mip_h, 1} },
            .dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, mip, first_layer, layer_count },
            .dstOffsets = { {0, 0, 0}, {mip_w > 1 ? mip_w / 2 : 1, mip_h > 1 ? mip_h / 2 : 1, 1} },
        };
        vkCmdBlitImage(cmd, texture_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            texture_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0, 0, NULL, 0, NULL, 1, &barrier);

        mip_w = mip_w > 1 ? mip_w / 2 : 1;
        mip_h = mip_h > 1 ? mip_h / 2 : 1;
    }
    barrier.subresourceRange.baseMipLevel = texture_mips - 1;
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
}

void create_shadow_maps() {
    // Depth images + views + framebuffers per cascade (sizes differ)
    for (int i = 0; i < SHADOW_COUNT; i++) {
        vulkan_create_depth_target(shadow_sz[i],
            &shadow[i].image, &shadow[i].memory, &shadow[i].image_view);
        vulkan_create_depth_framebuffer(shadow_render_pass, shadow[i].image_view,
            shadow_sz[i], &shadow[i].framebuffer);
        shadow[i].slot = -1;  // Initialize slot to uninitialized
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

    fprintf(stderr, "Created shadow maps: %d %d %d %d %d %d\n",
        shadow_sz[0], shadow_sz[1], shadow_sz[2], shadow_sz[3], shadow_sz[4], shadow_sz[5]);
}

void create_descriptor_pool_and_set() {
    // Sampled images: binding 1 is the texture array, 2.. are the shadow maps
    VkDescriptorImageInfo samplers[1 + SHADOW_COUNT] = {
        {
            .sampler = texture_sampler,
            .imageView = texture_image_view,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        },
    };
    for (int i = 0; i < SHADOW_COUNT; i++) {
        samplers[1 + i] = (VkDescriptorImageInfo){
            .sampler = shadow_sampler,
            .imageView = shadow[i].image_view,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        };
    }

    vulkan_create_descriptor_sets(main_descriptor_set_layout, MAX_FRAMES_IN_FLIGHT,
        main_buffer, sizeof(struct main_ubo),
        samplers, 1 + SHADOW_COUNT,
        &descriptor_pool, main_descriptor_set);
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
void vksetup()
{
        vulkan_startup(window_title, TINYC_DIR "/blocko-game/assets/tinyc-icon.png", TINYC_DIR "/blocko-game/shaders/", 1440, 900);

        // Ensure swapchain image count doesn't exceed our descriptor set array size
        if (vk.maxFrames > MAX_FRAMES_IN_FLIGHT) {
                fprintf(stderr, "ERROR: vk.maxFrames (%u) > MAX_FRAMES_IN_FLIGHT (%d). Increase MAX_FRAMES_IN_FLIGHT.\n",
                        vk.maxFrames, MAX_FRAMES_IN_FLIGHT);
                exit(1);
        }
        fprintf(stderr, "Vulkan: maxFrames=%u, swapchainImageCount=%u\n", vk.maxFrames, vk.swapchainImageCount);

        // Main terrain pipeline with vertex inputs matching struct vbufv
        // One instance per face; the vertex shader expands 4 strip corners
        VkVertexInputBindingDescription mainBindingDesc = {
                .binding = 0,
                .stride = sizeof(struct vbufv),
                .inputRate = VK_VERTEX_INPUT_RATE_INSTANCE
        };
        VkVertexInputAttributeDescription mainAttrDescs[] = {
                {.location = 0, .binding = 0, .format = VK_FORMAT_R32_SFLOAT, .offset = offsetof(struct vbufv, tex)},
                {.location = 1, .binding = 0, .format = VK_FORMAT_R32_SFLOAT, .offset = offsetof(struct vbufv, orient)},
                {.location = 2, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(struct vbufv, x)},
                {.location = 3, .binding = 0, .format = VK_FORMAT_R32G32B32A32_SFLOAT, .offset = offsetof(struct vbufv, illum0)},
                {.location = 4, .binding = 0, .format = VK_FORMAT_R32G32B32A32_SFLOAT, .offset = offsetof(struct vbufv, glow0)},
                {.location = 5, .binding = 0, .format = VK_FORMAT_R32_SFLOAT, .offset = offsetof(struct vbufv, alpha)},
        };

        // Must create descriptor set layout BEFORE using it in pipeline creation
        // binding 0: UBO, binding 1: texture array, bindings 2..: shadow maps
        vulkan_create_descriptor_set_layout(1 + SHADOW_COUNT, &main_descriptor_set_layout);
        main_pipe = vulkan_make_pipeline("main.vert", NULL, "main.frag",
                1, &mainBindingDesc, 6, mainAttrDescs, &main_descriptor_set_layout, VK_NULL_HANDLE, PIPE_TRIANGLE_STRIP);
        // no-cull so every water face is visible from both sides (you can see the
        // surface and the exposed walls/floor from inside the water as well as out)
        water_pipe = vulkan_make_pipeline("main.vert", NULL, "main.frag",
                1, &mainBindingDesc, 6, mainAttrDescs, &main_descriptor_set_layout, VK_NULL_HANDLE, PIPE_TRIANGLE_STRIP | PIPE_BLEND | PIPE_NO_CULL);
        // Far water renders solid: past the alpha ramp in main.frag every water
        // fragment is opaque anyway, so out there we drop the blend AND back-face
        // cull (you're never underneath distant water), roughly halving the
        // rasterized faces. draw.c draws these chunks front-to-back so early-Z
        // rejects the covered ones - all the wins the weak-GPU path wants.
        water_solid_pipe = vulkan_make_pipeline("main.vert", NULL, "main.frag",
                1, &mainBindingDesc, 6, mainAttrDescs, &main_descriptor_set_layout, VK_NULL_HANDLE, PIPE_TRIANGLE_STRIP);
        // mobs share the vertex layout and the lit fragment shader, but use their
        // own vertex shader (spins to face heading; no reject box)
        mob_pipe = vulkan_make_pipeline("mob.vert", NULL, "main.frag",
                1, &mainBindingDesc, 6, mainAttrDescs, &main_descriptor_set_layout, VK_NULL_HANDLE, PIPE_TRIANGLE_STRIP);

        // player models: instanced prism faces carrying their piece transform
        // as matrix rows (struct pmvert); lit by the shared main.frag
        VkVertexInputBindingDescription pmodelBindingDesc = {
                .binding = 0,
                .stride = sizeof(struct pmvert),
                .inputRate = VK_VERTEX_INPUT_RATE_INSTANCE
        };
        VkVertexInputAttributeDescription pmodelAttrDescs[] = {
                {.location = 0, .binding = 0, .format = VK_FORMAT_R32G32B32A32_SFLOAT, .offset = offsetof(struct pmvert, r0)},
                {.location = 1, .binding = 0, .format = VK_FORMAT_R32G32B32A32_SFLOAT, .offset = offsetof(struct pmvert, r1)},
                {.location = 2, .binding = 0, .format = VK_FORMAT_R32G32B32A32_SFLOAT, .offset = offsetof(struct pmvert, r2)},
                {.location = 3, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(struct pmvert, dims)},
                {.location = 4, .binding = 0, .format = VK_FORMAT_R32G32B32A32_SFLOAT, .offset = offsetof(struct pmvert, orient)},
        };
        pmodel_pipe = vulkan_make_pipeline("pmodel.vert", NULL, "main.frag",
                1, &pmodelBindingDesc, 5, pmodelAttrDescs, &main_descriptor_set_layout, VK_NULL_HANDLE, PIPE_TRIANGLE_STRIP);

        allocate_world();

        // Create per-frame UBOs to avoid race conditions
        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
                createUniformBuffer(&main_buffer[i], &main_memory[i]);

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
                TINYC_DIR "/blocko-game/assets/slime_body.png",      // 35
                TINYC_DIR "/blocko-game/assets/slime_eyes.png",      // 36
                TINYC_DIR "/blocko-game/assets/grass_mtn_top.png",   // 37
                TINYC_DIR "/blocko-game/assets/grass_mtn_side.png",  // 38
                TINYC_DIR "/blocko-game/assets/leaves_spruce.png",   // 39
                TINYC_DIR "/blocko-game/assets/tall_grass.png",      // 40
                TINYC_DIR "/blocko-game/assets/tall_mtn_grass.png",  // 41
                TINYC_DIR "/blocko-game/assets/open.png",            // 42 (debug)
                TINYC_DIR "/blocko-game/assets/barrier.png",         // 43 (debug)
        };
        int texture_count = sizeof(texture_files) / sizeof(texture_files[0]);
        // the player model face tiles go in as extra layers after the assets
        pmodel_tex_base = texture_count;
        int pmodel_layers;
        unsigned char *pmodel_tiles = pmodel_make_tiles(&pmodel_layers);
        create_texture_array(texture_files, texture_count, pmodel_tiles, pmodel_layers);

        // Create shadow mapping resources (must happen before create_descriptor_pool_and_set)
        vulkan_create_depth_render_pass(&shadow_render_pass);
        create_shadow_maps();

        // Create shadow pipeline with same vertex layout as main pipeline
        // Use main_descriptor_set_layout to access texture for alpha testing leaves
        shadow_pipe = vulkan_make_pipeline(
                "shadow.vert", NULL, "shadow.frag",
                1, &mainBindingDesc, 6, mainAttrDescs,
                &main_descriptor_set_layout, shadow_render_pass,
                PIPE_TRIANGLE_STRIP | PIPE_DEPTH_BIAS | PIPE_NO_CULL);

        // far/extreme cascades treat leaves as solid, so their terrain draws
        // skip the alpha-test fragment shader entirely - an empty fragment
        // stage keeps the GPU on its fast depth-only path
        shadow_solid_pipe = vulkan_make_pipeline(
                "shadow.vert", NULL, "shadow_solid.frag",
                1, &mainBindingDesc, 6, mainAttrDescs,
                &main_descriptor_set_layout, shadow_render_pass,
                PIPE_TRIANGLE_STRIP | PIPE_DEPTH_BIAS | PIPE_NO_CULL);

        // mob shadow caster: spins with the mob, no reject box
        mob_shadow_pipe = vulkan_make_pipeline(
                "mob_shadow.vert", NULL, "shadow.frag",
                1, &mainBindingDesc, 6, mainAttrDescs,
                &main_descriptor_set_layout, shadow_render_pass,
                PIPE_TRIANGLE_STRIP | PIPE_DEPTH_BIAS | PIPE_NO_CULL);

        // player model shadow caster (this is how the local player sees
        // their own model in first person: shadow only)
        pmodel_shadow_pipe = vulkan_make_pipeline(
                "pmodel_shadow.vert", NULL, "shadow.frag",
                1, &pmodelBindingDesc, 5, pmodelAttrDescs,
                &main_descriptor_set_layout, shadow_render_pass,
                PIPE_TRIANGLE_STRIP | PIPE_DEPTH_BIAS | PIPE_NO_CULL);

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

#endif // BLOCKO_VKSETUP_C_INCLUDED

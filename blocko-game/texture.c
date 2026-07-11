#include "blocko.c"
#ifndef BLOCKO_TEXTURE_C_INCLUDED
#define BLOCKO_TEXTURE_C_INCLUDED

// The terrain texture array and its runtime updates. All the game's textures
// live in one 16x16 RGBA8 array image: the asset files first, then the player
// model face tiles (72 layers per player slot, filled by pmodel.c).

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

#endif // BLOCKO_TEXTURE_C_INCLUDED

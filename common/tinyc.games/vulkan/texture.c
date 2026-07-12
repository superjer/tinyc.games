#include "main.c"
#ifndef VULKAN_DEMO_TEXTURE_C
#define VULKAN_DEMO_TEXTURE_C

// texture.c - build a mipmapped 2D texture array from a list of image files
// (loaded with stb_image; all images must share the first image's dimensions)

void vulkan_create_texture_array(char **files, int file_count,
        VkImage *image, VkDeviceMemory *memory, VkImageView *view, VkSampler *sampler)
{
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
    vulkan_create_buffer(total_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &staging_buffer, &staging_memory);

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
    vkCreateImage(vk.device, &image_info, NULL, image);

    VkMemoryRequirements mem_reqs;
    vkGetImageMemoryRequirements(vk.device, *image, &mem_reqs);
    VkMemoryAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = mem_reqs.size,
        .memoryTypeIndex = find_memory_type(mem_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
    };
    vkAllocateMemory(vk.device, &alloc_info, NULL, memory);
    vkBindImageMemory(vk.device, *image, *memory, 0);

    // Transition and copy using a one-time command buffer
    VkCommandBuffer cmd = vulkan_begin_commands();

    // Transition all mip levels to TRANSFER_DST
    VkImageMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = *image,
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
    vkCmdCopyBufferToImage(cmd, staging_buffer, *image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, file_count, regions);
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
        vkCmdBlitImage(cmd, *image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            *image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

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

    vulkan_end_commands(cmd);

    // Cleanup staging
    vkDestroyBuffer(vk.device, staging_buffer, NULL);
    vkFreeMemory(vk.device, staging_memory, NULL);

    // Create image view with all mip levels
    VkImageViewCreateInfo view_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = *image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, mip_levels, 0, file_count },
    };
    vkCreateImageView(vk.device, &view_info, NULL, view);

    // Create sampler with mipmap support
    VkSamplerCreateInfo sampler_info = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_NEAREST,
        .minFilter = VK_FILTER_LINEAR,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        // U/V repeat so a uv past 1 tiles the layer (blocko's grass slope
        // stretches its top uv to keep texels square). Every other face stays
        // in 0..1, where repeat and clamp are identical. W is the array layer -
        // keep it clamped so filtering never bleeds into an adjacent tile.
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .minLod = 0.0f,
        .maxLod = (float)mip_levels,
    };
    vkCreateSampler(vk.device, &sampler_info, NULL, sampler);

    fprintf(stderr, "Created texture array: %dx%d, %d layers, %d mip levels\n", tex_w, tex_h, file_count, mip_levels);
}

#endif // VULKAN_DEMO_TEXTURE_C

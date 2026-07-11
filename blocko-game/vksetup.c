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
        vulkan_startup("Blocko", TINYC_DIR "/blocko-game/assets/tinyc-icon.png", TINYC_DIR "/blocko-game/shaders/", 1440, 900);

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
        vulkan_create_texture_array(texture_files, texture_count,
                &texture_image, &texture_memory, &texture_image_view, &texture_sampler);

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

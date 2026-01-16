#include "blocko.c"
#ifndef BLOCKO_GLSETUP_C_INCLUDED
#define BLOCKO_GLSETUP_C_INCLUDED

//SDL_GLContext ctx;
//
//int check_shader_errors(GLuint shader, char *name)
//{
//        GLint success;
//        GLchar log[1024];
//        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
//        if (success) return 0;
//        glGetShaderInfoLog(shader, 1024, NULL, log);
//        fprintf(stderr, "ERROR in %s shader program: %s\n", name, log);
//        exit(1);
//        return 1;
//}
//
//int check_program_errors(GLuint shader, char *name)
//{
//        GLint success;
//        GLchar log[1024];
//        glGetProgramiv(shader, GL_LINK_STATUS, &success);
//        if (success) return 0;
//        glGetProgramInfoLog(shader, 1024, NULL, log);
//        fprintf(stderr, "ERROR in %s shader: %s\n", name, log);
//        exit(1);
//        return 1;
//}

// please free() the returned string
char *file2str(char *filename)
{
        FILE *f;

        #if defined(_MSC_VER) && _MSC_VER >= 1400
                if (fopen_s(&f, filename, "rb"))
                        f = NULL;
        #else
                f = fopen(filename, "r");
        #endif

        if (!f) goto bad;
        fseek(f, 0, SEEK_END);
        size_t sz = ftell(f);
        rewind(f);
        char *buf = calloc(sz + 1, sizeof *buf);
        if (fread(buf, 1, sz, f) != sz) goto bad;
        fclose(f);
        return buf;

        bad:
        fprintf(stderr, __FILE__ " Failed to open/read %s\n", filename);
        return NULL;
}

//unsigned int file2shader(unsigned int type, char *filename)
//{
//        char *code = file2str(filename);
//        unsigned int id = glCreateShader(type);
//        glShaderSource(id, 1, (const char *const *)&code, NULL);
//        glCompileShader(id);
//        check_shader_errors(id, filename);
//        free(code);
//        return id;
//}
//
//void load_shaders()
//{
//        printf("GLSL version on this system is %s\n", (char *)glGetString(GL_SHADING_LANGUAGE_VERSION));
//
//        unsigned int vertex          = file2shader(GL_VERTEX_SHADER,   TINYC_DIR "/blocko-game/shaders/main.vert");
//        unsigned int geometry        = file2shader(GL_GEOMETRY_SHADER, TINYC_DIR "/blocko-game/shaders/main.geom");
//        unsigned int fragment        = file2shader(GL_FRAGMENT_SHADER, TINYC_DIR "/blocko-game/shaders/main.frag");
//        unsigned int shadow_vertex   = file2shader(GL_VERTEX_SHADER,   TINYC_DIR "/blocko-game/shaders/shadow.vert");
//        unsigned int shadow_geometry = file2shader(GL_GEOMETRY_SHADER, TINYC_DIR "/blocko-game/shaders/shadow.geom");
//        unsigned int shadow_fragment = file2shader(GL_FRAGMENT_SHADER, TINYC_DIR "/blocko-game/shaders/shadow.frag");
//
//        prog_id = glCreateProgram();
//        glAttachShader(prog_id, vertex);
//        glAttachShader(prog_id, geometry);
//        glAttachShader(prog_id, fragment);
//        glLinkProgram(prog_id);
//        check_program_errors(prog_id, "main");
//
//        shadow_prog_id = glCreateProgram();
//        glAttachShader(shadow_prog_id, shadow_vertex);
//        glAttachShader(shadow_prog_id, shadow_geometry);
//        glAttachShader(shadow_prog_id, shadow_fragment);
//        glLinkProgram(shadow_prog_id);
//        check_program_errors(shadow_prog_id, "shadow");
//
//        glDeleteShader(vertex);
//        glDeleteShader(geometry);
//        glDeleteShader(fragment);
//        glDeleteShader(shadow_vertex);
//        glDeleteShader(shadow_geometry);
//        glDeleteShader(shadow_fragment);
//}
//
//#ifndef SDL_PLATFORM_APPLE
//void GLAPIENTRY
//MessageCallback(GLenum source, GLenum type, GLuint id, GLenum severity,
//                GLsizei length, const GLchar* message, const void* userParam)
//{
//        if (type != GL_DEBUG_TYPE_ERROR) return; // too much yelling
//        fprintf( stderr, "GL CALLBACK: %s type = 0x%x, severity = 0x%x, message = %s\n",
//                        ( type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : "" ),
//                        type, severity, message );
//        exit(-7);
//}
//#endif

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
    VkDescriptorSetLayoutBinding uboLayoutBinding = {};
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding samplerBinding1 = {0};
    samplerBinding1.binding = 1;
    samplerBinding1.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerBinding1.descriptorCount = 1;
    samplerBinding1.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding samplerBinding2 = {0};
    samplerBinding2.binding = 2;
    samplerBinding2.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerBinding2.descriptorCount = 1;
    samplerBinding2.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding samplerBinding3 = {0};
    samplerBinding3.binding = 3;
    samplerBinding3.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerBinding3.descriptorCount = 1;
    samplerBinding3.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding bindings[] = {uboLayoutBinding, samplerBinding1, samplerBinding2, samplerBinding3};

    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 4;
    layoutInfo.pBindings = bindings;

    vkCreateDescriptorSetLayout(vk.device, &layoutInfo, NULL, descriptorSetLayout);
}

void create_texture_array(char **files, int file_count) {
    // Load first texture to get dimensions
    int tex_w, tex_h, tex_n;
    unsigned char *first = stbi_load(files[0], &tex_w, &tex_h, &tex_n, 4);
    if (!first) { fprintf(stderr, "Failed to load texture %s\n", files[0]); exit(1); }
    stbi_image_free(first);

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

    // Create texture image
    VkImageCreateInfo image_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .extent = { tex_w, tex_h, 1 },
        .mipLevels = 1,
        .arrayLayers = file_count,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
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

    // Transition to TRANSFER_DST
    VkImageMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = texture_image,
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, file_count },
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

    // Transition to SHADER_READ_ONLY
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

    // Create image view
    VkImageViewCreateInfo view_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = texture_image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, file_count },
    };
    vkCreateImageView(vk.device, &view_info, NULL, &texture_image_view);

    // Create sampler
    VkSamplerCreateInfo sampler_info = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_NEAREST,
        .minFilter = VK_FILTER_NEAREST,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
    };
    vkCreateSampler(vk.device, &sampler_info, NULL, &texture_sampler);

    fprintf(stderr, "Created texture array: %dx%d, %d layers\n", tex_w, tex_h, file_count);
}

void create_descriptor_pool_and_set() {
    // Create descriptor pool
    VkDescriptorPoolSize pool_sizes[] = {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3 },
    };
    VkDescriptorPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 1,
        .poolSizeCount = 2,
        .pPoolSizes = pool_sizes,
    };
    vkCreateDescriptorPool(vk.device, &pool_info, NULL, &descriptor_pool);

    // Allocate descriptor set
    VkDescriptorSetAllocateInfo set_alloc = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = descriptor_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &main_descriptor_set_layout,
    };
    vkAllocateDescriptorSets(vk.device, &set_alloc, &main_descriptor_set);

    // Update descriptor set with UBO and texture
    VkDescriptorBufferInfo buffer_info = {
        .buffer = main_buffer,
        .offset = 0,
        .range = sizeof(struct main_ubo),
    };
    VkDescriptorImageInfo image_info = {
        .sampler = texture_sampler,
        .imageView = texture_image_view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    VkWriteDescriptorSet writes[] = {
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = main_descriptor_set,
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = &buffer_info,
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = main_descriptor_set,
            .dstBinding = 1,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &image_info,
        },
    };
    vkUpdateDescriptorSets(vk.device, 2, writes, 0, NULL);
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
        for (int j = 0; j < VAOW; j++)
        {
                mem_use += world_mem_info.allocationSize;
                vkAllocateMemory(vk.device, &world_mem_info, NULL, &world_mem[j]);
                for (int k = 0; k < VAOD; k++)
                {
                        int buffer_idx = j * VAOD + k;
                        int offset = k * world_aligned_sz;
                        vkBindBufferMemory(vk.device, world_buf[buffer_idx], world_mem[j], offset);
                        //fprintf(stderr, "Buffer %d in allocation %d at offset %d / %lu\n", buffer_idx, j, offset, world_mem_info.allocationSize);
                }
        }

        fprintf(stderr, "World VRAM usage: %luMB", mem_use / 1024 / 1024);
}

//initial setup to get the window and rendering going
void glsetup()
{
        vulkan_startup();

        triangle_pipe = vulkan_make_pipeline("shaders/triangle.vert.spv", "shaders/triangle.geom.spv", "shaders/triangle.frag.spv",
                                        0, NULL, 0, NULL);

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
        };
        // Must create descriptor set layout BEFORE using it in pipeline creation
        createDescriptorSetLayout(&main_descriptor_set_layout);
        main_pipe = vulkan_make_pipeline_ex("shaders/main_simple.vert.spv",
                "shaders/main_simple.geom.spv", "shaders/main_simple.frag.spv",
                1, &mainBindingDesc, 6, mainAttrDescs, &main_descriptor_set_layout);

        fprintf(stderr, "struct vbufv layout: size=%zu, tex=%zu, orient=%zu, x=%zu, illum0=%zu, glow0=%zu, alpha=%zu\n",
                sizeof(struct vbufv),
                offsetof(struct vbufv, tex),
                offsetof(struct vbufv, orient),
                offsetof(struct vbufv, x),
                offsetof(struct vbufv, illum0),
                offsetof(struct vbufv, glow0),
                offsetof(struct vbufv, alpha));

        allocate_world();

        createUniformBuffer(&main_buffer, &main_memory);

        //SDL_Init(SDL_INIT_VIDEO);
        //SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
        //SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);
        //win = SDL_CreateWindow("Blocko", W, H, SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN);
        //if (!win) exit(fprintf(stderr, "%s\n", SDL_GetError()));
	//SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        //SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
        //SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        //ctx = SDL_GL_CreateContext(win);
        //if (!ctx) exit(fprintf(stderr, "Could not create GL context\n"));

        //SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        //SDL_GL_SetSwapInterval(vsync);

        SDL_SetWindowRelativeMouseMode(vk.window, true);

        //#ifndef SDL_PLATFORM_APPLE
        //glewExperimental = GL_TRUE;
        //glewInit();
        //glEnable(GL_DEBUG_OUTPUT);
        //glDebugMessageCallback(MessageCallback, 0);
	//#endif

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
        create_descriptor_pool_and_set();
        //glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        //load_shaders();

        //glGenVertexArrays(VAOS, vao);
        //glGenBuffers(VAOS, vbo);
        for (int i = 0; i < VAOS; i++)
        {
                //glBindVertexArray(vao[i]);
                //glBindBuffer(GL_ARRAY_BUFFER, vbo[i]);
                //// tex number
                //glVertexAttribPointer(0, 1, GL_FLOAT, GL_FALSE, sizeof (struct vbufv), (void*)&((struct vbufv *)NULL)->tex);
                //glEnableVertexAttribArray(0);
                //// orientation
                //glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, sizeof (struct vbufv), (void*)&((struct vbufv *)NULL)->orient);
                //glEnableVertexAttribArray(1);
                //// position
                //glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof (struct vbufv), (void*)&((struct vbufv *)NULL)->x);
                //glEnableVertexAttribArray(2);
                //// illum
                //glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof (struct vbufv), (void*)&((struct vbufv *)NULL)->illum0);
                //glEnableVertexAttribArray(3);
                //// glow
                //glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof (struct vbufv), (void*)&((struct vbufv *)NULL)->glow0);
                //glEnableVertexAttribArray(4);
                //// alpha
                //glVertexAttribPointer(5, 1, GL_FLOAT, GL_FALSE, sizeof (struct vbufv), (void*)&((struct vbufv *)NULL)->alpha);
                //glEnableVertexAttribArray(5);
        }

        float border_color[4] = {1.f, 1.f, 1.f, 1.f};

        // create shadow map texture
        //glGenTextures(1, &shadow_tex_id);
        //glBindTexture(GL_TEXTURE_2D, shadow_tex_id);
        //glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32, SHADOW_SZ, SHADOW_SZ,
        //                0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
        //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
        //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
        //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        //glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border_color);
        //glBindTexture(GL_TEXTURE_2D, 0);

        //glGenFramebuffers(1, &shadow_fbo);
        //glBindFramebuffer(GL_FRAMEBUFFER, shadow_fbo);
        //glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadow_tex_id, 0);
        //glDrawBuffer(GL_NONE);
        //glReadBuffer(GL_NONE);
        //glBindFramebuffer(GL_FRAMEBUFFER, 0); // <- even need this?

        //// create shadow map texture ***2***
        //glGenTextures(1, &shadow2_tex_id);
        //glBindTexture(GL_TEXTURE_2D, shadow2_tex_id);
        //glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32, SHADOW_SZ, SHADOW_SZ,
        //                0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
        //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
        //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
        //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        //glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border_color);
        //glBindTexture(GL_TEXTURE_2D, 0);

        //glGenFramebuffers(1, &shadow2_fbo);
        //glBindFramebuffer(GL_FRAMEBUFFER, shadow2_fbo);
        //glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadow2_tex_id, 0);
        //glDrawBuffer(GL_NONE);
        //glReadBuffer(GL_NONE);
        //glBindFramebuffer(GL_FRAMEBUFFER, 0); // <- even need this?
}

#endif // BLOCKO_GLSETUP_C_INCLUDED

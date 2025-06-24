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

        allocate_world();

        createUniformBuffer(&main_buffer, &main_memory);
        createDescriptorSetLayout(&main_descriptor_set_layout);

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

        int x, y, n, mode;
        //glGenTextures(1, &material_tex_id);
        //glBindTexture(GL_TEXTURE_2D_ARRAY, material_tex_id);

        unsigned char *texels;
        char *files[] = {
                TINYC_DIR "/blocko-game/assets/grass_top.png",
                TINYC_DIR "/blocko-game/assets/grass_side.png",
                TINYC_DIR "/blocko-game/assets/dirt.png",
                TINYC_DIR "/blocko-game/assets/grass_grow1_top.png",
                TINYC_DIR "/blocko-game/assets/grass_grow2_top.png",
                TINYC_DIR "/blocko-game/assets/stone.png",
                TINYC_DIR "/blocko-game/assets/sand.png",
                TINYC_DIR "/blocko-game/assets/water.png",      //  7
                TINYC_DIR "/blocko-game/assets/water2.png",
                TINYC_DIR "/blocko-game/assets/water3.png",
                TINYC_DIR "/blocko-game/assets/water4.png",
                TINYC_DIR "/blocko-game/assets/ore.png",        // 11
                TINYC_DIR "/blocko-game/assets/ore_hint.png",   // 12
                TINYC_DIR "/blocko-game/assets/hard.png",       // 13
                TINYC_DIR "/blocko-game/assets/wood_side.png",  // 14
                TINYC_DIR "/blocko-game/assets/granite.png",    // 15
                // transparent:
                TINYC_DIR "/blocko-game/assets/leaves_red.png", // 16
                TINYC_DIR "/blocko-game/assets/leaves_gold.png",// 17
                TINYC_DIR "/blocko-game/assets/mushlite.png",   // 18
                TINYC_DIR "/blocko-game/assets/0.png",          // 19 see #define PNG0 in blocko.h!
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
                TINYC_DIR "/blocko-game/assets/F.png",
                ""
        };

        for (int f = 0; files[f][0]; f++)
        {
                texels = stbi_load(files[f], &x, &y, &n, 0);
                //mode = (n == 4) ? GL_RGBA : GL_RGB;
                //if (mode == GL_RGBA && f <= 17)
                //        for (int i = 0; i < x * y; i++) // remove transparency
                //                texels[i*n + 3] = 0xff;
                //if (f == 0)
                //        glTexStorage3D(GL_TEXTURE_2D_ARRAY, 4, GL_RGBA8, x, y, 256);
                //glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, f, x, y, 1, mode, GL_UNSIGNED_BYTE, texels);
                stbi_image_free(texels);
        }

        //glGenerateMipmap(GL_TEXTURE_2D_ARRAY);

        for (int f = 16; f <= 17; f++) // reload transparent textures now that mipmaps are generated
        {
                texels = stbi_load(files[f], &x, &y, &n, 0);
                //mode = (n == 4) ? GL_RGBA : GL_RGB;
                //glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, f, x, y, 1, mode, GL_UNSIGNED_BYTE, texels);
                stbi_image_free(texels);
        }

        //glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
        //glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        //glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
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

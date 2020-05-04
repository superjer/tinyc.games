#include "blocko.h"

SDL_GLContext ctx;

int check_shader_errors(GLuint shader, char *name)
{
        GLint success;
        GLchar log[1024];
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (success) return 0;
        glGetShaderInfoLog(shader, 1024, NULL, log);
        fprintf(stderr, "ERROR in %s shader program: %s\n", name, log);
        exit(1);
        return 1;
}

int check_program_errors(GLuint shader, char *name)
{
        GLint success;
        GLchar log[1024];
        glGetProgramiv(shader, GL_LINK_STATUS, &success);
        if (success) return 0;
        glGetProgramInfoLog(shader, 1024, NULL, log);
        fprintf(stderr, "ERROR in %s shader: %s\n", name, log);
        exit(1);
        return 1;
}

// please free() the returned string
char *file2str(char *filename)
{
        FILE *f;

        #if defined(_MSC_VER) && _MSC_VER >= 1400
                if (fopen_s(&f, filename, "r"))
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
        fprintf(stderr, "Failed to open/read %s\n", filename);
        return NULL;
}

unsigned int file2shader(unsigned int type, char *filename)
{
        char *code = file2str(filename);
        unsigned int id = glCreateShader(type);
        glShaderSource(id, 1, (const char *const *)&code, NULL);
        glCompileShader(id);
        check_shader_errors(id, filename);
        free(code);
        return id;
}

void load_shaders()
{
        printf("GLSL version on this system is %s\n", (char *)glGetString(GL_SHADING_LANGUAGE_VERSION));

        unsigned int vertex          = file2shader(GL_VERTEX_SHADER,   "shaders/main.vert");
        unsigned int geometry        = file2shader(GL_GEOMETRY_SHADER, "shaders/main.geom");
        unsigned int fragment        = file2shader(GL_FRAGMENT_SHADER, "shaders/main.frag");
        unsigned int shadow_vertex   = file2shader(GL_VERTEX_SHADER,   "shaders/shadow.vert");
        unsigned int shadow_geometry = file2shader(GL_GEOMETRY_SHADER, "shaders/shadow.geom");
        unsigned int shadow_fragment = file2shader(GL_FRAGMENT_SHADER, "shaders/shadow.frag");

        prog_id = glCreateProgram();
        glAttachShader(prog_id, vertex);
        glAttachShader(prog_id, geometry);
        glAttachShader(prog_id, fragment);
        glLinkProgram(prog_id);
        check_program_errors(prog_id, "main");

        shadow_prog_id = glCreateProgram();
        glAttachShader(shadow_prog_id, shadow_vertex);
        glAttachShader(shadow_prog_id, shadow_geometry);
        glAttachShader(shadow_prog_id, shadow_fragment);
        glLinkProgram(shadow_prog_id);
        check_program_errors(shadow_prog_id, "shadow");

        glDeleteShader(vertex);
        glDeleteShader(geometry);
        glDeleteShader(fragment);
        glDeleteShader(shadow_vertex);
        glDeleteShader(shadow_geometry);
        glDeleteShader(shadow_fragment);
}

#ifndef __APPLE__
void GLAPIENTRY
MessageCallback(GLenum source, GLenum type, GLuint id, GLenum severity,
                GLsizei length, const GLchar* message, const void* userParam)
{
        if (type != GL_DEBUG_TYPE_ERROR) return; // too much yelling
        fprintf( stderr, "GL CALLBACK: %s type = 0x%x, severity = 0x%x, message = %s\n",
                        ( type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : "" ),
                        type, severity, message );
        exit(-7);
}
#endif

//initial setup to get the window and rendering going
void glsetup()
{
        SDL_Init(SDL_INIT_VIDEO);
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);
        win = SDL_CreateWindow("Blocko", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                W, H, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL);
        if (!win) exit(fprintf(stderr, "%s\n", SDL_GetError()));
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        ctx = SDL_GL_CreateContext(win);
        if (!ctx) exit(fprintf(stderr, "Could not create GL context\n"));

        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        SDL_GL_SetSwapInterval(vsync);

        SDL_SetRelativeMouseMode(SDL_TRUE);

        #ifndef __APPLE__
        glewExperimental = GL_TRUE;
        glewInit();
        glEnable(GL_DEBUG_OUTPUT);
        glDebugMessageCallback(MessageCallback, 0);
	#endif

        int x, y, n, mode;
        glGenTextures(1, &material_tex_id);
        glBindTexture(GL_TEXTURE_2D_ARRAY, material_tex_id);

        unsigned char *texels;
        char *files[] = {
                "res/grass_top.png",
                "res/grass_side.png",
                "res/dirt.png",
                "res/grass_grow1_top.png",
                "res/grass_grow2_top.png",
                "res/stone.png",
                "res/sand.png",
                "res/water.png",      //  7
                "res/water2.png",
                "res/water3.png",
                "res/water4.png",
                "res/ore.png",        // 11
                "res/ore_hint.png",   // 12
                "res/hard.png",       // 13
                "res/wood_side.png",  // 14
                "res/granite.png",    // 15
                // transparent:
                "res/leaves_red.png", // 16
                "res/leaves_gold.png",// 17
                "res/mushlite.png",   // 18
                "res/0.png",          // 19 see #define PNG0 in blocko.h!
                "res/1.png",
                "res/2.png",
                "res/3.png",
                "res/4.png",
                "res/5.png",
                "res/6.png",
                "res/7.png",
                "res/8.png",
                "res/9.png",
                "res/A.png",
                "res/B.png",
                "res/C.png",
                "res/D.png",
                "res/E.png",
                "res/F.png",
                ""
        };

        for (int f = 0; files[f][0]; f++)
        {
                texels = stbi_load(files[f], &x, &y, &n, 0);
                mode = (n == 4) ? GL_RGBA : GL_RGB;
                if (mode == GL_RGBA && f <= 17)
                        for (int i = 0; i < x * y; i++) // remove transparency
                                texels[i*n + 3] = 0xff;
                if (f == 0)
                        glTexStorage3D(GL_TEXTURE_2D_ARRAY, 4, GL_RGBA8, x, y, 256);
                glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, f, x, y, 1, mode, GL_UNSIGNED_BYTE, texels);
                stbi_image_free(texels);
        }

        glGenerateMipmap(GL_TEXTURE_2D_ARRAY);

        for (int f = 16; f <= 17; f++) // reload transparent textures now that mipmaps are generated
        {
                texels = stbi_load(files[f], &x, &y, &n, 0);
                mode = (n == 4) ? GL_RGBA : GL_RGB;
                glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, f, x, y, 1, mode, GL_UNSIGNED_BYTE, texels);
                stbi_image_free(texels);
        }

        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        load_shaders();

        glGenVertexArrays(VAOS, vao);
        glGenBuffers(VAOS, vbo);
        for (int i = 0; i < VAOS; i++)
        {
                glBindVertexArray(vao[i]);
                glBindBuffer(GL_ARRAY_BUFFER, vbo[i]);
                // tex number
                glVertexAttribPointer(0, 1, GL_FLOAT, GL_FALSE, sizeof (struct vbufv), (void*)&((struct vbufv *)NULL)->tex);
                glEnableVertexAttribArray(0);
                // orientation
                glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, sizeof (struct vbufv), (void*)&((struct vbufv *)NULL)->orient);
                glEnableVertexAttribArray(1);
                // position
                glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof (struct vbufv), (void*)&((struct vbufv *)NULL)->x);
                glEnableVertexAttribArray(2);
                // illum
                glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof (struct vbufv), (void*)&((struct vbufv *)NULL)->illum0);
                glEnableVertexAttribArray(3);
                // glow
                glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof (struct vbufv), (void*)&((struct vbufv *)NULL)->glow0);
                glEnableVertexAttribArray(4);
                // alpha
                glVertexAttribPointer(5, 1, GL_FLOAT, GL_FALSE, sizeof (struct vbufv), (void*)&((struct vbufv *)NULL)->alpha);
                glEnableVertexAttribArray(5);
        }

        // create shadow map texture
        glGenTextures(1, &shadow_tex_id);
        glBindTexture(GL_TEXTURE_2D, shadow_tex_id);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_SZ, SHADOW_SZ,
                        0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        float border_color[4] = {1.f, 1.f, 1.f, 1.f};
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border_color);
        glBindTexture(GL_TEXTURE_2D, 0);

        glGenFramebuffers(1, &shadow_fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, shadow_fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadow_tex_id, 0);
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
        glBindFramebuffer(GL_FRAMEBUFFER, 0); // <- even need this?
}

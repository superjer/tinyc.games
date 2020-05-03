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

void load_shaders()
{
        const char *vertex_code = "\
#version 330 core                                                               \n\
layout (location = 0) in float tex_in;                                          \n\
layout (location = 1) in float orient_in;                                       \n\
layout (location = 2) in vec3 pos_in;                                           \n\
layout (location = 3) in vec4 illum_in;                                         \n\
layout (location = 4) in vec4 glow_in;                                          \n\
layout (location = 5) in float alpha_in;                                        \n\
                                                                                \n\
out float tex_vs;                                                               \n\
out float orient_vs;                                                            \n\
out vec4 illum_vs;                                                              \n\
out vec4 glow_vs;                                                               \n\
out float alpha_vs;                                                             \n\
out vec4 world_pos_vs;                                                          \n\
                                                                                \n\
uniform mat4 model;                                                             \n\
uniform mat4 view;                                                              \n\
uniform mat4 proj;                                                              \n\
uniform float BS;                                                               \n\
                                                                                \n\
void main(void)                                                                 \n\
{                                                                               \n\
    vec3 pos = BS * pos_in;                                                     \n\
    gl_Position = proj * view * model * vec4(pos, 1);                           \n\
    world_pos_vs = model * vec4(pos, 1);                                        \n\
    tex_vs = tex_in;                                                            \n\
    orient_vs = orient_in;                                                      \n\
    illum_vs = illum_in;                                                        \n\
    glow_vs = glow_in;                                                          \n\
    alpha_vs = alpha_in;                                                        \n\
}                                                                               \n\
";

        const char *geometry_code = "\
#version 330 core                                                               \n\
layout (points) in;                                                             \n\
layout (triangle_strip, max_vertices = 4) out;                                  \n\
                                                                                \n\
in float tex_vs[];                                                              \n\
in float orient_vs[];                                                           \n\
in vec4 illum_vs[];                                                             \n\
in vec4 glow_vs[];                                                              \n\
in float alpha_vs[];                                                            \n\
in vec4 world_pos_vs[];                                                         \n\
                                                                                \n\
flat out float tex;                                                             \n\
out float illum;                                                                \n\
out float glow;                                                                 \n\
flat out float alpha;                                                           \n\
out vec2 uv;                                                                    \n\
flat out float eyedist;                                                         \n\
out vec4 shadow_pos;                                                            \n\
out vec4 world_pos;                                                             \n\
flat out vec3 normal;                                                           \n\
                                                                                \n\
uniform mat4 model;                                                             \n\
uniform mat4 view;                                                              \n\
uniform mat4 proj;                                                              \n\
uniform mat4 shadow_space;                                                      \n\
uniform float BS;                                                               \n\
                                                                                \n\
void main(void) // geometry                                                     \n\
{                                                                               \n\
    float sidel = 0.0f;                                                         \n\
    vec4 a, b, c, d;                                                            \n\
    mat4 mvp = proj * view * model;                                             \n\
    switch(int(orient_vs[0])) {                                                 \n\
        case 1: // UP                                                           \n\
            a = vec4( 0, 0, 0,0);                                               \n\
            b = vec4(BS, 0, 0,0);                                               \n\
            c = vec4( 0, 0,BS,0);                                               \n\
            d = vec4(BS, 0,BS,0);                                               \n\
            normal = vec3(0, -1, 0);                                            \n\
            sidel = 1.0f;                                                       \n\
            break;                                                              \n\
        case 2: // EAST                                                         \n\
            a = vec4(BS, 0,BS,0);                                               \n\
            b = vec4(BS, 0, 0,0);                                               \n\
            c = vec4(BS,BS,BS,0);                                               \n\
            d = vec4(BS,BS, 0,0);                                               \n\
            normal = vec3(1, 0, 0);                                             \n\
            sidel = 0.9f;                                                       \n\
            break;                                                              \n\
        case 3: // NORTH                                                        \n\
            a = vec4( 0, 0,BS,0);                                               \n\
            b = vec4(BS, 0,BS,0);                                               \n\
            c = vec4( 0,BS,BS,0);                                               \n\
            d = vec4(BS,BS,BS,0);                                               \n\
            normal = vec3(0, 0, 1);                                             \n\
            sidel = 0.8f;                                                       \n\
            break;                                                              \n\
        case 4: // WEST                                                         \n\
            a = vec4( 0, 0, 0,0);                                               \n\
            b = vec4( 0, 0,BS,0);                                               \n\
            c = vec4( 0,BS, 0,0);                                               \n\
            d = vec4( 0,BS,BS,0);                                               \n\
            normal = vec3(-1, 0, 0);                                            \n\
            sidel = 0.9f;                                                       \n\
            break;                                                              \n\
        case 5: // SOUTH                                                        \n\
            a = vec4(BS, 0, 0,0);                                               \n\
            b = vec4( 0, 0, 0,0);                                               \n\
            c = vec4(BS,BS, 0,0);                                               \n\
            d = vec4( 0,BS, 0,0);                                               \n\
            normal = vec3(0, 0, -1);                                            \n\
            sidel = 0.8f;                                                       \n\
            break;                                                              \n\
        case 6: // DOWN                                                         \n\
            a = vec4(BS,BS, 0,0);                                               \n\
            b = vec4( 0,BS, 0,0);                                               \n\
            c = vec4(BS,BS,BS,0);                                               \n\
            d = vec4( 0,BS,BS,0);                                               \n\
            normal = vec3(0, 1, 0);                                             \n\
            sidel = 0.6f;                                                       \n\
            break;                                                              \n\
    }                                                                           \n\
                                                                                \n\
    tex = tex_vs[0];                                                            \n\
    alpha = alpha_vs[0];                                                        \n\
    eyedist = length(gl_in[0].gl_Position);                                     \n\
                                                                                \n\
    gl_Position = gl_in[0].gl_Position + mvp * a;                               \n\
    world_pos = world_pos_vs[0] + a;                                            \n\
    shadow_pos = shadow_space * world_pos;                                      \n\
    uv = vec2(1,0);                                                             \n\
    illum = (0.1 + illum_vs[0].x) * sidel;                                      \n\
    glow = (0.1 + glow_vs[0].x) * sidel;                                        \n\
    EmitVertex();                                                               \n\
                                                                                \n\
    gl_Position = gl_in[0].gl_Position + mvp * b;                               \n\
    world_pos = world_pos_vs[0] + b;                                            \n\
    shadow_pos = shadow_space * world_pos;                                      \n\
    uv = vec2(0,0);                                                             \n\
    illum = (0.1 + illum_vs[0].y) * sidel;                                      \n\
    glow = (0.1 + glow_vs[0].y) * sidel;                                        \n\
    EmitVertex();                                                               \n\
                                                                                \n\
    gl_Position = gl_in[0].gl_Position + mvp * c;                               \n\
    world_pos = world_pos_vs[0] + c;                                            \n\
    shadow_pos = shadow_space * world_pos;                                      \n\
    uv = vec2(1,1);                                                             \n\
    illum = (0.1 + illum_vs[0].z) * sidel;                                      \n\
    glow = (0.1 + glow_vs[0].z) * sidel;                                        \n\
    EmitVertex();                                                               \n\
                                                                                \n\
    gl_Position = gl_in[0].gl_Position + mvp * d;                               \n\
    world_pos = world_pos_vs[0] + d;                                            \n\
    shadow_pos = shadow_space * world_pos;                                      \n\
    uv = vec2(0,1);                                                             \n\
    illum = (0.1 + illum_vs[0].w) * sidel;                                      \n\
    glow = (0.1 + glow_vs[0].w) * sidel;                                        \n\
    EmitVertex();                                                               \n\
                                                                                \n\
    EndPrimitive();                                                             \n\
}                                                                               \n\
";

        const char *fragment_code = "\
#version 330 core                                                               \n\
out vec4 color;                                                                 \n\
                                                                                \n\
flat in float tex;                                                              \n\
in float illum;                                                                 \n\
in float glow;                                                                  \n\
flat in float alpha;                                                            \n\
in vec2 uv;                                                                     \n\
flat in float eyedist;                                                          \n\
in vec4 shadow_pos;                                                             \n\
in vec4 world_pos;                                                              \n\
flat in vec3 normal;                                                            \n\
                                                                                \n\
uniform sampler2DArray tarray;                                                  \n\
uniform sampler2DShadow shadow_map;                                             \n\
uniform vec3 day_color;                                                         \n\
uniform vec3 glo_color;                                                         \n\
uniform vec3 fog_color;                                                         \n\
uniform vec3 light_pos;                                                         \n\
uniform vec3 view_pos;                                                          \n\
uniform float sharpness;                                                        \n\
                                                                                \n\
void main(void)                                                                 \n\
{                                                                               \n\
    float fog = smoothstep(10000, 100000, eyedist);                             \n\
    float il = illum + 0.1 * smoothstep(1000, 0, eyedist);                      \n\
                                                                                \n\
    vec3 light_dir = normalize(light_pos - world_pos.xyz);                      \n\
    float diff = max(dot(light_dir, normal), 0.0);                              \n\
    vec3 view_dir = normalize(view_pos - world_pos.xyz);                        \n\
    vec3 halfway_dir = normalize(light_dir + view_dir);                         \n\
    float spec = pow(max(dot(normal, halfway_dir), 0), 16);                     \n\
                                                                                \n\
    vec4 s = vec4(shadow_pos.xyz, 1);                                           \n\
    float unshadow = textureProj(shadow_map, s);                                \n\
                                                                                \n\
    float s0 = 0.6 + 0.4 * sharpness;                                           \n\
    float s1 = 0.3 + 0.7 * (1-sharpness);                                       \n\
    vec3 sky = vec3(s1 * il + s0 * unshadow * (diff + spec)) * day_color;       \n\
                                                                                \n\
    vec3 glo = vec3(glow * glo_color);                                          \n\
    vec3 unsky = vec3(1 - sky.r, 1 - sky.g, 1 - sky.b);                         \n\
    vec4 combined = vec4(sky + glo * unsky, alpha);                             \n\
    vec4 c = texture(tarray, vec3(uv, tex)) * combined;                         \n\
    if (c.a < 0.01) discard;                                                    \n\
    color = mix(c, vec4(fog_color, 1), fog);                                    \n\
}                                                                               \n\
";

        const char *shadow_vertex_code = "\
#version 330 core                                                               \n\
layout (location = 0) in float tex_in;                                          \n\
layout (location = 1) in float orient_in;                                       \n\
layout (location = 2) in vec3 pos_in;                                           \n\
layout (location = 3) in vec4 illum_in;                                         \n\
layout (location = 4) in vec4 glow_in;                                          \n\
layout (location = 5) in float alpha_in;                                        \n\
                                                                                \n\
out float tex_vs;                                                               \n\
out float orient_vs;                                                            \n\
                                                                                \n\
uniform mat4 model;                                                             \n\
uniform mat4 view;                                                              \n\
uniform mat4 proj;                                                              \n\
uniform float BS;                                                               \n\
                                                                                \n\
void main(void)                                                                 \n\
{                                                                               \n\
    vec3 pos = BS * pos_in;                                                     \n\
    gl_Position = proj * view * model * vec4(pos, 1.0f);                        \n\
    tex_vs = tex_in;                                                            \n\
    orient_vs = orient_in;                                                      \n\
}                                                                               \n\
";

        const char *shadow_geometry_code = "\
#version 330 core                                                               \n\
layout (points) in;                                                             \n\
layout (triangle_strip, max_vertices = 4) out;                                  \n\
                                                                                \n\
in float tex_vs[];                                                              \n\
in float orient_vs[];                                                           \n\
                                                                                \n\
flat out float tex;                                                             \n\
out vec2 uv;                                                                    \n\
                                                                                \n\
uniform mat4 model;                                                             \n\
uniform mat4 view;                                                              \n\
uniform mat4 proj;                                                              \n\
uniform float BS;                                                               \n\
                                                                                \n\
void main(void) // geometry                                                     \n\
{                                                                               \n\
    vec4 a, b, c, d;                                                            \n\
    mat4 mvp = proj * view * model;                                             \n\
    switch(int(orient_vs[0])) {                                                 \n\
        case 1: // UP                                                           \n\
            b = vec4(BS, 0, 0,0);                                               \n\
            a = vec4( 0, 0, 0,0);                                               \n\
            c = vec4( 0, 0,BS,0);                                               \n\
            d = vec4(BS, 0,BS,0);                                               \n\
            break;                                                              \n\
        case 2: // EAST                                                         \n\
            a = vec4(BS, 0,BS,0);                                               \n\
            b = vec4(BS, 0, 0,0);                                               \n\
            c = vec4(BS,BS,BS,0);                                               \n\
            d = vec4(BS,BS, 0,0);                                               \n\
            break;                                                              \n\
        case 3: // NORTH                                                        \n\
            a = vec4( 0, 0,BS,0);                                               \n\
            b = vec4(BS, 0,BS,0);                                               \n\
            c = vec4( 0,BS,BS,0);                                               \n\
            d = vec4(BS,BS,BS,0);                                               \n\
            break;                                                              \n\
        case 4: // WEST                                                         \n\
            a = vec4( 0, 0, 0,0);                                               \n\
            b = vec4( 0, 0,BS,0);                                               \n\
            c = vec4( 0,BS, 0,0);                                               \n\
            d = vec4( 0,BS,BS,0);                                               \n\
            break;                                                              \n\
        case 5: // SOUTH                                                        \n\
            a = vec4(BS, 0, 0,0);                                               \n\
            b = vec4( 0, 0, 0,0);                                               \n\
            c = vec4(BS,BS, 0,0);                                               \n\
            d = vec4( 0,BS, 0,0);                                               \n\
            break;                                                              \n\
        case 6: // DOWN                                                         \n\
            a = vec4(BS,BS, 0,0);                                               \n\
            b = vec4( 0,BS, 0,0);                                               \n\
            c = vec4(BS,BS,BS,0);                                               \n\
            d = vec4( 0,BS,BS,0);                                               \n\
            break;                                                              \n\
    }                                                                           \n\
                                                                                \n\
    tex = tex_vs[0];                                                            \n\
                                                                                \n\
    gl_Position = gl_in[0].gl_Position + mvp * a;                               \n\
    uv = vec2(1,0);                                                             \n\
    EmitVertex();                                                               \n\
    gl_Position = gl_in[0].gl_Position + mvp * b;                               \n\
    uv = vec2(0,0);                                                             \n\
    EmitVertex();                                                               \n\
    gl_Position = gl_in[0].gl_Position + mvp * c;                               \n\
    uv = vec2(1,1);                                                             \n\
    EmitVertex();                                                               \n\
    gl_Position = gl_in[0].gl_Position + mvp * d;                               \n\
    uv = vec2(0,1);                                                             \n\
    EmitVertex();                                                               \n\
    EndPrimitive();                                                             \n\
}                                                                               \n\
";

        const char *shadow_fragment_code= "\
#version 330 core                                                               \n\
flat in float tex;                                                              \n\
in vec2 uv;                                                                     \n\
                                                                                \n\
out vec4 color;                                                                 \n\
                                                                                \n\
uniform sampler2DArray tarray;                                                  \n\
                                                                                \n\
void main(void)                                                                 \n\
{                                                                               \n\
    color = texture(tarray, vec3(uv, tex));                                     \n\
    if (color.a < 0.51) discard;                                                \n\
}                                                                               \n\
";

        printf("GLSL version on this system is %s\n", (char *)glGetString(GL_SHADING_LANGUAGE_VERSION));

        unsigned int vertex = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertex, 1, (const char *const *)&vertex_code, NULL);
        glCompileShader(vertex);
        check_shader_errors(vertex, "main vertex");

        unsigned int shadow_vertex = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(shadow_vertex, 1, (const char *const *)&shadow_vertex_code, NULL);
        glCompileShader(shadow_vertex);
        check_shader_errors(shadow_vertex, "shadow vertex");

        unsigned int geometry = glCreateShader(GL_GEOMETRY_SHADER);
        glShaderSource(geometry, 1, (const char *const *)&geometry_code, NULL);
        glCompileShader(geometry);
        check_shader_errors(geometry, "main geometry");

        unsigned int shadow_geometry = glCreateShader(GL_GEOMETRY_SHADER);
        glShaderSource(shadow_geometry, 1, (const char *const *)&shadow_geometry_code, NULL);
        glCompileShader(shadow_geometry);
        check_shader_errors(shadow_geometry, "shadow geometry");

        unsigned int fragment = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragment, 1, (const char *const *)&fragment_code, NULL);
        glCompileShader(fragment);
        check_shader_errors(fragment, "main fragment");

        unsigned int shadow_fragment = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(shadow_fragment, 1, (const char *const *)&shadow_fragment_code, NULL);
        glCompileShader(shadow_fragment);
        check_shader_errors(shadow_fragment, "shadow fragment");

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

#ifdef __APPLE__
#include <OpenGL/gl3.h>
#else
#include <GL/glew.h>
#endif

#include <stdio.h>
#include <stdlib.h>

unsigned int prog_id;
unsigned int shadow_prog_id;

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
out float tex;                                                                  \n\
out float illum;                                                                \n\
out float glow;                                                                 \n\
out float alpha;                                                                \n\
out vec2 uv;                                                                    \n\
out float eyedist;                                                              \n\
out vec4 shadow_pos;                                                            \n\
out vec4 world_pos;                                                             \n\
out vec3 normal;                                                                \n\
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
    world_pos = world_pos_vs[0] + model * a;                                    \n\
    shadow_pos = shadow_space * world_pos;                                      \n\
    uv = vec2(1,0);                                                             \n\
    illum = (0.1 + illum_vs[0].x) * sidel;                                      \n\
    glow = (0.1 + glow_vs[0].x) * sidel;                                        \n\
    EmitVertex();                                                               \n\
                                                                                \n\
    gl_Position = gl_in[0].gl_Position + mvp * b;                               \n\
    world_pos = world_pos_vs[0] + model * b;                                    \n\
    shadow_pos = shadow_space * world_pos;                                      \n\
    uv = vec2(0,0);                                                             \n\
    illum = (0.1 + illum_vs[0].y) * sidel;                                      \n\
    glow = (0.1 + glow_vs[0].y) * sidel;                                        \n\
    EmitVertex();                                                               \n\
                                                                                \n\
    gl_Position = gl_in[0].gl_Position + mvp * c;                               \n\
    world_pos = world_pos_vs[0] + model * c;                                    \n\
    shadow_pos = shadow_space * world_pos;                                      \n\
    uv = vec2(1,1);                                                             \n\
    illum = (0.1 + illum_vs[0].z) * sidel;                                      \n\
    glow = (0.1 + glow_vs[0].z) * sidel;                                        \n\
    EmitVertex();                                                               \n\
                                                                                \n\
    gl_Position = gl_in[0].gl_Position + mvp * d;                               \n\
    world_pos = world_pos_vs[0] + model * d;                                    \n\
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
in float tex;                                                                   \n\
in float illum;                                                                 \n\
in float glow;                                                                  \n\
in float alpha;                                                                 \n\
in vec2 uv;                                                                     \n\
in float eyedist;                                                               \n\
in vec4 shadow_pos;                                                             \n\
in vec4 world_pos;                                                              \n\
in vec3 normal;                                                                 \n\
                                                                                \n\
uniform sampler2DArray tarray;                                                  \n\
uniform sampler2D shadow_map;                                                   \n\
uniform vec3 day_color;                                                         \n\
uniform vec3 glo_color;                                                         \n\
uniform vec3 fog_color;                                                         \n\
uniform vec3 light_pos;                                                         \n\
uniform vec3 view_pos;                                                          \n\
                                                                                \n\
float get_shadow(vec4 v)                                                        \n\
{                                                                               \n\
    vec3 p = v.xyz;// / v.w;                                                    \n\
    p = p * 0.5 + 0.5;                                                          \n\
    float closest = texture(shadow_map, v.xy).r;                                \n\
    return v.z >= closest ? 0 : 1;                                              \n\
}                                                                               \n\
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
    //float unshadow = textureProj(shadow_map, shadow_pos);                     \n\
    float unshadow = get_shadow(shadow_pos);                                    \n\
                                                                                \n\
    vec3 sky = vec3(il * 0.3 + unshadow * (diff + spec)) * day_color;           \n\
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
    gl_Position = gl_in[0].gl_Position + mvp * a;                               \n\
    EmitVertex();                                                               \n\
    gl_Position = gl_in[0].gl_Position + mvp * b;                               \n\
    EmitVertex();                                                               \n\
    gl_Position = gl_in[0].gl_Position + mvp * c;                               \n\
    EmitVertex();                                                               \n\
    gl_Position = gl_in[0].gl_Position + mvp * d;                               \n\
    EmitVertex();                                                               \n\
    EndPrimitive();                                                             \n\
}                                                                               \n\
";

        const char *shadow_fragment_code= "\
#version 330 core                                                               \n\
out vec4 color;                                                                 \n\
                                                                                \n\
void main(void)                                                                 \n\
{                                                                               \n\
    color = vec4(1.0);                                                          \n\
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

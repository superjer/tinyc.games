#ifdef __APPLE__
#include <OpenGL/gl3.h>
#else
#include <GL/glew.h>
#endif

#include <stdio.h>
#include <stdlib.h>

unsigned int prog_id;

static void check_compile_errors(GLuint shader, char type);

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
                                                                                \n\
out float tex;                                                                  \n\
out float illum;                                                                \n\
out float glow;                                                                 \n\
out float alpha;                                                                \n\
out vec2 uv;                                                                    \n\
out float eyedist;                                                              \n\
                                                                                \n\
uniform mat4 model;                                                             \n\
uniform mat4 view;                                                              \n\
uniform mat4 proj;                                                              \n\
uniform float BS;                                                               \n\
                                                                                \n\
void main(void) // geometry                                                     \n\
{                                                                               \n\
    float sidel = 0.0f;                                                         \n\
    vec4 a, b, c, d;                                                            \n\
    mat4 mvp = proj * view * model;                                             \n\
    switch(int(orient_vs[0])) {                                                 \n\
        case 1: // UP                                                           \n\
            a = mvp * vec4( 0, 0, 0,0);                                         \n\
            b = mvp * vec4(BS, 0, 0,0);                                         \n\
            c = mvp * vec4( 0, 0,BS,0);                                         \n\
            d = mvp * vec4(BS, 0,BS,0);                                         \n\
            sidel = 1.0f;                                                       \n\
            break;                                                              \n\
        case 2: // EAST                                                         \n\
            a = mvp * vec4(BS, 0,BS,0);                                         \n\
            b = mvp * vec4(BS, 0, 0,0);                                         \n\
            c = mvp * vec4(BS,BS,BS,0);                                         \n\
            d = mvp * vec4(BS,BS, 0,0);                                         \n\
            sidel = 0.9f;                                                       \n\
            break;                                                              \n\
        case 3: // NORTH                                                        \n\
            a = mvp * vec4( 0, 0,BS,0);                                         \n\
            b = mvp * vec4(BS, 0,BS,0);                                         \n\
            c = mvp * vec4( 0,BS,BS,0);                                         \n\
            d = mvp * vec4(BS,BS,BS,0);                                         \n\
            sidel = 0.8f;                                                       \n\
            break;                                                              \n\
        case 4: // WEST                                                         \n\
            a = mvp * vec4( 0, 0, 0,0);                                         \n\
            b = mvp * vec4( 0, 0,BS,0);                                         \n\
            c = mvp * vec4( 0,BS, 0,0);                                         \n\
            d = mvp * vec4( 0,BS,BS,0);                                         \n\
            sidel = 0.9f;                                                       \n\
            break;                                                              \n\
        case 5: // SOUTH                                                        \n\
            a = mvp * vec4(BS, 0, 0,0);                                         \n\
            b = mvp * vec4( 0, 0, 0,0);                                         \n\
            c = mvp * vec4(BS,BS, 0,0);                                         \n\
            d = mvp * vec4( 0,BS, 0,0);                                         \n\
            sidel = 0.8f;                                                       \n\
            break;                                                              \n\
        case 6: // DOWN                                                         \n\
            a = mvp * vec4(BS,BS, 0,0);                                         \n\
            b = mvp * vec4( 0,BS, 0,0);                                         \n\
            c = mvp * vec4(BS,BS,BS,0);                                         \n\
            d = mvp * vec4( 0,BS,BS,0);                                         \n\
            sidel = 0.6f;                                                       \n\
            break;                                                              \n\
    }                                                                           \n\
                                                                                \n\
    tex = tex_vs[0];                                                            \n\
    alpha = alpha_vs[0];                                                        \n\
    eyedist = length(gl_in[0].gl_Position);                                     \n\
                                                                                \n\
    gl_Position = gl_in[0].gl_Position + a;                                     \n\
    uv = vec2(1,0);                                                             \n\
    illum = (0.1 + illum_vs[0].x) * sidel;                                      \n\
    glow = (0.1 + glow_vs[0].x) * sidel;                                        \n\
    EmitVertex();                                                               \n\
                                                                                \n\
    gl_Position = gl_in[0].gl_Position + b;                                     \n\
    uv = vec2(0,0);                                                             \n\
    illum = (0.1 + illum_vs[0].y) * sidel;                                      \n\
    glow = (0.1 + glow_vs[0].y) * sidel;                                        \n\
    EmitVertex();                                                               \n\
                                                                                \n\
    gl_Position = gl_in[0].gl_Position + c;                                     \n\
    uv = vec2(1,1);                                                             \n\
    illum = (0.1 + illum_vs[0].z) * sidel;                                      \n\
    glow = (0.1 + glow_vs[0].z) * sidel;                                        \n\
    EmitVertex();                                                               \n\
                                                                                \n\
    gl_Position = gl_in[0].gl_Position + d;                                     \n\
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
                                                                                \n\
uniform sampler2DArray tarray;                                                  \n\
uniform vec3 day_color;                                                         \n\
uniform vec3 glo_color;                                                         \n\
uniform vec3 fog_color;                                                         \n\
                                                                                \n\
void main(void)                                                                 \n\
{                                                                               \n\
    float fog = smoothstep(10000, 100000, eyedist);                             \n\
    float il = illum + 0.1 * smoothstep(1000, 0, eyedist);                      \n\
                                                                                \n\
    vec3 sky = vec3(il * day_color);                                            \n\
    vec3 glo = vec3(glow * glo_color);                                          \n\
    vec3 unsky = vec3(1 - sky.r, 1 - sky.g, 1 - sky.b);                         \n\
    vec4 combined = vec4(sky + glo * unsky, alpha);                             \n\
    vec4 c = texture(tarray, vec3(uv, tex)) * combined;                         \n\
    if (c.a < 0.01) discard;                                                    \n\
    color = mix(c, vec4(fog_color, 1), fog);                                    \n\
}                                                                               \n\
";

        printf("GLSL version on this system is %s\n", (char *)glGetString(GL_SHADING_LANGUAGE_VERSION));

	unsigned int vertex, fragment, geometry;
	// vertex
	vertex = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertex, 1, (const char *const *)&vertex_code, NULL);
	glCompileShader(vertex);
	check_compile_errors(vertex, 'V');
	// geometry
	geometry = glCreateShader(GL_GEOMETRY_SHADER);
	glShaderSource(geometry, 1, (const char *const *)&geometry_code, NULL);
	glCompileShader(geometry);
	check_compile_errors(geometry, 'G');
	// fragment
	fragment = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragment, 1, (const char *const *)&fragment_code, NULL);
	glCompileShader(fragment);
	check_compile_errors(fragment, 'F');
	// program
	prog_id = glCreateProgram();
	glAttachShader(prog_id, vertex);
	glAttachShader(prog_id, geometry);
	glAttachShader(prog_id, fragment);
	glLinkProgram(prog_id);
	check_compile_errors(prog_id, 'P');
	glDeleteShader(vertex);
	glDeleteShader(geometry);
	glDeleteShader(fragment);
}

void check_compile_errors(GLuint shader, char type)
{
	GLint success;
	GLchar log[1024];
	if (type != 'P')
	{
		glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
		if (!success)
		{
			glGetShaderInfoLog(shader, 1024, NULL, log);
			fprintf(stderr, "ERROR type %c\n%s\n", type, log);
                        exit(-1);
		}
	}
	else
	{
		glGetProgramiv(shader, GL_LINK_STATUS, &success);
		if (!success)
		{
			glGetProgramInfoLog(shader, 1024, NULL, log);
			fprintf(stderr, "ERROR type %c\n%s\n", type, log);
                        exit(-1);
		}
	}
}

#ifdef __APPLE__
#include <gl.h>
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
                                                                                \n\
out float tex_vs;                                                               \n\
out float orient_vs;                                                            \n\
out vec4 illum_vs;                                                              \n\
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
                                                                                \n\
out float tex;                                                                  \n\
out float illum;                                                                \n\
out vec2 uv;                                                                    \n\
out float fog;                                                                  \n\
                                                                                \n\
uniform mat4 model;                                                             \n\
uniform mat4 view;                                                              \n\
uniform mat4 proj;                                                              \n\
uniform float BS;                                                               \n\
                                                                                \n\
void main(void) // geometry                                                     \n\
{                                                                               \n\
    float BQ = BS + 0.01f; // overdraw the edges a bit                          \n\
    float Q = -0.01f;                                                           \n\
    float sidel = 0.0f;                                                         \n\
    vec4 a, b, c, d;                                                            \n\
    switch(int(orient_vs[0])) {                                                 \n\
        case 1: // UP                                                           \n\
            a = proj * view * model * vec4( Q, 0, Q,0);                         \n\
            b = proj * view * model * vec4(BQ, 0, Q,0);                         \n\
            c = proj * view * model * vec4( Q, 0,BQ,0);                         \n\
            d = proj * view * model * vec4(BQ, 0,BQ,0);                         \n\
            sidel = 1.0f;                                                       \n\
            break;                                                              \n\
        case 2: // EAST                                                         \n\
            a = proj * view * model * vec4(BS, Q,BQ,0);                         \n\
            b = proj * view * model * vec4(BS, Q, Q,0);                         \n\
            c = proj * view * model * vec4(BS,BQ,BQ,0);                         \n\
            d = proj * view * model * vec4(BS,BQ, Q,0);                         \n\
            sidel = 0.9f;                                                       \n\
            break;                                                              \n\
        case 3: // NORTH                                                        \n\
            a = proj * view * model * vec4( Q, Q,BS,0);                         \n\
            b = proj * view * model * vec4(BQ, Q,BS,0);                         \n\
            c = proj * view * model * vec4( Q,BQ,BS,0);                         \n\
            d = proj * view * model * vec4(BQ,BQ,BS,0);                         \n\
            sidel = 0.8f;                                                       \n\
            break;                                                              \n\
        case 4: // WEST                                                         \n\
            a = proj * view * model * vec4( 0, Q, Q,0);                         \n\
            b = proj * view * model * vec4( 0, Q,BQ,0);                         \n\
            c = proj * view * model * vec4( 0,BQ, Q,0);                         \n\
            d = proj * view * model * vec4( 0,BQ,BQ,0);                         \n\
            sidel = 0.9f;                                                       \n\
            break;                                                              \n\
        case 5: // SOUTH                                                        \n\
            a = proj * view * model * vec4(BQ, Q, 0,0);                         \n\
            b = proj * view * model * vec4( Q, Q, 0,0);                         \n\
            c = proj * view * model * vec4(BQ,BQ, 0,0);                         \n\
            d = proj * view * model * vec4( Q,BQ, 0,0);                         \n\
            sidel = 0.8f;                                                       \n\
            break;                                                              \n\
        case 6: // DOWN                                                         \n\
            a = proj * view * model * vec4(BQ,BS, Q,0);                         \n\
            b = proj * view * model * vec4( Q,BS, Q,0);                         \n\
            c = proj * view * model * vec4(BQ,BS,BQ,0);                         \n\
            d = proj * view * model * vec4( Q,BS,BQ,0);                         \n\
            sidel = 0.6f;                                                       \n\
            break;                                                              \n\
    }                                                                           \n\
                                                                                \n\
    tex = tex_vs[0];                                                            \n\
                                                                                \n\
    vec4 eye = vec4(0, 0, 0, 1);                                                \n\
    float dist = distance(eye, gl_in[0].gl_Position);                           \n\
    if (dist >= 100000) fog = 1;                                                \n\
    else if (dist <=  10000) fog = 0;                                           \n\
    else fog = 1 - (100000 - dist) / (100000 - 10000);                          \n\
                                                                                \n\
    gl_Position = gl_in[0].gl_Position + a;                                     \n\
    illum = (0.1 + illum_vs[0].x) * sidel;                                      \n\
    uv = vec2(1,0);                                                             \n\
    EmitVertex();                                                               \n\
                                                                                \n\
    gl_Position = gl_in[0].gl_Position + b;                                     \n\
    uv = vec2(0,0);                                                             \n\
    illum = (0.1 + illum_vs[0].y) * sidel;                                      \n\
    EmitVertex();                                                               \n\
                                                                                \n\
    gl_Position = gl_in[0].gl_Position + c;                                     \n\
    uv = vec2(1,1);                                                             \n\
    illum = (0.1 + illum_vs[0].z) * sidel;                                      \n\
    EmitVertex();                                                               \n\
                                                                                \n\
    gl_Position = gl_in[0].gl_Position + d;                                     \n\
    uv = vec2(0,1);                                                             \n\
    illum = (0.1 + illum_vs[0].w) * sidel;                                      \n\
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
in vec2 uv;                                                                     \n\
in float fog;                                                                   \n\
                                                                                \n\
uniform sampler2DArray tarray;                                                  \n\
uniform vec4 eye;                                                               \n\
                                                                                \n\
void main(void)                                                                 \n\
{                                                                               \n\
    color = mix(                                                                \n\
            texture(tarray, vec3(uv, tex)) * vec4(illum, illum, illum, 0),      \n\
            vec4(0.31, 0.91, 1.01, 0),                                          \n\
            fog                                                                 \n\
    );                                                                          \n\
}                                                                               \n\
";

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

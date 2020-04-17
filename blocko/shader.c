#ifdef __APPLE__
#include <gl.h>
#else
#include <GL/glew.h>
#endif

#include <stdio.h>
#include <stdlib.h>

unsigned int ID;

static void check_compile_errors(GLuint shader, char type);

void load_shaders()
{
	const char *vertex_code = "\
#version 330 core                                                               \n\
layout (location = 0) in float tex_in;                                          \n\
layout (location = 1) in float orient_in;                                       \n\
layout (location = 2) in vec3 pos_in;                                           \n\
layout (location = 3) in float illum_in;                                        \n\
                                                                                \n\
out float tex_vs;                                                               \n\
out float orient_vs;                                                            \n\
out float illum_vs;                                                             \n\
                                                                                \n\
uniform mat4 model;                                                             \n\
uniform mat4 view;                                                              \n\
uniform mat4 proj;                                                              \n\
uniform float BS;                                                               \n\
                                                                                \n\
void main()                                                                     \n\
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
in float illum_vs[];                                                            \n\
                                                                                \n\
out float tex;                                                                  \n\
out float illum;                                                                \n\
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
    switch(int(orient_vs[0])) {                                                 \n\
        case 1: // UP                                                           \n\
            a = proj * view * model * vec4( 0, 0, 0,0);                         \n\
            b = proj * view * model * vec4(BS, 0, 0,0);                         \n\
            c = proj * view * model * vec4( 0, 0,BS,0);                         \n\
            d = proj * view * model * vec4(BS, 0,BS,0);                         \n\
            break;                                                              \n\
        case 2: // EAST                                                         \n\
            a = proj * view * model * vec4(BS, 0,BS,0);                         \n\
            b = proj * view * model * vec4(BS, 0, 0,0);                         \n\
            c = proj * view * model * vec4(BS,BS,BS,0);                         \n\
            d = proj * view * model * vec4(BS,BS, 0,0);                         \n\
            break;                                                              \n\
        case 3: // NORTH                                                        \n\
            a = proj * view * model * vec4( 0, 0,BS,0);                         \n\
            b = proj * view * model * vec4(BS, 0,BS,0);                         \n\
            c = proj * view * model * vec4( 0,BS,BS,0);                         \n\
            d = proj * view * model * vec4(BS,BS,BS,0);                         \n\
            break;                                                              \n\
        case 4: // WEST                                                         \n\
            a = proj * view * model * vec4( 0, 0, 0,0);                         \n\
            b = proj * view * model * vec4( 0, 0,BS,0);                         \n\
            c = proj * view * model * vec4( 0,BS, 0,0);                         \n\
            d = proj * view * model * vec4( 0,BS,BS,0);                         \n\
            break;                                                              \n\
        case 5: // SOUTH                                                        \n\
            a = proj * view * model * vec4(BS, 0, 0,0);                         \n\
            b = proj * view * model * vec4( 0, 0, 0,0);                         \n\
            c = proj * view * model * vec4(BS,BS, 0,0);                         \n\
            d = proj * view * model * vec4( 0,BS, 0,0);                         \n\
            break;                                                              \n\
        case 6: // DOWN                                                         \n\
            a = proj * view * model * vec4(BS,BS, 0,0);                         \n\
            b = proj * view * model * vec4( 0,BS, 0,0);                         \n\
            c = proj * view * model * vec4(BS,BS,BS,0);                         \n\
            d = proj * view * model * vec4( 0,BS,BS,0);                         \n\
            break;                                                              \n\
    }                                                                           \n\
                                                                                \n\
    tex = tex_vs[0];                                                            \n\
    illum = illum_vs[0];                                                        \n\
                                                                                \n\
    gl_Position = gl_in[0].gl_Position + a;                                     \n\
    uv = vec2(1,0);                                                             \n\
    EmitVertex();                                                               \n\
                                                                                \n\
    gl_Position = gl_in[0].gl_Position + b;                                     \n\
    uv = vec2(0,0);                                                             \n\
    EmitVertex();                                                               \n\
                                                                                \n\
    gl_Position = gl_in[0].gl_Position + c;                                     \n\
    uv = vec2(1,1);                                                             \n\
    EmitVertex();                                                               \n\
                                                                                \n\
    gl_Position = gl_in[0].gl_Position + d;                                     \n\
    uv = vec2(0,1);                                                             \n\
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
                                                                                \n\
uniform sampler2DArray tarray;                                                  \n\
                                                                                \n\
void main()                                                                     \n\
{                                                                               \n\
    color = texture(tarray, vec3(uv, tex)) * vec4(illum, illum, illum, 0);      \n\
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
	ID = glCreateProgram();
	glAttachShader(ID, vertex);
	glAttachShader(ID, geometry);
	glAttachShader(ID, fragment);
	glLinkProgram(ID);
	check_compile_errors(ID, 'P');
	glDeleteShader(vertex);
	glDeleteShader(geometry);
	glDeleteShader(fragment);
}

/* uniform examples
	set bool
        glUniform1i(glGetUniformLocation(ID, name.c_str()), (int)value);
	set int
        glUniform1i(glGetUniformLocation(ID, name.c_str()), value);
	set float
        glUniform1f(glGetUniformLocation(ID, name.c_str()), value);
	set vec2
        glUniform2fv(glGetUniformLocation(ID, name.c_str()), 1, &value[0]);
        glUniform2f(glGetUniformLocation(ID, name.c_str()), x, y);
	set vec3
        glUniform3fv(glGetUniformLocation(ID, name.c_str()), 1, &value[0]);
        glUniform3f(glGetUniformLocation(ID, name.c_str()), x, y, z);
	set vec4
        glUniform4fv(glGetUniformLocation(ID, name.c_str()), 1, &value[0]);
        glUniform4f(glGetUniformLocation(ID, name.c_str()), x, y, z, w);
	set mat2
        glUniformMatrix2fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE, &mat[0][0]);
	set mat3
        glUniformMatrix3fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE, &mat[0][0]);
	set mat4
        glUniformMatrix4fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE, &mat[0][0]);
*/

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

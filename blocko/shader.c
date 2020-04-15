#ifdef __APPLE__
#include <gl.h>
#else
#include <GL/glew.h>
#endif

#include <stdio.h>

unsigned int ID;

static void check_compile_errors(GLuint shader, char type);

void load_shaders()
{
	const char *vertex_code = "\
#version 330 core\n\
layout (location = 0) in vec3 aPos;\n\
layout (location = 1) in float aTMix;\n\
layout (location = 2) in vec2 aTexCoord;\n\
layout (location = 3) in vec3 aColor;\n\
\n\
out vec2 TexCoord;\n\
flat out int TMix;\n\
out vec3 Color;\n\
\n\
uniform mat4 model;\n\
uniform mat4 view;\n\
uniform mat4 projection;\n\
\n\
void main()\n\
{\n\
    gl_Position = projection * view * model * vec4(aPos, 1.0f);\n\
    TexCoord = vec2(aTexCoord.x, 1.0 - aTexCoord.y);\n\
    TMix = int(aTMix);\n\
    Color = aColor;\n\
}\n\
";

	const char *fragment_code = "\
#version 330 core\n\
out vec4 FragColor;\n\
\n\
in vec2 TexCoord;\n\
flat in int TMix;\n\
in vec3 Color;\n\
\n\
uniform sampler2D texture1;\n\
uniform sampler2D texture2;\n\
uniform sampler2D texture3;\n\
\n\
void main()\n\
{\n\
    if (TMix == 1)\n\
        FragColor = texture(texture1, TexCoord) * vec4(Color, 0);\n\
    else if(TMix == 2)\n\
        FragColor = texture(texture2, TexCoord) * vec4(Color, 0);\n\
    else if(TMix == 3)\n\
        FragColor = texture(texture3, TexCoord) * vec4(Color, 0);\n\
}\n\
";

	const char *geometry_code = "\
#version 330 core\n\
layout (triangles) in;\n\
layout (triangle_strip, max_vertices = 3) out;\n\
\n\
void main(void)\n\
{\n\
    int n;\n\
    for (n = 0; n < gl_in.length(); n++) {\n\
        gl_Position = gl_in[0].gl_Position;\n\
        EmitVertex();\n\
    }\n\
    EndPrimitive();\n\
}\n\
";

	unsigned int vertex, fragment, geometry;
	// vertex
	vertex = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertex, 1, (const char *const *)&vertex_code, NULL);
	glCompileShader(vertex);
	check_compile_errors(vertex, 'V');
	// fragment
	fragment = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragment, 1, (const char *const *)&fragment_code, NULL);
	glCompileShader(fragment);
	check_compile_errors(fragment, 'F');
	// geometry
	geometry = glCreateShader(GL_GEOMETRY_SHADER);
	glShaderSource(geometry, 1, (const char *const *)&geometry_code, NULL);
	glCompileShader(geometry);
	check_compile_errors(geometry, 'G');
	// program
	ID = glCreateProgram();
	glAttachShader(ID, vertex);
	glAttachShader(ID, fragment);
	//glAttachShader(ID, geometry); // skip geometry shader for now (conflict with QUADS?)
	glLinkProgram(ID);
	check_compile_errors(ID, 'P');
	glDeleteShader(vertex);
	glDeleteShader(fragment);
	glDeleteShader(geometry);
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
		}
	}
	else
	{
		glGetProgramiv(shader, GL_LINK_STATUS, &success);
		if (!success)
		{
			glGetProgramInfoLog(shader, 1024, NULL, log);
			fprintf(stderr, "ERROR type %c\n%s\n", type, log);
		}
	}
}

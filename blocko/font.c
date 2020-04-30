#ifdef __APPLE__
#include <OpenGL/gl3.h>
#else
#include <GL/glew.h>
#endif

#include <math.h>

#define FONT_CH_W 8
#define FONT_CH_H 12
#define FONT_PITCH 128
#define FONT_LINES 128
#define FONT_BUFLEN 16000

int check_shader_errors(unsigned int, char *);
int check_program_errors(unsigned int, char *);

GLuint font_tex_id;
unsigned int font_prog_id;
GLuint font_vbo, font_vao;

int font_screenw;
int font_screenh;

float font_buf[FONT_BUFLEN + 100];
float *font_buf_limit = font_buf + FONT_BUFLEN;
float *font_buf_p = font_buf;

int font_spacing[256] = { 6,3,5,7,7,6,7,3,5,5,6,7,3,6,3,6,
                          6,6,6,6,6,6,6,6,6,6,3,3,5,6,5,7,
                          7,6,6,6,6,6,6,6,6,5,6,6,6,7,7,6,
                          6,6,6,6,7,6,7,7,7,7,6,5,6,5,7,7,
                          4,6,6,6,6,6,5,6,6,4,5,6,4,7,6,6,
                          6,6,5,6,5,6,7,7,7,6,6,5,3,5,6,6,
                          6,7,9,0,0,0,0,0,0,0,0,0,0,0,0,0,
                          0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
                          0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
                          0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };

void font_init()
{
        char font_data[] =
"                                                                                                                                "
"                                                                                                                                "
"         O       O O               O                     O         O     O                                                  O   "
"         O       O O      O O     OOO             OO     O        O       O                O                                O   "
"         O               OOOOO   O O             O               O         O      O O      O                               O    "
"         O                O O     OOO    O  O     O              O         O       O     OOOOO           OOOO              O    "
"         O                O O      O O     O     O O O           O         O      O O      O                              O     "
"                         OOOOO    OOO     O      O  O             O       O                O                              O     "
"         O                O O      O     O  O     OO O             O     O                       O               O       O      "
"                                                                                                 O                       O      "
"                                                                                                                                "
"                                                                                                                                "
"                                                                                                                                "
"                                                                                                                                "
"                                                                                                                          OOO   "
"  OO       O      OO      OO     O       OOOO     OO     OOOO     OO      OO                       O             O       O   O  "
" O  O     OO     O  O    O  O    O  O    O       O          O    O  O    O  O                     O      OOOO     O          O  "
" O OO      O        O      O     O  O    OOO     OOO       O      OO     O  O    O       O       O                 O        O   "
" OO O      O      OO        O    OOOO       O    O  O      O     O  O     OOO                     O      OOOO     O        O    "
" O  O      O     O       O  O       O    O  O    O  O     O      O  O       O                      O             O              "
"  OO      OOO    OOOO     OO        O     OO      OO      O       OO      OO     O       O                                 O    "
"                                                                                         O                                      "
"                                                                                                                                "
"                                                                                                                                "
"                                                                                                                                "
"                                                                                                                                "
"  OOO     OO     OOO      OO     OOO     OOOO    OOOO     OO     O  O    OOO        O    O  O    O       O   O   O   O    OO    "
" O   O   O  O    O  O    O  O    O  O    O       O       O  O    O  O     O         O    O  O    O       OO OO   OO  O   O  O   "
" O OOO   O  O    O  O    O       O  O    O       O       O       O  O     O         O    O O     O       O O O   O O O   O  O   "
" O O O   OOOO    OOO     O       O  O    OOO     OOO     O OO    OOOO     O         O    OO      O       O O O   O  OO   O  O   "
" O OOO   O  O    O  O    O       O  O    O       O       O  O    O  O     O         O    O O     O       O   O   O   O   O  O   "
" O       O  O    O  O    O  O    O  O    O       O       O  O    O  O     O      O  O    O  O    O       O   O   O   O   O  O   "
"  OOO    O  O    OOO      OO     OOO     OOOO    O        OOO    O  O    OOO      OO     O  O    OOOO    O   O   O   O    OO    "
"                                                                                                                                "
"                                                                                                                                "
"                                                                                                                                "
"                                                                                                                                "
"                                                                                                                                "
" OOO      OO     OOO      OOO    OOOOO   O  O    O   O   O   O   O   O   O   O   OOOO    OOO     O       OOO       O            "
" O  O    O  O    O  O    O         O     O  O    O   O   O   O   O   O   O   O      O    O       O         O      O O           "
" O  O    O  O    O  O    O         O     O  O    O   O   O   O    O O    O   O     O     O        O        O     O   O          "
" OOO     O  O    OOO      OO       O     O  O    O   O   O O O     O      OOO     O      O        O        O                    "
" O       O  O    O  O       O      O     O  O     O O    O O O    O O      O     O       O         O       O                    "
" O       O  O    O  O       O      O     O  O     O O    O O O   O   O     O     O       O         O       O                    "
" O        OO     O  O    OOO       O      OO       O      O O    O   O     O     OOOO    OOO        O    OOO                    "
"            O                                                                                       O                    OOOOO  "
"                                                                                                                                "
"                                                                                                                                "
"                                                                                                                                "
"                                                                                                                                "
" O               O                  O              O             O        O        O     O       OO                             "
"  O              O                  O             O              O                       O        O                             "
"          OO     OOO      OOO     OOO     OO     OOO      OOO    OOO     OO       OO     O  O     O      OOOO    OOO      OO    "
"            O    O  O    O       O  O    O  O     O      O  O    O  O     O        O     O O      O      O O O   O  O    O  O   "
"          OOO    O  O    O       O  O    OOOO     O      O  O    O  O     O        O     OO       O      O O O   O  O    O  O   "
"         O  O    O  O    O       O  O    O        O      O  O    O  O     O        O     O O      O      O O O   O  O    O  O   "
"          OOO    OOO      OOO     OOO     OOO     O       OOO    O  O     O        O     O  O     O      O O O   O  O     OO    "
"                                                            O                      O                                            "
"                                                          OO                     OO                                             "
"                                                                                                                                "
"                                                                                                                                "
"                                                                                                                                "
"                                                                                           O     O       O        O O           "
"                                  O                                                       O      O        O      O O            "
" OOO      OOO    O O      OOO    OOO     O  O    O   O   O O O   O   O   O  O    OOOO     O      O        O                     "
" O  O    O  O    OO      O        O      O  O    O   O   O O O    O O    O  O       O    O       O         O                    "
" O  O    O  O    O        OO      O      O  O     O O    O O O     O     O  O     OO      O               O                     "
" O  O    O  O    O          O     O      O  O     O O    O O O    O O    O  O    O        O      O        O                     "
" OOO      OOO    O       OOO      OO      OOO      O      OOOO   O   O    OOO    OOOO      O     O       O                      "
" O          O                                                               O                    O                              "
" O          O                                                             OO                     O                              "
"                                                                                                                                "
"                                                                                                                                "
"           O        OOO                                                                                                         "
"           OO       OOOO         OOOO    OOOO    OOOO    OOOO    OOOO    OOOO    OOOO    OOOO    OOOO    OOOO    OOOO    OOOO   "
"         O  O        OOO         O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O   "
" OOOO    OO      OOO             O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O   "
" OOOO     O O    OOOO OOO        O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O   "
" OOOO       OO    OOO OOOO       O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O   "
" OOOO     O  O         OOO       O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O   "
"          OO       OOO           OOOO    OOOO    OOOO    OOOO    OOOO    OOOO    OOOO    OOOO    OOOO    OOOO    OOOO    OOOO   "
"           O       OOOO                                                                                                         "
"                    OOO                                                                                                         "
"                                                                                                                                "
"                                                                                                                                "
"                                                                                                                                "
" OOOO    OOOO    OOOO    OOOO    OOOO    OOOO    OOOO    OOOO    OOOO    OOOO    OOOO    OOOO    OOOO    OOOO    OOOO    OOOO   "
" O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O   "
" O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O   "
" O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O   "
" O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O   "
" O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O   "
" OOOO    OOOO    OOOO    OOOO    OOOO    OOOO    OOOO    OOOO    OOOO    OOOO    OOOO    OOOO    OOOO    OOOO    OOOO    OOOO   "
"                                                                                                                                "
"                                                                                                                                "
"                                                                                                                                "
"                                                                                                                                "
"                                                                                                                                "
" OOOO    OOOO    OOOO    OOOO    OOOO    OOOO    OOOO    OOOO    OOOO    OOOO    OOOO    OOOO    OOOO    OOOO    OOOO    OOOO   "
" O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O   "
" O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O   "
" O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O   "
" O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O   "
" O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O   "
" OOOO    OOOO    OOOO    OOOO    OOOO    OOOO    OOOO    OOOO    OOOO    OOOO    OOOO    OOOO    OOOO    OOOO    OOOO    OOOO   "
"                                                                                                                                "
"                                                                                                                                "
"                                                                                                                                "
"                                                                                                                                "
"                                                                                                                                "
" OOOO    OOOO    OOOO    OOOO    OOOO    OOOO    OOOO    OOOO    OOOO    OOOO    OOOO    OOOO    OOOO    OOOO    OOOO    OOOO   "
" O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O   "
" O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O   "
" O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O   "
" O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O   "
" O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O    O  O   "
" OOOO    OOOO    OOOO    OOOO    OOOO    OOOO    OOOO    OOOO    OOOO    OOOO    OOOO    OOOO    OOOO    OOOO    OOOO    OOOO   "
"                                                                                                                                "
"                                                                                                                                "
"                                                                                                                                "
"                                                                                                                                "
"                                                                                                                                "
"                                                                                                                                "
"                                                                                                                                "
"                                                                                                                                "
"                                                                                                                                "
"                                                                                                                                "
"                                                                                                                                ";
        const char *vertex_code = "\
#version 330 core                                                               \n\
layout (location = 0) in vec4 pos;                                              \n\
out vec2 uv;                                                                    \n\
                                                                                \n\
uniform mat4 proj;                                                              \n\
                                                                                \n\
void main()                                                                     \n\
{                                                                               \n\
    gl_Position = proj * vec4(pos.xy, 0.0, 1.0);                                \n\
    uv = pos.zw;                                                                \n\
}                                                                               \n\
";

        const char *fragment_code = "\
#version 330 core                                                               \n\
in vec2 uv;                                                                     \n\
out vec4 color;                                                                 \n\
                                                                                \n\
uniform vec3 incolor;                                                           \n\
uniform sampler2D tex;                                                          \n\
                                                                                \n\
void main()                                                                     \n\
{                                                                               \n\
    color = vec4(incolor, texture(tex, uv).r);                                  \n\
}                                                                               \n\
";

        unsigned char raw_data[FONT_PITCH * FONT_LINES];

        for (int i = 0; i < FONT_PITCH * FONT_LINES; i++)
                raw_data[i] = (font_data[i] == ' ') ? 0 : 0xFF;

        glGenTextures(1, &font_tex_id);
        glBindTexture(GL_TEXTURE_2D, font_tex_id);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, FONT_PITCH, FONT_LINES, 0, GL_RED, GL_UNSIGNED_BYTE, raw_data);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        unsigned int vertex, fragment;
        vertex = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertex, 1, (const char *const *)&vertex_code, NULL);
        glCompileShader(vertex);
        check_shader_errors(vertex, "font vertex");

        fragment = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragment, 1, (const char *const *)&fragment_code, NULL);
        glCompileShader(fragment);
        check_shader_errors(fragment, "font fragment");

        font_prog_id = glCreateProgram();
        glAttachShader(font_prog_id, vertex);
        glAttachShader(font_prog_id, fragment);
        glLinkProgram(font_prog_id);
        check_program_errors(font_prog_id, "font");
        glDeleteShader(vertex);
        glDeleteShader(fragment);

        glGenVertexArrays(1, &font_vao);
        glGenBuffers(1, &font_vbo);
}

void font_begin(int w, int h)
{
        font_screenw = w;
        font_screenh = h;
        font_buf_p = font_buf;
}

void font_add_text(char *s, int inx, int iny)
{
        int x = inx;
        int y = iny;
        float scale = roundf((font_screenw < font_screenh ? font_screenw : font_screenh) / 250.f);

        for (; *s && font_buf_p < font_buf_limit; s++)
        {
                if (*s == '\n')
                {
                        x = inx;
                        y += FONT_CH_H * scale;
                        continue;
                }

                int c = *s - ' ';
                float u = (c * FONT_CH_W) % FONT_PITCH;
                float v = (c / (FONT_PITCH / FONT_CH_W)) * FONT_CH_H;

                if (c) // don't render spaces
                {
                        *font_buf_p++ = x;
                        *font_buf_p++ = y;
                        *font_buf_p++ = u / FONT_PITCH;
                        *font_buf_p++ = v / FONT_LINES;

                        *font_buf_p++ = x + font_spacing[c] * scale;
                        *font_buf_p++ = y;
                        *font_buf_p++ = (u + font_spacing[c]) / FONT_PITCH;
                        *font_buf_p++ = v / FONT_LINES;

                        *font_buf_p++ = x;
                        *font_buf_p++ = y + FONT_CH_H * scale;
                        *font_buf_p++ = u / FONT_PITCH;
                        *font_buf_p++ = (v + FONT_CH_H) / FONT_LINES;

                        *font_buf_p++ = x + font_spacing[c] * scale;
                        *font_buf_p++ = y;
                        *font_buf_p++ = (u + font_spacing[c]) / FONT_PITCH;
                        *font_buf_p++ = v / FONT_LINES;

                        *font_buf_p++ = x;
                        *font_buf_p++ = y + FONT_CH_H * scale;
                        *font_buf_p++ = u / FONT_PITCH;
                        *font_buf_p++ = (v + FONT_CH_H) / FONT_LINES;

                        *font_buf_p++ = x + font_spacing[c] * scale;
                        *font_buf_p++ = y + FONT_CH_H * scale;
                        *font_buf_p++ = (u + font_spacing[c]) / FONT_PITCH;
                        *font_buf_p++ = (v + FONT_CH_H) / FONT_LINES;
                }

                x += font_spacing[c] * scale;
        }
}

void font_end(float r, float g, float b)
{
        int n = (font_buf_p - font_buf);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glUseProgram(font_prog_id);

        float near = -100.f;
        float far = 100.f;
        float x = 1.f / (font_screenw / 2.f);
        float y = -1.f / (font_screenh / 2.f);
        float z = -1.f / ((far - near) / 2.f);
        float tz = -(far + near) / (far - near);
        float ortho[] = {
                x, 0, 0,  0,
                0, y, 0,  0,
                0, 0, z,  0,
               -1, 1, tz, 1,
        };
        glUniformMatrix4fv(glGetUniformLocation(font_prog_id, "proj"), 1, GL_FALSE, ortho);

        glBindTexture(GL_TEXTURE_2D, font_tex_id);

        glBindVertexArray(font_vao);
        glBindBuffer(GL_ARRAY_BUFFER, font_vbo);
        glBufferData(GL_ARRAY_BUFFER, n * sizeof *font_buf, font_buf, GL_STATIC_DRAW);

        glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), 0);
        glEnableVertexAttribArray(0);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glUniform3f(glGetUniformLocation(font_prog_id, "incolor"), 0, 0, 0);
        glDrawArrays(GL_TRIANGLES, 0, n);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glUniform3f(glGetUniformLocation(font_prog_id, "incolor"), r, g, b);
        glDrawArrays(GL_TRIANGLES, 0, n);
}

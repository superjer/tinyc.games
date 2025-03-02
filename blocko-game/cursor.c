#include "blocko.c"
#ifndef BLOCKO_CURSOR_C_INCLUDED
#define BLOCKO_CURSOR_C_INCLUDED

unsigned int cursor_prog_id;
unsigned int cursor_vbo, cursor_vao;

int cursor_screenw;
int cursor_screenh;

float cursor_buf[100];
float *cursor_buf_limit = cursor_buf + 100;
float *cursor_buf_p = cursor_buf;

void cursor_init()
{
        //unsigned int vertex = file2shader(GL_VERTEX_SHADER, TINYC_DIR "/blocko-game/shaders/cursor.vert");
        //unsigned int fragment = file2shader(GL_FRAGMENT_SHADER, TINYC_DIR "/blocko-game/shaders/cursor.frag");

        //cursor_prog_id = glCreateProgram();
        //glAttachShader(cursor_prog_id, vertex);
        //glAttachShader(cursor_prog_id, fragment);
        //glLinkProgram(cursor_prog_id);
        //check_program_errors(cursor_prog_id, "cursor");
        //glDeleteShader(vertex);
        //glDeleteShader(fragment);

        //glGenVertexArrays(1, &cursor_vao);
        //glGenBuffers(1, &cursor_vbo);
}

void cursor_rect(int x0, int y0, int x1, int y1)
{
        *cursor_buf_p++ = x0;
        *cursor_buf_p++ = y0;
        *cursor_buf_p++ = x1;
        *cursor_buf_p++ = y1;
        *cursor_buf_p++ = x1;
        *cursor_buf_p++ = y0;

        *cursor_buf_p++ = x1;
        *cursor_buf_p++ = y1;
        *cursor_buf_p++ = x0;
        *cursor_buf_p++ = y1;
        *cursor_buf_p++ = x0;
        *cursor_buf_p++ = y0;
}

void cursor(int w, int h)
{
        cursor_screenw = w;
        cursor_screenh = h;
        int w2 = w/2;
        int h2 = h/2;
        int n;

        //glDisable(GL_BLEND);
        //glDisable(GL_DEPTH_TEST);
        //glDisable(GL_CULL_FACE);
        //glUseProgram(cursor_prog_id);

        float near = -100.f;
        float far = 100.f;
        float x = 1.f / (cursor_screenw / 2.f);
        float y = -1.f / (cursor_screenh / 2.f);
        float z = -1.f / ((far - near) / 2.f);
        float tz = -(far + near) / (far - near);
        float ortho[] = {
                x, 0, 0,  0,
                0, y, 0,  0,
                0, 0, z,  0,
               -1, 1, tz, 1,
        };
        //glUniformMatrix4fv(glGetUniformLocation(cursor_prog_id, "proj"), 1, GL_FALSE, ortho);

        cursor_buf_p = cursor_buf;

        cursor_rect(w2 - 25 + 1, h2 -  1 + 1, w2 -  9 + 1, h2 +  1 + 1);
        cursor_rect(w2 +  9 + 1, h2 -  1 + 1, w2 + 25 + 1, h2 +  1 + 1);
        cursor_rect(w2 -  1 + 1, h2 - 25 + 1, w2 +  1 + 1, h2 -  9 + 1);
        cursor_rect(w2 -  1 + 1, h2 +  9 + 1, w2 +  1 + 1, h2 + 25 + 1);

        n = cursor_buf_p - cursor_buf;

        //glBindVertexArray(cursor_vao);
        //glBindBuffer(GL_ARRAY_BUFFER, cursor_vbo);
        //glBufferData(GL_ARRAY_BUFFER, n * sizeof *cursor_buf, cursor_buf, GL_STATIC_DRAW);

        //glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), 0);
        //glEnableVertexAttribArray(0);

        //glUniform3f(glGetUniformLocation(cursor_prog_id, "incolor"), .1f, .1f, .1f);
        //glDrawArrays(GL_TRIANGLES, 0, n);

        cursor_buf_p = cursor_buf;

        cursor_rect(w2 - 25, h2 -  1, w2 -  9, h2 +  1);
        cursor_rect(w2 +  9, h2 -  1, w2 + 25, h2 +  1);
        cursor_rect(w2 -  1, h2 - 25, w2 +  1, h2 -  9);
        cursor_rect(w2 -  1, h2 +  9, w2 +  1, h2 + 25);

        n = cursor_buf_p - cursor_buf;

        //glBindVertexArray(cursor_vao);
        //glBindBuffer(GL_ARRAY_BUFFER, cursor_vbo);
        //glBufferData(GL_ARRAY_BUFFER, n * sizeof *cursor_buf, cursor_buf, GL_STATIC_DRAW);

        //glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), 0);
        //glEnableVertexAttribArray(0);

        //glUniform3f(glGetUniformLocation(cursor_prog_id, "incolor"), .9f, .9f, .9f);
        //glDrawArrays(GL_TRIANGLES, 0, n);
}

#endif // BLOCKO_CURSOR_C_INCLUDED

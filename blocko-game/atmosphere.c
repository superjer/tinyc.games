#include "blocko.h"

unsigned int sun_prog_id;
GLuint sun_vbo, sun_vao;

void sun_init()
{
        unsigned int vertex = file2shader(GL_VERTEX_SHADER, "shaders/sun.vert");
        unsigned int fragment = file2shader(GL_FRAGMENT_SHADER, "shaders/sun.frag");

        sun_prog_id = glCreateProgram();
        glAttachShader(sun_prog_id, vertex);
        glAttachShader(sun_prog_id, fragment);
        glLinkProgram(sun_prog_id);
        check_program_errors(sun_prog_id, "sun");
        glDeleteShader(vertex);
        glDeleteShader(fragment);

        float sun_buf[] = {
                // sun
                10000, -1000, -1000, 1, 0, // A
                10000,  1000, -1000, 1, 1, // B
                10000, -1000,  1000, 0, 0, // D
                10000,  1000,  1000, 0, 1, // C
                // moon
                -10000, -400, -400, 1, 0, // A
                -10000,  400, -400, 1, 1, // B
                -10000, -400,  400, 0, 0, // D
                -10000,  400,  400, 0, 1, // C
        };

        glGenVertexArrays(1, &sun_vao);
        glGenBuffers(1, &sun_vbo);
        glBindVertexArray(sun_vao);
        glBindBuffer(GL_ARRAY_BUFFER, sun_vbo);

        glBufferData(GL_ARRAY_BUFFER, sizeof sun_buf, sun_buf, GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), 0);
        glEnableVertexAttribArray(0);

        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
}

void sun_draw(float *proj, float *view, float sun_pitch, unsigned int texid)
{
        float a = sun_pitch;
        float model[] = {
                cosf(a), -sinf(a), 0, 0,
                sinf(a),  cosf(a), 0, 0,
                      0,       0, 1, 0,
                      0,       0, 0, 1,
        };

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_CULL_FACE);
        glDepthFunc(GL_ALWAYS);
        glDepthMask(GL_FALSE);
        glUseProgram(sun_prog_id);

        glUniformMatrix4fv(glGetUniformLocation(sun_prog_id, "proj"), 1, GL_FALSE, proj);
        glUniformMatrix4fv(glGetUniformLocation(sun_prog_id, "view"), 1, GL_FALSE, view);
        glUniformMatrix4fv(glGetUniformLocation(sun_prog_id, "model"), 1, GL_FALSE, model);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, texid);
        glUniform1i(glGetUniformLocation(sun_prog_id, "tex"), show_shadow_map ? 1 : 3);

        glBindVertexArray(sun_vao);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glDrawArrays(GL_TRIANGLE_STRIP, 4, 4);
}

#include "blocko.h"

unsigned int sun_prog_id;
GLuint sun_vbo, sun_vao;

void do_atmos_colors()
{
        if (sun_pitch < PI) // in the day, linearly change the sky color
        {
                night_amt = fmodf(sun_pitch + 3*PI2, TAU) / TAU;
                if (night_amt > 0.5f) night_amt = 1.f - night_amt;
                night_amt *= 2.f;
        }
        else // at night change via cubic-sine so that it's mostly dark all night
        {
                night_amt = 1.f + sinf(sun_pitch);  //  0 to  1
                night_amt *= night_amt * night_amt; //  0 to  1
                night_amt *= -0.5f;                 //-.5 to  0
                night_amt += 1.f;                   //  1 to .5
        }

        if (night_amt > 0.5f)
        {
                fog_r = lerp(2.f*(night_amt - 0.5f), FOG_DUSK_R, FOG_NIGHT_R);
                fog_g = lerp(2.f*(night_amt - 0.5f), FOG_DUSK_G, FOG_NIGHT_G);
                fog_b = lerp(2.f*(night_amt - 0.5f), FOG_DUSK_B, FOG_NIGHT_B);
        }
        else
        {
                fog_r = lerp(2.f*night_amt, FOG_DAY_R, FOG_DUSK_R);
                fog_g = lerp(2.f*night_amt, FOG_DAY_G, FOG_DUSK_G);
                fog_b = lerp(2.f*night_amt, FOG_DAY_B, FOG_DUSK_B);
        }
}

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

void sun_draw(float *proj, float *view, float pitch, float yaw, float roll, unsigned int texid)
{
        float a = pitch;
        float b = yaw;
        float model[] = {
                -cosf(a) * sinf(b), -sinf(a), -cosf(a) * cosf(b), 0,
                -sinf(a) * sinf(b),  cosf(a), -sinf(a) * cosf(b), 0,
                          -cosf(b),        0,            sinf(b), 0,
                                0,         0,                  0, 1,
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

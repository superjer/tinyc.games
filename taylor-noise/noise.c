#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <SDL.h>
#include <omp.h>

#define GL3_PROTOTYPES 1

#ifdef __APPLE__
#include <OpenGL/gl3.h>
#else
#include <GL/glew.h>
#endif

#define WINSZ 1600

int win_x = WINSZ;
int win_y = WINSZ;
int x;
int seed;

SDL_GLContext ctx;
SDL_Event event;
SDL_Window *win;

void setup();

#include "timer.c"
#include "../common/tinyc.games/utils.c"
#include "../common/tinyc.games/font.c"

#define VBUFLEN (WINSZ * 5)

unsigned main_prog_id;
GLuint main_vao;
GLuint main_vbo;
float vbuf[VBUFLEN];
int vbuf_n;

void draw_setup()
{
        fprintf(stderr, "GLSL version on this system is %s\n", (char *)glGetString(GL_SHADING_LANGUAGE_VERSION));
        unsigned int vertex = file2shader(GL_VERTEX_SHADER, "shaders/main.vert");
        unsigned int fragment = file2shader(GL_FRAGMENT_SHADER, "shaders/main.frag");
        main_prog_id = glCreateProgram();
        glAttachShader(main_prog_id, vertex);
        glAttachShader(main_prog_id, fragment);
        glLinkProgram(main_prog_id);
        check_program_errors(main_prog_id, "main");
        glDeleteShader(vertex);
        glDeleteShader(fragment);
        glGenVertexArrays(1, &main_vao);
        glGenBuffers(1, &main_vbo);
}

void vertex(float x, float y, float r, float g, float b)
{
        if (vbuf_n >= VBUFLEN - 5) return;
        vbuf[vbuf_n++] = x;
        vbuf[vbuf_n++] = y;
        vbuf[vbuf_n++] = r;
        vbuf[vbuf_n++] = g;
        vbuf[vbuf_n++] = b;
}

unsigned dumb_rand(unsigned seed)
{
        return rand();
        return (seed = (1103515245 * seed + 12345) % 2147483648);
}

unsigned lame_rand(unsigned seed)
{
        return (seed = (1103515245 * seed + 13456) % 2147483648);
}

int hit, miss;

float noise(int x, int y, int sz, int seed, int samples)
{
        // no negative numbers!
        sz &= 0x00ffffff;
        sz /= 2;
        x += 0x10000000;
        y += 0x01000000;
        if (x <= sz) x = sz + 1;
        if (y <= sz) y = sz + 1;

        struct memo {
                int i, j, u[16], v[16], n;
                int seed;
                int sz;
                union {
                        float f;
                        unsigned u;
                } strength[16];
        };
        static struct memo memos[17][17][17];

        float limit = sz * sz;
        float limit_inv = 1.f / limit;
        int xx = (x / sz) * sz;
        int yy = (y / sz) * sz;
        float sum_strengths = .5f;
        float sum_weights = 1.f;
        for (int i = xx-sz; i <= xx+sz; i+=sz) for (int j = yy-sz; j <= yy+sz; j+=sz)
        {
                struct memo *m = &memos[i % 17][j % 17][sz % 17];
                int is_memod = (m->i == i && m->j == j && m->sz == sz && m->seed == seed);
                if (!is_memod)
                {
                        m->i = i;
                        m->j = j;
                        m->sz = sz;
                        m->seed = seed;
                        memset(m->strength, 0, sizeof m->strength);
                        srand(i ^ (j * 128) ^ seed);
                        m->n = samples; //9 + rand() % 8;
                        miss++;
                }
                else
                        hit++;

                for (int n = 0; n < m->n; n++)
                {
                        if (!is_memod)
                        {
                                m->u[n] = i + rand() % sz;
                                m->v[n] = j + rand() % sz;
                                m->strength[n].u = (0x3f800000 | (0x007fffff & rand()));
                                m->strength[n].f -= 1.f;
                        }
                        float dist_sq = (x-m->u[n]) * (x-m->u[n]) + (y-m->v[n]) * (y-m->v[n]);
                        if (dist_sq > limit)
                                continue;
                        float weight = (limit - dist_sq) * limit_inv;
                        sum_strengths += m->strength[n].f * weight;
                        sum_weights += weight;
                }
        }
        return sum_strengths / sum_weights;
}

float remap(float val, float fromlo, float fromhi, float tolo, float tohi)
{
        val = CLAMP(fromlo, val, fromhi);
        val = (val - fromlo) / (fromhi - fromlo);
        val = val * (tohi - tolo) + tolo;
        return val;
}

float get_height(int x, int y)
{
        float val = noise(x, y, 200, seed, 4);
        float denom = 1.f;

        float oceaniness = remap(noise(x, y, 2030, seed, 1), .5f, .6f, 0.f, 1.f);
        if (oceaniness > 0.f && val > 0.46f)
        {
                float ocean_val = .2f * val;
                float ocean_val2 = val * (1.f - oceaniness) + ocean_val * oceaniness;
                float ocean_blend = remap(val, 0.46f, 0.49f, 0.f, 1.f);
                val = val * (1.f - ocean_blend) + ocean_val2 * ocean_blend;
        }

        if (val < .48f)
        {
                float flippy = remap(noise(x, y, 2008, seed, 1), .65f, .68f, 0.f, 1.f);
                if (flippy > 0.f)
                {
                        float val2 = remap(val, 0.f, .48f, 1.f, .48f);
                        val = val2 * flippy + val * (1.f - flippy);
                }
        }

        float shelfiness = remap(noise(x, y, 1200, seed, 1), .50f, .55f, 0.f, 1.f);
        if (shelfiness > 0.f)
        {
                float shelf_val = val;
                if (shelf_val >= .5f && shelf_val <= .6f)
                        shelf_val = remap(shelf_val, .5f, .6f, .5f, .505f);
                else if (shelf_val >= .6f)
                        shelf_val = remap(shelf_val, .6f, 1.f, .505f, .509f);
                val = shelf_val * shelfiness + val * (1.f - shelfiness);
        }

        float deepiness = remap(noise(x, y, 1150, seed, 1), .52f, .62f, 0.f, 1.f);
        if (deepiness > 0.f)
        {
                float deep_val = val;
                if (deep_val <= .49f && deep_val >= .44f)
                        deep_val = remap(deep_val, .44f, .49f, .2f, .49f);
                else if (deep_val <= .44f)
                        deep_val = remap(deep_val, 0.f, .44f, 0.f, .2f);
                val = deep_val * deepiness + val * (1.f - deepiness);
        }

        float intensity200 = CLAMP(0.f, remap(noise(x, y, 200, seed, 3) - .5f, -.5f, .5f, -12.f, 8.f), 1.f) * .5f;
        if (intensity200 > 0.f)
        {
                val += noise(x, y, 200, seed, 3) * intensity200;
                denom += intensity200;
        }

        float intensity100 = CLAMP(0.f, remap(noise(x, y, 100, seed, 3) - .5f, -.5f, .5f, -12.f, 8.f), 1.f) * .5f;
        if (intensity100 > 0.f)
        {
                val += noise(x, y, 100, seed, 3) * intensity100;
                denom += intensity100;
        }

        return val / denom;
}

void draw_verts()
{
        for (int y = 0; y < WINSZ; y++)
        {
                float h;
                int x2 = x;
                int y2 = y;

                // wacky spiral rotation
                {
                        int originx = 600;
                        int originy = 600;
                        int tx = x2 - originx;
                        int ty = y2 - originy;
                        float dist = sqrtf(tx * tx + ty * ty) / 600.f;
                        if (dist < 1.f)
                        {
                                float ang =  2.f * (1.f - dist) * (1.f - dist) * (1.f - dist)
                                        * remap(noise(x, y, 1080, seed, 3), .5f, 1.f, 0.f, 10.f);
                                x2 = originx + tx * cosf(ang) - ty * sinf(ang);
                                y2 = originy + tx * sinf(ang) + ty * cosf(ang);
                        }
                }

                // wacky spiral rotation reverse!
                {
                        int originx = 1000;
                        int originy = 1000;
                        int tx = x2 - originx;
                        int ty = y2 - originy;
                        float dist = sqrtf(tx * tx + ty * ty) / 600.f;
                        if (dist < 1.f)
                        {
                                float ang = -2.f * (1.f - dist) * (1.f - dist) * (1.f - dist)
                                        * remap(noise(x, y, 1120, seed, 3), .5f, 1.f, 0.f, 10.f);
                                x2 = originx + tx * cosf(ang) - ty * sinf(ang);
                                y2 = originy + tx * sinf(ang) + ty * cosf(ang);
                        }
                }

                float smoothness = remap(noise(x2, y2, 500, seed, 1), .55f, 1.f, 0.f, 1.f);
                if (0 && smoothness > 0.f)
                {
                        h = (get_height(x2 + smoothness, y2 + smoothness)
                           + get_height(x2 - smoothness, y2 + smoothness)
                           + get_height(x2 + smoothness, y2 - smoothness)
                           + get_height(x2 - smoothness, y2 - smoothness)) * .25f;
                }
                else
                        h = get_height(x2, y2);

                if (h > .7f)
                {
                        h = remap(h, .7f, 1.f, 1.f, 0.f);
                        vertex(x, y, 1.f, h, 1.f);
                }
                else if (h > .6f)
                {
                        h = remap(h, .6f, .7f, 0.f, 1.f);
                        vertex(x, y, h, 1.f, h);
                }
                else if (h > .51f)
                {
                        vertex(x, y, 0, remap(h, .51f, .6f, .4f, 1.f), 0);
                }
                else if (h > .49f)
                {
                        h = remap(h, .49f, .51f, .9f, .5f);
                        vertex(x, y, h, h, h * .2f);
                }
                else if (h > .3f)
                {
                        vertex(x, y, 0, 0, remap(h, .3f, .49f, 0.f, 1.f));
                }
                else
                {
                        vertex(x, y, remap(h, 0.f, .3f, 1.f, 0.f), 0, 0);
                }
        }

        if (++x == WINSZ)
        {
                char timings_buf[8000];
                timer_print(timings_buf, 8000, true);
                fprintf(stderr, "%s\nhits = %d\nmisses = %d\n", timings_buf, hit, miss);
        }
}

void draw_actual()
{
        glViewport(0, 0, win_x, win_y);
        //glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(main_prog_id);

        float near = -1.f;
        float far = 1.f;
        float x = 1.f / (win_x / 2.f);
        float y = -1.f / (win_y / 2.f);
        float z = -1.f / ((far - near) / 2.f);
        float tz = -(far + near) / (far - near);
        float ortho[] = {
                x, 0, 0,  0,
                0, y, 0,  0,
                0, 0, z,  0,
               -1, 1, tz, 1,
        };
        glUniformMatrix4fv(glGetUniformLocation(main_prog_id, "proj"), 1, GL_FALSE, ortho);

        glBindVertexArray(main_vao);
        glBindBuffer(GL_ARRAY_BUFFER, main_vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof vbuf, vbuf, GL_STATIC_DRAW);

        // show GL where the position data is
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), 0);
        glEnableVertexAttribArray(0);

        // show GL where the color data is
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)(2 * sizeof(float)));
        glEnableVertexAttribArray(1);

        glDrawArrays(GL_POINTS, 0, WINSZ);
        
        // defeat double or triple buffering
        TIMECALL(SDL_GL_SwapWindow, (win));
        glDrawArrays(GL_POINTS, 0, WINSZ);
        TIMECALL(SDL_GL_SwapWindow, (win));
        glDrawArrays(GL_POINTS, 0, WINSZ);

        vbuf_n = 0;
}

// the entry point and main game loop
int main()
{
        setup();
        for (;;)
        {
                while (SDL_PollEvent(&event)) switch (event.type)
                {
                        case SDL_KEYDOWN:
                                x = 0;
                                seed++;
                                break;
                        case SDL_QUIT:
                                exit(0);
                }

                if (x == WINSZ)
                {
                        TIMECALL(SDL_Delay, (10));
                }
                else
                {
                        TIMECALL(draw_verts, ());
                        TIMECALL(draw_actual, ());
                }
                TIMECALL(SDL_GL_SwapWindow, (win));
        }
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

// initial setup to get the window and rendering going
void setup()
{
        srand(time(NULL));
        seed = rand();

        SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_AUDIO);

        win = SDL_CreateWindow("Gridless Noise Test", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                win_x, win_y, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL);
        if (!win) exit(fprintf(stderr, "%s\n", SDL_GetError()));
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        ctx = SDL_GL_CreateContext(win);
        if (!ctx) exit(fprintf(stderr, "Could not create GL context\n"));

        SDL_GL_SetSwapInterval(0); // vsync?
        glClearColor(0.5f, 0.5f, 0.5f, 1.f);

        #ifndef __APPLE__
        glewExperimental = GL_TRUE;
        glewInit();
        glEnable(GL_DEBUG_OUTPUT);
        glDebugMessageCallback(MessageCallback, 0);
        #endif

        font_init();
        draw_setup();
}


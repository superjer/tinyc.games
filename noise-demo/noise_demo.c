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

#define WINSZ 1200

int win_x = WINSZ;
int win_y = WINSZ;

SDL_GLContext ctx;
SDL_Event event;
SDL_Window *win;

void setup();

#include "timer.c"
#include "../common/tinyc.games/utils.c"
#include "../common/tinyc.games/font.c"
#include "../common/tinyc.games/terrain.c"

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

int x;

void create_verts()
{
        for (int y = 0; y < WINSZ; y++)
        {
                float h = get_filtered_height(x, y);

                #ifndef BLACK_AND_WHITE
                #define BLACK_AND_WHITE 0
                #endif
                if (BLACK_AND_WHITE) // black and white
                {
                        h = remap(h, 0.f, 1.f, -1.f, 2.f);
                        vertex(x, y, h, h, h);
                }
                else if (h > .7f)
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
                fprintf(stderr, "%s\n"
                                "     noise(): hits   = %10d\n"
                                "              misses = %10d\n"
                                "get_height(): hits   = %10d\n"
                                "              misses = %10d\n",
                                timings_buf, noise_hit, noise_miss, get_height_hit, get_height_miss);
        }
}

void draw_verts()
{
        glViewport(0, 0, win_x, win_y);
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
                        TIMECALL(create_verts, ());
                        TIMECALL(draw_verts, ());
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
        //seed = rand();

        SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_AUDIO);

        win = SDL_CreateWindow("Taylor Noise Test", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
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
        noise_setup();
}

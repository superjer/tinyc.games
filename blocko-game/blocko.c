// Blocko -- http://tinyc.games -- (c) 2025 Jer Wilson
//
// Blocko is a 1st-person block building game using OpenGL via GLEW.
//
// Blocko is part of the TinyC.games project
//   http://tinyc.games
//   https://github.com/superjer/tinyc.games

#ifndef BLOCKO_C_INCLUDED
#define BLOCKO_C_INCLUDED

#ifndef NO_OMPH
        #include <omp.h>
#else
        #define omp_get_num_threads() 0
        #define omp_set_nested(n)
#endif

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <stdbool.h>
#define GL3_PROTOTYPES 1

#define SDL_DISABLE_IMMINTRIN_H
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_vulkan.h>
#define TCGVK_SKIP_MAIN
#include "../common/tinyc.games/vulkan/main.c"

#define STBI_NO_SIMD
#define STB_IMAGE_IMPLEMENTATION
#include "../common/nothings/stb_image.h"

#include "../common/smcameron/open-simplex-noise.c"
struct osn_context *osn_context;
#define noise(x,y,z,scale) open_simplex_noise3(osn_context,(float)((x)-tscootx+0.5f)/(scale),(float)((y)+0.5f)/(scale),(float)((z)-tscootz+0.5f)/(scale))

#define TINYC_DIR ".."
#include "build-config.h"

#include "timer.c"
#include "vector.c"
#include "defs.c"
#include "atmosphere.c"
#include "collision.c"
#include "draw.c"
#include "font.c"
#include "cursor.c"
#include "glsetup.c"
#include "interface.c"
#include "blocklight.c"
#include "player.c"
#include "test.c"
#include "terrain.c"

//prototypes
void startup();
void new_game();
void main_loop();
void update_world();

int main()
{
        omp_set_nested(1); // needed or omp won't parallelize chunk gen
        startup();

        #pragma omp parallel sections
        {
                #pragma omp section
                { // main thread
                        TIMECALL(glsetup, ());
                        TIMECALL(font_init, ());
                        TIMECALL(cursor_init, ());
                        TIMECALL(sun_init, ());
                        char timings_buf[8000];
                        timer_print(timings_buf, 8000, true);
                        printf("%s", timings_buf);
                        if (TERRAIN_THREAD)
                                new_game();
                        else
                                create_hmap();
                        main_loop();
                }

                #pragma omp section
                { // worker thread for terrain generation
                        if (TERRAIN_THREAD)
                        {
                                create_hmap();
                                chunk_builder();
                        }
                }
        }
}

void shutdown(int code)
{
        exit(code);
        vulkan_shutdown();
        exit(code);
}

void main_loop()
{ for (;;) {
        apply_scoot();

        while (SDL_PollEvent(&event)) switch (event.type)
        {
                case SDL_EVENT_QUIT:              shutdown(0);
                case SDL_EVENT_KEY_DOWN:          key_move(1);       break;
                case SDL_EVENT_KEY_UP:            key_move(0);       break;
                case SDL_EVENT_MOUSE_MOTION:      mouse_move();      break;
                case SDL_EVENT_MOUSE_BUTTON_DOWN: mouse_button(1);   break;
                case SDL_EVENT_MOUSE_BUTTON_UP:   mouse_button(0);   break;
                case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                        resize();
                        break;
        }

        if (!TERRAIN_THREAD)
        {
                chunk_builder();
                chunk_builder();
                chunk_builder();
                chunk_builder();
                if (frame == 0)
                        new_game();
        }

        float interval = 1000.f / 60.f;
        static float accumulated_elapsed = 0.f;
        static int last_ticks = 0;
        int ticks = SDL_GetTicks();
        accumulated_elapsed += ticks - last_ticks;
        last_ticks = ticks;
        CLAMP(accumulated_elapsed, 0, interval * 3 - 1);

        if (!regulated) accumulated_elapsed = interval;

        while (accumulated_elapsed >= interval)
        {
                TIMECALL(update_player, (&player[0], 1));
                TIMECALL(update_world, ());
                pframe++;
                accumulated_elapsed -= interval;
        }

        camplayer = player[0];

        if (regulated)
        {
                TIMECALL(update_player, (&camplayer, 0));
        }

        lerp_camera(accumulated_elapsed / interval, &player[0], &camplayer);
        TIMECALL(step_sunlight, ());
        TIMECALL(step_glolight, ());
        draw_stuff();
        frame++;
} }

void startup()
{
        open_simplex_noise(world_seed, &osn_context);

        tiles = calloc(TILESD * TILESH * TILESW, sizeof *tiles);
        sunlight = calloc(TILESD * TILESH * TILESW, sizeof *sunlight);
        glolight = calloc(TILESD * TILESH * TILESW, sizeof *glolight);
        cornlight = calloc((TILESD+1) * (TILESH+1) * (TILESW+1), sizeof *cornlight);
        kornlight = calloc((TILESD+1) * (TILESH+1) * (TILESW+1), sizeof *kornlight);
}

void new_game()
{
        while(just_gen_len < 4)
                SDL_Delay(1); // wait for worker thread build first chunk

        printf("1st 4 chunks generated, ready to start game\n");

        recalc_gndheight(STARTPX/BS, STARTPZ/BS);
        move_to_ground(&player[0].pos.y, STARTPX/BS, STARTPY/BS, STARTPZ/BS);
}

void update_world()
{
        int i, x, y, z;
        unsigned seed = SEED1(pframe);
        for (i = 0; i < 500; i++) {
                x = RANDI(1, TILESW - 2);
                z = RANDI(1, TILESD - 2);

                for (y = 1; y < TILESH - 1; y++) {
                        if (0) ;
                        else if (T_(x, y, z) == GRG1) T_(x, y, z) = GRG2;
                        else if (T_(x, y, z) == GRG2) T_(x, y, z) = GRAS;
                        else if (T_(x, y, z) == DIRT) {
                                if (T_(x  , y-1, z  ) == OPEN && (
                                    (T_(x  , y  , z+1) | 1) == GRAS ||
                                    (T_(x  , y  , z-1) | 1) == GRAS ||
                                    (T_(x+1, y  , z  ) | 1) == GRAS ||
                                    (T_(x-1, y  , z  ) | 1) == GRAS ||
                                    (T_(x  , y+1, z+1) | 1) == GRAS ||
                                    (T_(x  , y+1, z-1) | 1) == GRAS ||
                                    (T_(x+1, y+1, z  ) | 1) == GRAS ||
                                    (T_(x-1, y+1, z  ) | 1) == GRAS ||
                                    (T_(x  , y-1, z+1) | 1) == GRAS ||
                                    (T_(x  , y-1, z-1) | 1) == GRAS ||
                                    (T_(x+1, y-1, z  ) | 1) == GRAS ||
                                    (T_(x-1, y-1, z  ) | 1) == GRAS) ) {
                                        T_(x, y, z) = GRG1;
                                }
                                break;
                        }
                }
        }

        float speed = speedy_sun ? 0.01f : 0.0001f;
        sun_pitch += speed * (reverse_sun ? -1 : 1);
        if (sun_pitch > TAU) sun_pitch -= TAU;
        if (sun_pitch < 0.f) sun_pitch += TAU;
}


void move_to_ground(float *inout, int x, int y, int z)
{
        *inout = GNDH_(x, z) * BS - PLYR_H - 1;
}

void recalc_gndheight(int x, int z)
{
        int y;
        for (y = 0; y < TILESH-1; y++)
        {
                if (T_(x, y, z) != OPEN)
                        break;
        }

        GNDH_(x, z) = y;
}

// select block from eye following vector f
void rayshot(float eye0, float eye1, float eye2, float f0, float f1, float f2)
{
        int x = (int)(eye0 / BS);
        int y = (int)(eye1 / BS);
        int z = (int)(eye2 / BS);

        for (int i = 0; ; i++)
        {
                float a0 = (BS * (x + (f0 > 0 ? 1 : 0)) - eye0) / f0;
                float a1 = (BS * (y + (f1 > 0 ? 1 : 0)) - eye1) / f1;
                float a2 = (BS * (z + (f2 > 0 ? 1 : 0)) - eye2) / f2;
                float amt = 0;

                place_x = x;
                place_y = y;
                place_z = z;

                if (a0 < a1 && a0 < a2) { x += (f0 > 0 ? 1 : -1); amt = a0; }
                else if (a1 < a2)       { y += (f1 > 0 ? 1 : -1); amt = a1; }
                else                    { z += (f2 > 0 ? 1 : -1); amt = a2; }

                eye0 += amt * f0 * 1.0001;
                eye1 += amt * f1 * 1.0001;
                eye2 += amt * f2 * 1.0001;

                if (x < 0 || y < 0 || z < 0 || x >= TILESW || y >= TILESH || z >= TILESD)
                        goto bad;

                if (T_(x, y, z) != OPEN)
                        break;

                if (i == 6)
                        goto bad;
        }

        target_x = x;
        target_y = y;
        target_z = z;

        return;

        bad:
        target_x = target_y = target_z = -1;
        place_x = place_y = place_z = -1;
}

void scoot(int cx, int cz)
{
        #pragma omp critical
        {
                future_scootx += cx;
                future_scootz += cz;
        }
}

void apply_scoot()
{
        #pragma omp critical
        {
                scootx = future_scootx * CHUNKW;
                scootz = future_scootz * CHUNKD;
                chunk_scootx = future_scootx;
                chunk_scootz = future_scootz;
        }
}

#endif // BLOCKO_C_INCLUDED

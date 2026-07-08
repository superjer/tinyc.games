// Blocko -- http://tinyc.games -- (c) 2025 Jer Wilson
//
// Blocko is a 1st-person block building game using Vulkan.
//
// Blocko is part of the TinyC.games project
//   http://tinyc.games
//   https://github.com/superjer/tinyc.games

#ifndef BLOCKO_C_INCLUDED
#define BLOCKO_C_INCLUDED

#ifndef NO_OMPH
        #include <omp.h>
#else
        #define omp_get_num_threads() 1
        #define omp_get_thread_num() 0
        #define omp_get_max_threads() 1
        #define omp_set_nested(n)
        #define omp_set_num_threads(n)
#endif

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <time.h>
#include <math.h>
#include <stdbool.h>
#define GL3_PROTOTYPES 1

#define TINYC_DIR ".."
#include "build-config.h"

#define SDL_DISABLE_IMMINTRIN_H
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_vulkan.h>
#include "../common/tinyc.games/vulkan/main.c"
#include "../common/tinyc.games/utils.c"
#include "../common/tinyc.games/taylor_noise.c"

#define STBI_NO_SIMD
#define STB_IMAGE_IMPLEMENTATION
#include "../common/nothings/stb_image.h"

#include "timer.c"
#include "vector.c"
#include "defs.c"
#include "../common/tinyc.games/terrain.c"
#include "formations.c"
#include "atmosphere.c"
#include "collision.c"
#include "mesh.c"
#include "shadow.c"
#include "draw.c"
#include "../common/tinyc.games/font.c"
#include "cursor.c"
#include "vksetup.c"
#include "remote.c"
#include "interface.c"
#include "blocklight.c"
#include "player.c"
#include "mob.c"
#include "mine.c"
#include "hand.c"
#include "item.c"
#include "patch.c"
#include "test.c"
#include "chunker.c"

//prototypes
void startup();
void new_game();
void main_loop();
void update_world();
void chunk_builder();

// SDL-thread entry point for the terrain workers (see main)
static int chunk_builder_thread(void *unused) { chunk_builder(); return 0; }

int main(int argc, char **argv)
{
        world_seed = time(NULL);

        for (int i = 1; i < argc; i++)
        {
                if      (!strcmp(argv[i], "--noise-kernel2"))                    noise_kernel_sq = 1;
                else if (!strcmp(argv[i], "--noise-nvary"))                      noise_nvary = 1;
                else if (!strcmp(argv[i], "--noise-contrast") && i + 1 < argc)   noise_base_weight = atof(argv[++i]);
                else if (!strcmp(argv[i], "--noise-aniso") && i + 1 < argc)      noise_aniso = atof(argv[++i]);
                else if (!strcmp(argv[i], "--seed") && i + 1 < argc)             world_seed = atoi(argv[++i]);
                else
                {
                        fprintf(stderr, "usage: %s [--seed <n>] [--noise-kernel2] [--noise-nvary] "
                                "[--noise-contrast <weight>] [--noise-aniso <0..1>]\n", argv[0]);
                        return 1;
                }
        }

        fprintf(stderr, "world seed: %d\n", world_seed);
        fprintf(stderr, "OpenMP threads available: %d\n", omp_get_max_threads());
        startup();

        // Terrain workers run as plain SDL threads, launched once and left
        // running, rather than as OpenMP sections. That keeps the per-frame
        // mesh build's #pragma omp parallel at the TOP level, where libgomp
        // reuses its pooled team (cheap) instead of spinning up a nested team
        // every rebuild (which cost ~11ms of fork/join). The workers only use
        // #pragma omp critical/atomic, which are plain global locks that work
        // from any thread.
        if (TERRAIN_THREAD)
        {
                SDL_DetachThread(SDL_CreateThread(chunk_builder_thread, "terrain0", NULL));
                SDL_DetachThread(SDL_CreateThread(chunk_builder_thread, "terrain1", NULL));
        }

        TIMECALL(vksetup, ());
        TIMECALL(font_init, ());
        TIMECALL(cursor_init, ());
        TIMECALL(sun_init, ());
        char timings_buf[8000];
        timer_print(timings_buf, 8000, true);
        printf("%s", timings_buf);
        if (TERRAIN_THREAD)
                new_game();
        main_loop();
}

void game_shutdown(int code) // "shutdown" collides with the sockets API
{
        exit(code);
        vulkan_shutdown();
        exit(code);
}

void main_loop()
{ for (;;) {
        auto_scoot();
        apply_scoot();

        while (SDL_PollEvent(&event)) switch (event.type)
        {
                case SDL_EVENT_QUIT:              if (!test_lock) game_shutdown(0);
                                                  break;
                case SDL_EVENT_KEY_DOWN:          key_move(1);       break;
                case SDL_EVENT_TEXT_INPUT:        console_text(event.text.text); break;
                case SDL_EVENT_KEY_UP:            key_move(0);       break;
                case SDL_EVENT_MOUSE_MOTION:      if (!test_lock) mouse_move();    break;
                case SDL_EVENT_MOUSE_BUTTON_DOWN: if (!test_lock) mouse_button(1); break;
                case SDL_EVENT_MOUSE_BUTTON_UP:   if (!test_lock) mouse_button(0); break;
                case SDL_EVENT_MOUSE_WHEEL:       if (!test_lock) mouse_wheel();   break;
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
        accumulated_elapsed = CLAMP(accumulated_elapsed, 0, interval * 3 - 1);

        if (!regulated) accumulated_elapsed = interval;

        while (accumulated_elapsed >= interval)
        {
                TIMECALL(update_player, (&player[0], 1));
                hand_animate(&player[0]);
                TIMECALL(update_mobs, ());
                update_items();
                TIMECALL(update_world, ());
                pframe++;
                accumulated_elapsed -= interval;
        }

        camplayer = player[0];

        if (regulated)
        {
                TIMECALL(update_player, (&camplayer, 0));
        }

        remote_poll();

        mob_lerp_t = accumulated_elapsed / interval;
        lerp_camera(accumulated_elapsed / interval, &player[0], &camplayer);
        TIMECALL(step_sunlight, ());
        TIMECALL(step_glolight, ());
        draw_stuff();
        frame++;
} }

void startup()
{
        noise_setup();
        remote_init();

        // size the mesh-build thread team: enough to hide memory latency,
        // but leaving cores for the two terrain workers and the main thread
        mesh_threads = ICLAMP(omp_get_max_threads() - 4, 2, 8);

        for (int i = 0; i < VAOD; i++) for (int j = 0; j < VAOW; j++)
        {
                chunk_stamp[i][j].ax = INT_MIN;
                chunk_stamp[i][j].az = INT_MIN;
                chunk_estamp[i][j].ax = INT_MIN;
                chunk_estamp[i][j].az = INT_MIN;
        }

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
        float speed = sun_frozen ? 0.f : speedy_sun ? 0.01f : 0.0001f;
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

                // pass through water like air: mining hits the solid block behind
                // it, and building lands on the water cell before that block (so it
                // replaces the water). you can't target/mine water itself.
                if (T_(x, y, z) != OPEN && T_(x, y, z) != WATR)
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

// scoot the window whenever the player wanders off the center chunk,
// so they never get far from the origin (no float precision problems)
void auto_scoot()
{
        int cx = (int)(player[0].pos.x / (BS * CHUNKW));
        int cz = (int)(player[0].pos.z / (BS * CHUNKD));
        if (cx != VAOW/2 || cz != VAOD/2)
                scoot(VAOW/2 - cx, VAOD/2 - cz);
}

void apply_scoot()
{
        #pragma omp critical
        {
                int dx = (future_scootx - chunk_scootx) * CHUNKW; // window coords of
                int dz = (future_scootz - chunk_scootz) * CHUNKD; // everything move by this

                if (dx || dz)
                {
                        player[0].pos.x += dx * BS;
                        player[0].pos.z += dz * BS;

                        mob_scoot(dx, dz);
                        item_scoot(dx, dz);

                        // pending light updates hold window coords
                        for (size_t i = 0; i < sq_curr_len; i++) { sunq_curr[i].x += dx; sunq_curr[i].z += dz; }
                        for (size_t i = 0; i < sq_next_len; i++) { sunq_next[i].x += dx; sunq_next[i].z += dz; }
                        for (size_t i = 0; i < gq_curr_len; i++) { gloq_curr[i].x += dx; gloq_curr[i].z += dz; }
                        for (size_t i = 0; i < gq_next_len; i++) { gloq_next[i].x += dx; gloq_next[i].z += dz; }

                        // so does the current block target
                        if (target_x >= 0) { target_x += dx; target_z += dz; }
                        if (place_x >= 0)  { place_x  += dx; place_z  += dz; }

                        // stored shadow matrices expect the old window coords
                        for (int i = 0; i < SHADOW_COUNT; i++)
                                retranslate(shadow[i].matrix, -dx * (float)BS, 0.f, -dz * (float)BS);

                        // so does the (possibly locked) chunk-culling frustum
                        retranslate(cull_mtrx, -dx * (float)BS, 0.f, -dz * (float)BS);
                        cull_x += dx * BS;
                        cull_z += dz * BS;
                }

                scootx = future_scootx * CHUNKW;
                scootz = future_scootz * CHUNKD;
                chunk_scootx = future_scootx;
                chunk_scootz = future_scootz;
        }
}

#endif // BLOCKO_C_INCLUDED

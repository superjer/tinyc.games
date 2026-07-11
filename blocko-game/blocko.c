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
#include <signal.h>
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

#include "vector.c"
#include "defs.c"
#include "../common/tinyc.games/terrain.c"
#include "atmosphere.c"
#include "simarea.c"
#include "collision.c"
#include "mesh.c"
#include "shadow.c"
#include "draw.c"
#include "../common/tinyc.games/font.c"
#include "cursor.c"
#include "vksetup.c"
#include "console.c"
#include "interface.c"
#include "blocklight.c"
#include "edit.c"
#include "net.c"
#include "player.c"
#include "mob.c"
#include "mine.c"
#include "hand.c"
#include "test.c"
#include "chunker.c"

//prototypes
void startup();
void new_game();
void main_loop();
void chunk_builder();

// SDL-thread entry point for the terrain workers (see main)
static int chunk_builder_thread(void *unused) { chunk_builder(); return 0; }

// headless has no window to close, so Ctrl+C is the way out; the handler just
// raises a flag and the main loop exits cleanly between frames
static volatile sig_atomic_t got_sigint;
static void on_sigint(int sig) { (void)sig; got_sigint = 1; }

int main(int argc, char **argv)
{
        world_seed = time(NULL);
        int serve = 0, serve_port = NET_PORT;
        char *connect_host = NULL;
        int connect_port = NET_PORT;

        for (int i = 1; i < argc; i++)
        {
                if      (!strcmp(argv[i], "--seed") && i + 1 < argc)             world_seed = atoi(argv[++i]);
                else if (!strcmp(argv[i], "--dist") && i + 1 < argc)             draw_dist = atof(argv[++i]);
                else if (!strcmp(argv[i], "--headless"))                         headless = 1;
                else if (!strcmp(argv[i], "--serve"))
                {
                        serve = 1;
                        if (i + 1 < argc && atoi(argv[i+1]) > 0)
                                serve_port = atoi(argv[++i]);
                }
                else if (!strcmp(argv[i], "--connect") && i + 1 < argc)
                {
                        connect_host = argv[++i];
                        char *colon = strchr(connect_host, ':');
                        if (colon)
                        {
                                *colon = '\0';
                                connect_port = atoi(colon + 1);
                        }
                }
                else
                {
                        fprintf(stderr, "usage: %s [--seed <n>] [--serve [port]] [--connect <host[:port]>] "
                                "[--headless] [--dist <blocks>]\n", argv[0]);
                        return 1;
                }
        }

        // headless with nothing to connect to means a dedicated server
        if (headless && !connect_host)
                serve = 1;

        fprintf(stderr, "world seed: %d\n", world_seed);
        fprintf(stderr, "OpenMP threads available: %d\n", omp_get_max_threads());
        startup();

        // join/host before the terrain workers launch, so a client's first
        // chunks already generate from the server's seed (net_connect blocks
        // until WELCOME lands or times out)
        if (serve)
                net_serve(serve_port);
        else if (connect_host)
                net_connect(connect_host, connect_port);

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

        if (headless)
        {
                signal(SIGINT, on_sigint);
                signal(SIGTERM, on_sigint);
                fprintf(stderr, "headless: no window; stop with Ctrl+C\n");
        }
        else
        {
                vksetup();
                // the flat sky IS the clear color (keep in sync with SKY_COLOR
                // in main.frag, which fogs distant terrain to match)
                vk.clear_color = (VkClearColorValue){{0.53f, 0.71f, 0.92f, 1.f}};
                font_init();
                cursor_init();
                sun_init();
        }
        if (TERRAIN_THREAD)
                new_game();
        main_loop();
}

void game_shutdown(int code) // "shutdown" collides with the sockets API
{
        // every normal quit path (window close, `bk quit`, console quit) routes
        // through here with code 0. announce it so a clean close isn't mistaken
        // for a crash by anything watching the process; nonzero is reserved for
        // real errors, which exit() elsewhere with a message of their own.
        if (code == 0)
                fprintf(stderr, "blocko: clean exit\n");
        else
                fprintf(stderr, "blocko: exit code %d (error)\n", code);
        exit(code);
}

void main_loop()
{ for (;;) {
        auto_scoot();
        apply_scoot();

        if (got_sigint)
                game_shutdown(0);

        if (!headless) while (SDL_PollEvent(&event)) switch (event.type)
        {
                case SDL_EVENT_QUIT:              game_shutdown(0);  break;
                case SDL_EVENT_KEY_DOWN:          key_move(1);       break;
                case SDL_EVENT_TEXT_INPUT:        console_text(event.text.text); break;
                case SDL_EVENT_KEY_UP:            key_move(0);       break;
                case SDL_EVENT_MOUSE_MOTION:      mouse_move();      break;
                case SDL_EVENT_MOUSE_BUTTON_DOWN: mouse_button(1);   break;
                case SDL_EVENT_MOUSE_BUTTON_UP:   mouse_button(0);   break;
                case SDL_EVENT_MOUSE_WHEEL:       mouse_wheel();     break;
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

        while (accumulated_elapsed >= interval)
        {
                update_player(&player[my_player], 1);
                hand_animate(&player[my_player]);
                update_mobs();
                pframe++;
                accumulated_elapsed -= interval;
        }

        net_poll();
        sim_areas_update();

        if (headless)
        {
                // aim: rayshot normally runs in draw, from the lerped camera.
                // there is no camera here, so aim straight from the player -
                // otherwise target/click (mining, placing) never work headless
                struct player *p = &player[my_player];
                rayshot(p->pos.x + PLYR_W / 2,
                        p->pos.y + EYEDOWN * (p->sneaking ? 2 : 1),
                        p->pos.z + PLYR_W / 2,
                        cosf(p->pitch) * sinf(p->yaw),
                        sinf(p->pitch),
                        cosf(p->pitch) * cosf(p->yaw));

                // no drawing: adopt fresh chunks (otherwise draw_stuff's job)
                // and sleep out the rest of the tick so the loop still runs
                // at ~60Hz. Round the sleep UP: rounding down leaves sub-ms
                // of tick unspent and the loop spins through it at hundreds
                // of frames per tick
                sync_fresh_chunks();
                SDL_Delay((int)(interval - accumulated_elapsed) + 1);
                frame++;
                continue;
        }

        camplayer = player[my_player];
        update_player(&camplayer, 0);

        mob_lerp_t = accumulated_elapsed / interval;
        lerp_camera(accumulated_elapsed / interval, &player[my_player], &camplayer);
        draw_stuff();
        frame++;
} }

void startup()
{
        noise_setup();

        // size the mesh-build thread team: enough to hide memory latency,
        // but leaving cores for the two terrain workers and the main thread
        mesh_threads = ICLAMP(omp_get_max_threads() - 4, 2, 8);

        for (int i = 0; i < VAOD; i++) for (int j = 0; j < VAOW; j++)
        {
                chunk_stamp[i][j].ax = INT_MIN;
                chunk_stamp[i][j].az = INT_MIN;
                chunk_estamp[i][j].ax = INT_MIN;
                chunk_estamp[i][j].az = INT_MIN;
                mesh_stamp[i][j].ax = INT_MIN;
                mesh_stamp[i][j].az = INT_MIN;
        }

        tiles = calloc(TILESD * TILESH * TILESW, sizeof *tiles);
        main_area = (struct warea){ .tiles = tiles, .gndh = gndheight,
                .maskw = TILESW-1, .maskd = TILESD-1, .pitchx = TILESH, .pitchz = TILESW * TILESH };
        cornlight = calloc((TILESD+1) * (TILESH+1) * (TILESW+1), sizeof *cornlight);
}

void new_game()
{
        while(just_gen_len < 4)
                SDL_Delay(1); // wait for worker thread build first chunk

        printf("1st 4 chunks generated, ready to start game\n");

        recalc_gndheight(STARTPX/BS, STARTPZ/BS);
        move_to_ground(&player[my_player].pos.y, STARTPX/BS, STARTPY/BS, STARTPZ/BS);
}

// invalidate every chunk's generation stamp: the whole ring regenerates in
// place, nearest chunks first. Recorded edits replay as the chunks return.
void regen_world()
{
        #pragma omp critical (chunks)
        {
                regen_epoch++;
                for (int i = 0; i < VAOD; i++) for (int j = 0; j < VAOW; j++)
                {
                        chunk_stamp[i][j].ax = INT_MIN;
                        chunk_stamp[i][j].az = INT_MIN;
                        chunk_estamp[i][j].ax = INT_MIN;
                        chunk_estamp[i][j].az = INT_MIN;
                }
                // sim area copies are stale-seed data now, too
                for (int i = 0; i < NR_PLAYERS; i++)
                {
                        struct warea *a = &sim_area[i];
                        for (int z = 0; z < SIM_AREA_CHUNKS; z++)
                        for (int x = 0; x < SIM_AREA_CHUNKS; x++)
                        {
                                a->stamp[z][x].ax = INT_MIN;
                                a->stamp[z][x].az = INT_MIN;
                                a->estamp[z][x].ax = INT_MIN;
                                a->estamp[z][x].az = INT_MIN;
                        }
                        a->epoch++;
                }
        }
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
                // replaces the water). you can't target/mine water itself. tall
                // grass (both kinds) is likewise non-mineable and lets the ray
                // through to the block it grows on.
                int tt = T_(x, y, z);
                if (tt != OPEN && tt != WATR && tt != TLGR && tt != TMGR)
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
        int cx = (int)(player[my_player].pos.x / (BS * CHUNKW));
        int cz = (int)(player[my_player].pos.z / (BS * CHUNKD));
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
                        for (int i = 0; i < NR_PLAYERS; i++)
                        {
                                player[i].pos.x += dx * BS;
                                player[i].pos.z += dz * BS;
                        }

                        mob_scoot(dx, dz);

                        // the current block target holds window coords
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

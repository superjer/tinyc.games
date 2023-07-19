#include "blocko.h"

#define GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX    0x9048
#define GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX  0x9049

int in_test_area(int x, int y, int z)
{
        return fabsf(x - player[0].pos.x / BS) < 16 &&
               floorf(y - player[0].pos.y / BS) == 0 &&
               fabsf(z - player[0].pos.z / BS) < 16;
}

void build_test_area()
{
        int tx = test_area_x = ICLAMP(player[0].pos.x / BS - TEST_AREA_SZ/2, 0, TILESW - TEST_AREA_SZ);
        int ty = test_area_y = ICLAMP(player[0].pos.y / BS + 1             , 0, TILESH - 10          );
        int tz = test_area_z = ICLAMP(player[0].pos.z / BS - TEST_AREA_SZ/2, 0, TILESD - TEST_AREA_SZ);

        show_light_values = true;

        for (int x = tx; x < tx+TEST_AREA_SZ; x++) for (int z = tz; z < tz+TEST_AREA_SZ; z++) for (int y = 0; y < ty+20; y++)
        {
                int on_edge = (x == tx || x == tx+TEST_AREA_SZ-1 || z == tz || z == tz+TEST_AREA_SZ-1);
                if (y == ty - 5) // ceiling
                {
                        if (on_edge)
                        {
                                T_(x, y, z) = OPEN;
                                SUN_(x, y, z) = 15;
                        }
                        else
                        {
                                T_(x, y, z) = GRAN;
                                SUN_(x, y, z) = 0;
                                GNDH_(x, z) = y;
                        }
                }
                else if (y < ty + 1) // space inside
                {
                        T_(x, y, z) = OPEN;
                        SUN_(x, y, z) = 0;
                        if (on_edge)
                        {
                                GNDH_(x, z) = y;
                                sun_enqueue(x, y, z, 0, 15);
                        }
                }
                else // floor
                {
                        T_(x, y, z) = GRAN;
                        SUN_(x, y, z) = 0;
                }

        }

        recalc_corner_lighting(tx, tx + TEST_AREA_SZ, tz, tz + TEST_AREA_SZ);
}

void debrief()
{
        static unsigned last_ticks = 0;
        static unsigned last_frame = 0;
        static GLint total_kb = 0;
        static GLint avail_kb = 0;
        unsigned ticks = SDL_GetTicks();
        static char buf[8000];
        static char timings_buf[8000];
        char *p = buf;

        if (ticks - last_ticks >= 1000) {
                float elapsed = ((float)ticks - last_ticks);
                float frames = frame - last_frame;

                if (GLEW_NVX_gpu_memory_info) {
                        p += snprintf(p, 8000 - (p-buf),
                                      "vmem %0.0fm used of %0.0fm (%0.0f%% free)\n",
                                      (float)(total_kb - avail_kb) / 1000.f,
                                      (float)(total_kb)            / 1000.f,
                                      ((float)avail_kb / total_kb) * 100.f);
                }

                p += snprintf(p, 8000 - (p-buf),
                                "%d omp, %0.2f chunk/s\n",
                                omp_threads,
                                (float)nr_chunks_generated / (chunk_gen_ticks / 1000.f));

                p += snprintf(p, 8000 - (p-buf),
                                "%.3fm poly/s, %.3f shadow poly/s\n",
                                1000.f * (float)polys / elapsed / 1000000.f,
                                1000.f * (float)shadow_polys / elapsed / 1000000.f);

                p += snprintf(p, 8000 - (p-buf),
                                "%.1f fps\n", 1000.f * frames / elapsed );

                if (sunq_outta_room)
                        p += snprintf(p, 8000 - (p-buf),
                                        "Out of room in the sun queue (%d times)\n", sunq_outta_room);
                sunq_outta_room = 0;

                if (gloq_outta_room)
                        p += snprintf(p, 8000 - (p-buf),
                                        "Out of room in the glo queue (%d times)\n", gloq_outta_room);
                gloq_outta_room = 0;

                if (GLEW_NVX_gpu_memory_info) {
                        glGetIntegerv(GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX, &total_kb);
                        glGetIntegerv(GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX, &avail_kb);
                }

                last_ticks = ticks;
                last_frame = frame;
                polys = 0;
                shadow_polys = 0;

                timer_print(timings_buf, 8000, false);
        }

        if (noisy)
        {
                char xyzbuf[100];
                snprintf(xyzbuf, 100,
                                "X=%0.0f Y=%0.0f Z=%0.0f drawdist=%.0f %svsync %sreg %smsaa %sfast %scull %slock",
                                player[0].pos.x / BS, player[0].pos.y / BS, player[0].pos.z / BS,
                                draw_dist,
                                vsync           ? "" : "no",
                                regulated       ? "" : "no",
                                antialiasing    ? "" : "no",
                                fast > 1        ? "" : "no",
                                frustum_culling ? "" : "no",
                                lock_culling    ? "" : "no");

                font_begin(screenw, screenh);
                font_add_text(buf, 0, 0, 0);
                font_add_text(xyzbuf, 0, 0.955f * screenh, 0);
                font_end(1, 1, 1);

                font_begin(screenw, screenh);
                font_add_text(timings_buf, 0.80f * screenw, 0, 2);
                font_end(1, 1, 1);
        }

        if (help_layer == 0 && pframe < 200)
        {
                font_begin(screenw, screenh);
                font_add_text("Press H for help", screenw * 0.45f, screenh/2.f, 0);
                font_end(1, .5f, 1);
        }

        if (help_layer == 1)
        {
                char *h1 = "WASD\nShift\nCtrl/WW\nSpc/MB4\nLMB  \nRMB  \nE          \nZ   \nC            \nH                  \nPress G for more";
                char *h2 = "Move\nSneak\nRun    \nJump   \nBreak\nBuild\nPlace Light\nZoom\nDraw Distance\nHide this help text";
                font_begin(screenw, screenh);
                font_add_text(h1, screenw/100.f, screenh/4.f, 0);
                font_end(1, 0.5, 1);
                font_begin(screenw, screenh);
                font_add_text(h2, screenw/9.f, screenh/4.f, 0);
                font_end(1, 1, 1);
        }

        if (help_layer == 2)
        {
                char *g1 = "Q     \nF   \nN       \nP       \nT       \nL         \nM             \nV    \nR             \n/   \nF1     \nF2          \nF3                    \nF4 ";
                char *g2 = "Go up!\nFast\nRev. sun\nFast sun\nTest box\nLight vals\nShadow mapping\nVsync\nFixed interval\nMSAA\nCulling\nLock culling\nFPS, timings, position\nShow fresh updates";
                font_begin(screenw, screenh);
                font_add_text(g1, screenw/100.f, screenh/4.f, 0);
                font_end(0.5, 1, 1);
                font_begin(screenw, screenh);
                font_add_text(g2, screenw/20.f, screenh/4.f, 0);
                font_end(1, 1, 1);
        }

        // for debugging only
        if (alert[0])
        {
                font_begin(screenw, screenh);
                font_add_text(alert, screenw/2.f, screenh/2.f, 0);
                font_end(1, 0.5, 0);
        }
}

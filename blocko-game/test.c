#include "blocko.c"
#ifndef BLOCKO_TEST_C_INCLUDED
#define BLOCKO_TEST_C_INCLUDED

void debrief()
{
        font_frame_reset();  // Reset font buffer offset for this frame

        static unsigned last_ticks = 0;
        static unsigned last_frame = 0;
        static int total_kb = 0;
        static int avail_kb = 0;
        unsigned ticks = SDL_GetTicks();
        static char buf[8000];
        static char timings_buf[8000];
        char *p = buf;

        if (ticks - last_ticks >= 1000) {
                float elapsed = ((float)ticks - last_ticks);
                float frames = frame - last_frame;

                p += snprintf(p, 8000 - (p-buf),
                                "%d omp, %0.2f chunk/s\n",
                                omp_threads,
                                (float)nr_chunks_generated / (chunk_gen_ticks / 1000.f));

                p += snprintf(p, 8000 - (p-buf),
                                "%.1fm poly/s, shadow: %.1fm (n:%.1fm m:%.1fm f:%.1fm x:%.1fm)\n",
                                1000.f * (float)polys / elapsed / 1000000.f,
                                1000.f * (float)shadow_polys / elapsed / 1000000.f,
                                1000.f * (float)shadow[SHADOW_NEAR].polys / elapsed / 1000000.f,
                                1000.f * (float)shadow[SHADOW_MID].polys / elapsed / 1000000.f,
                                1000.f * (float)(shadow[SHADOW_FAR_A].polys + shadow[SHADOW_FAR_B].polys) / elapsed / 1000000.f,
                                1000.f * (float)(shadow[SHADOW_EXT_A].polys + shadow[SHADOW_EXT_B].polys) / elapsed / 1000000.f);

                // GPU timing display (accumulated averages)
                static float gpu_shadow_n_ms = 0, gpu_shadow_m_ms = 0;
                static float gpu_shadow_f_ms = 0, gpu_shadow_x_ms = 0;
                static float gpu_terrain_ms = 0, gpu_total_ms = 0;
                if (gpu_timestamps_valid && gpu_timestamp_period > 0) {
                        float ns_to_ms = gpu_timestamp_period / 1e6f;
                        gpu_shadow_n_ms = (gpu_timestamps[GPU_TS_SHADOW_N_END] - gpu_timestamps[GPU_TS_FRAME_START]) * ns_to_ms;
                        gpu_shadow_m_ms = (gpu_timestamps[GPU_TS_SHADOW_M_END] - gpu_timestamps[GPU_TS_SHADOW_N_END]) * ns_to_ms;
                        gpu_shadow_f_ms = (gpu_timestamps[GPU_TS_SHADOW_F_END] - gpu_timestamps[GPU_TS_SHADOW_M_END]) * ns_to_ms;
                        gpu_shadow_x_ms = (gpu_timestamps[GPU_TS_SHADOW_X_END] - gpu_timestamps[GPU_TS_SHADOW_F_END]) * ns_to_ms;
                        gpu_terrain_ms = (gpu_timestamps[GPU_TS_TERRAIN_END] - gpu_timestamps[GPU_TS_SHADOW_X_END]) * ns_to_ms;
                        gpu_total_ms = (gpu_timestamps[GPU_TS_FRAME_END] - gpu_timestamps[GPU_TS_FRAME_START]) * ns_to_ms;
                }
                p += snprintf(p, 8000 - (p-buf),
                                "GPU: %.1fms (sN:%.1f sM:%.1f sF:%.1f sX:%.1f terr:%.1f)\n",
                                gpu_total_ms, gpu_shadow_n_ms, gpu_shadow_m_ms,
                                gpu_shadow_f_ms, gpu_shadow_x_ms, gpu_terrain_ms);

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

                last_ticks = ticks;
                last_frame = frame;
                polys = 0;
                shadow_polys = 0;
                for (int i = 0; i < SHADOW_COUNT; i++)
                        shadow[i].polys = 0;

                timer_print(timings_buf, 8000, false);
        }

        if (noisy)
        {
                char xyzbuf[100];
                snprintf(xyzbuf, 100,
                                "X=%0.0f Y=%0.0f Z=%0.0f drawdist=%.0f %sreg %sfast %slock",
                                player[0].pos.x / BS, player[0].pos.y / BS, player[0].pos.z / BS,
                                draw_dist,
                                regulated       ? "" : "no",
                                fast > 1        ? "" : "no",
                                lock_culling    ? "" : "no");

                font_begin(screenw, screenh);
                font_add_text(buf, 0, 0, 0);
                font_add_text(xyzbuf, 0, 0.955f * screenh, 0);
                font_end(1, 1, 1);

                font_begin(screenw, screenh);
                font_add_text(timings_buf, 0.80f * screenw, 0, 0);
                font_end(1, 1, 1);
        }

        if (help_layer == 0 && pframe < 200)
        {
                font_begin(screenw, screenh);
                font_add_text("Press H for help", 20, screenh - 40, 0);
                font_end(1, .5f, 1);
        }

        if (help_layer == 1)
        {
                char *h1 = "WASD\nShift\nCtrl/WW\nSpc/MB4\nLMB  \nRMB  \nWheel\nE          \nZ   \nC            \nH                  \nPress G for more";
                char *h2 = "Move\nSneak\nRun    \nJump   \nBreak\nBuild\nChange held block\nPlace Light\nZoom\nDraw Distance\nHide this help text";
                font_begin(screenw, screenh);
                font_add_text(h1, screenw/100.f, screenh/4.f, 0);
                font_end(1, 0.5, 1);
                font_begin(screenw, screenh);
                font_add_text(h2, screenw/9.f, screenh/4.f, 0);
                font_end(1, 1, 1);
        }

        if (help_layer == 2)
        {
                char *g1 = "~\nQ\nF\nB\nN\nP\nM\nR\nF2\nF3\nF5";
                char *g2 = "Command console\nGo up!\nFast\nSpawn slime\nRev. sun\nFast sun\nShadow mapping\nFixed interval\nLock culling\nFPS, timings, position\nReload shaders";
                font_begin(screenw, screenh);
                font_add_text(g1, screenw/100.f, screenh/4.f, 0);
                font_end(0.5, 1, 1);
                font_begin(screenw, screenh);
                font_add_text(g2, screenw/20.f, screenh/4.f, 0);
                font_end(1, 1, 1);
        }

        // compass
        {
                char compass_buf[20] = {0};
                static char dir[][8] = {
                        "N (+Z)", "NNE", "NE", "ENE", "E (+X)", "ESE", "SE", "SSE", "S (-Z)", "SSW", "SW", "WSW", "W (-X)", "WNW", "NW", "NNW", "N (+Z)",
                };
                int idx = (int)floorf((player[0].yaw + PI / 16.f) / (PI / 8.f));

                snprintf(compass_buf, 20, "%d  %s", (int)(player[0].yaw / PI * 180.f), dir[idx]);
                font_begin(screenw, screenh);
                font_add_text(compass_buf, screenw/2.1f, 0.f, 0);
                font_end(1, 1, 1);
        }

        // for debugging only
        if (alert[0])
        {
                font_begin(screenw, screenh);
                font_add_text(alert, screenw/2.f, screenh/2.f, 0);
                font_end(1, 0.5, 0);
        }

        if (test_lock)
        {
                char banner[400];
                snprintf(banner, sizeof banner,
                        "TEST RUNNING - INPUT LOCKED\n%s\n(press ~ and type 'lock 0' to unlock)",
                        test_lock_msg);
                font_begin(screenw, screenh);
                font_add_text(banner, screenw/2.5f, screenh/20.f, 0);
                font_end(1, 0.6, 0.2);
        }

        console_draw();
}

#endif // BLOCKO_TEST_C_INCLUDED

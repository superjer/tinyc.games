#include "oilchange.h"

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

                p += snprintf(p, 8000 - (p-buf),
                                "vmem %0.0fm used of %0.0fm (%0.0f%% free)\n",
                                (float)(total_kb - avail_kb) / 1000.f,
                                (float)(total_kb)            / 1000.f,
                                ((float)avail_kb / total_kb) * 100.f);

                p += snprintf(p, 8000 - (p-buf),
                                "%.3fm poly/s, %.3f shadow poly/s\n",
                                1000.f * (float)polys / elapsed / 1000000.f,
                                1000.f * (float)shadow_polys / elapsed / 1000000.f);

                p += snprintf(p, 8000 - (p-buf),
                                "%.1f fps\n", 1000.f * frames / elapsed );

                glGetIntegerv(0x9048, &total_kb);
                glGetIntegerv(0x9049, &avail_kb);
                last_ticks = ticks;
                last_frame = frame;
                polys = 0;
                shadow_polys = 0;

                timer_print(timings_buf, 8000);
        }

        if (noisy)
        {
                char xyzbuf[100];
                snprintf(xyzbuf, 100,
                                "X=%0.0f Y=%0.0f Z=%0.0f %svsync %sreg %smsaa %sfast %scull %slock",
                                player[0].pos.x / BS, player[0].pos.y / BS, player[0].pos.z / BS,
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

        // for debugging only
        if (alert[0])
        {
                font_begin(screenw, screenh);
                font_add_text(alert, screenw/2.f, screenh/2.f, 0);
                font_end(1, 0.5, 0);
        }
}

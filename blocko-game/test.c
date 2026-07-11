#include "blocko.c"
#ifndef BLOCKO_TEST_C_INCLUDED
#define BLOCKO_TEST_C_INCLUDED

void debrief()
{
        font_frame_reset();  // Reset font buffer offset for this frame

        if (help_layer == 0 && pframe < 200)
        {
                font_begin(screenw, screenh);
                font_add_text("Press H for help", 20, screenh - 40, 0);
                font_end(1, .5f, 1);
        }

        if (help_layer == 1)
        {
                char *h1 = "WASD\nShift\nCtrl/WW\nSpc/MB4\nLMB  \nRMB  \nWheel\nT   \nC            \nH                  \nPress G for more";
                char *h2 = "Move\nSneak\nRun    \nJump   \nBreak\nBuild\nChange held block\nChat\nDraw Distance\nHide this help text";
                font_begin(screenw, screenh);
                font_add_text(h1, screenw/100.f, screenh/4.f, 0);
                font_end(1, 0.5, 1);
                font_begin(screenw, screenh);
                font_add_text(h2, screenw/9.f, screenh/4.f, 0);
                font_end(1, 1, 1);
        }

        if (help_layer == 2)
        {
                char *g1 = "Q\nF\nB\nN";
                char *g2 = "Go up!\nFast\nSpawn slime\nNoclip";
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
                int idx = (int)floorf((player[my_player].yaw + PI / 16.f) / (PI / 8.f));

                snprintf(compass_buf, 20, "%d  %s", (int)(player[my_player].yaw / PI * 180.f), dir[idx]);
                font_begin(screenw, screenh);
                font_add_text(compass_buf, screenw/2.1f, 0.f, 0);
                font_end(1, 1, 1);
        }

        console_draw();
}

#endif // BLOCKO_TEST_C_INCLUDED

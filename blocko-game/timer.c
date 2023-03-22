#define TIMER_NAMES \
        X(create_hmap), \
        X(update_world), \
        X(update_player), \
        X(step_sunlight), \
        X(step_sunlight_building), \
        X(step_glolight), \
        X(step_glolight_building), \
        X(recalc_corner_lighting), \
        X(rings), \
        X(drawstale), \
        X(buildvbo), \
        X(glBufferData), \
        X(glDrawArrays), \
        X(swapwindow), \
        X(glsetup), \
        X(font_init), \
        X(sun_init), \
        X(scoot),

#include "../common/tinyc.games/timer.c"

#undef TIMER_NAMES

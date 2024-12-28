#define TIMER_NAMES \
        X(SDL_Delay), \
        X(do_events), \
        X(update_player), \
        X(update_particles), \
        X(update_music), \
        X(draw_start), \
        X(draw_menu), \
        X(draw_player), \
        X(draw_particles), \
        X(draw_end), \
        X(SDL_GL_SwapWindow),

#include "../common/tinyc.games/timer.c"

#undef TIMER_NAMES

#define TIMER_NAMES \
        X(SDL_Delay), \
        X(draw_verts), \
        X(draw_actual), \
        X(SDL_GL_SwapWindow),

#include "../common/tinyc.games/timer.c"

#undef TIMER_NAMES

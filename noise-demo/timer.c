#define TIMER_NAMES \
        X(SDL_Delay), \
        X(create_verts), \
        X(upload_verts), \
        X(draw_verts), \
        X(vulkan_acquire_next), \
        X(vulkan_submit),

#include "../common/tinyc.games/timer.c"

#undef TIMER_NAMES

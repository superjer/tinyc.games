#define TIMER_NAMES \
        X(create_hmap), \
        X(update_world), \
        X(update_player), \
        X(step_sunlight), \
        X(step_sunlight_building), \
        X(step_glolight), \
        X(step_glolight_building), \
        X(recalc_corner_lighting), \
        X(gpu_sync), \
        X(shadow_calc), \
        X(shadow_render), \
        X(frame_setup), \
        X(collect_dirty), \
        X(draw_cached), \
        X(build_meshes), \
        X(gpu_upload), \
        X(draw_terrain), \
        X(frame_submit), \
        X(glsetup), \
        X(font_init), \
        X(cursor_init), \
        X(sun_init), \
        X(scoot),

#include "../common/tinyc.games/timer.c"

#undef TIMER_NAMES

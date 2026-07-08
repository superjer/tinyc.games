#define TIMER_NAMES \
        X(create_hmap), \
        X(update_world), \
        X(update_player), \
        X(update_mobs), \
        X(step_sunlight), \
        X(step_sunlight_building), \
        X(step_glolight), \
        X(step_glolight_building), \
        X(recalc_corner_lighting), \
        X(gpu_sync), \
        X(draw_start), \
        X(shadow_calc), \
        X(shadow_render), \
        X(frame_setup), \
        X(sync_w_terrain_gen), \
        X(build_meshes), \
        X(gpu_upload), \
        X(upload_ubo), \
        X(draw_terrain), \
        X(frame_submit), \
        X(vksetup), \
        X(font_init), \
        X(cursor_init), \
        X(sun_init), \
        X(scoot),

#include "../common/tinyc.games/timer.c"

#undef TIMER_NAMES

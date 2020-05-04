#include <stdio.h>

#define TIMER(name) {                                                           \
        long long int now = SDL_GetTicks();                                     \
        timer_times[ timer_curr_id ] += now - timer_then;                       \
        timer_curr_id = timer_ ## name;                                         \
        timer_then = now;                                                       \
}

// FIXME: push id onto stack if re-entering
#define TIMECALL(f, args) {                                                     \
        long long int now = SDL_GetTicks();                                     \
        timer_times[ timer_curr_id ] += now - timer_then;                       \
        timer_then = now;                                                       \
        (f)args;                                                                \
        now = SDL_GetTicks();                                                   \
        timer_times[ timer_ ## f ] += now - timer_then;                         \
        timer_curr_id = timer_;                                                 \
        timer_then = now;                                                       \
}

#define NAMES \
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
        X(sun_init),

enum timernames {
        #define X(x) timer_ ## x
        NAMES
        #undef X
        timer_
};

char timernamesprint[][80] = {
        #define X(x) #x
        NAMES
        #undef X
        "uncounted"
};

#undef NAMES

int timer_curr_id = timer_;
long long int timer_then = 0;
long long int timer_times[timer_ + 1] = { 0 };

void timer_print(char *buf, size_t n)
{
        char *p = buf;
        int i = 0;
        long long int sum = 0;

        for (i = 0; i <= timer_; i++)
                sum += timer_times[i];

        for (i = 0; i <= timer_; i++)
        {
                float secs = (float)timer_times[i] / 1000.f;
                float pct = 100.f * (float)timer_times[i] / sum;
                if (pct >= 0.5f || secs >= 0.1f)
                        p += snprintf(p, n - (p-buf),
                                        "%6.1f  %2.0f%%  %s\n", secs, pct, timernamesprint[i]);
        }
}

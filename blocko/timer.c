#include <stdio.h>

#define TIMER(name) { timer_times[ timer_ ## name ] -= SDL_GetTicks(); }
#define TIMERSTOP(name) { timer_times[ timer_ ## name ] += SDL_GetTicks(); }

#define NAMES \
        X(buildvbo), \
        X(glBufferData), \
        X(glDrawArrays), \
        X(swapwindow),

enum timernames {
        #define X(x) timer_ ## x
        NAMES
        #undef X
        timer_names_end
};

char timernamesprint[][80] = {
        #define X(x) #x
        NAMES
        #undef X
        "timer_names_end"
};

#undef NAMES

long long int timer_times[100] = { 0 };

void timer_print()
{
        int i = 0;
        for (i = 0; i < timer_names_end; i++)
        {
                printf("%8.3f %s\n", (float)timer_times[i] / 1000.f, timernamesprint[i]);
        }
}

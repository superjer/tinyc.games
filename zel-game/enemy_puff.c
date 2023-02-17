#include "zel.c"
#ifndef ENEMY_PUFF_C
#define ENEMY_PUFF_C
#define e (enemy[n])

void update_puff(int n)
{
        e.pos.x += e.vel.x;
        e.pos.y += e.vel.y;
}

#undef e
#endif

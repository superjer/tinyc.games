#include "zel.c"
#ifndef ZEL_ENEMY_PUFF_C
#define ZEL_ENEMY_PUFF_C
#define e (enemy[n])

void update_puff(int n)
{
        e.pos.x += e.vel.x;
        e.pos.y += e.vel.y;
}

#undef e
#endif // ZEL_ENEMY_PUFF_C

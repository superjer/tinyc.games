#include "zel.c"
#ifndef ZEL_ENEMY_WRENCH_C
#define ZEL_ENEMY_WRENCH_C
#define e (enemy[n])

void update_wrench(int n)
{

        if (e.stun > 0)
                e.stun--;

        if (got_stabbed(n) && e.state == READY)
        {
                e.state = REFLECTED;
                e.vel.x = 0;
                e.vel.y = -15;
        }

        if (stopped(n))
        {
                if (e.state == REFLECTED)
                {
                        become_puff(n);
                        return;
                }

                e.vel.x = rand() % 2 ? -2 : 2;
                e.vel.y = rand() % 2 ? -2 : 2;
        }

        // chain destroy after toolbox defeated
        if (e.state == ORPHANED && e.stun == 0)
                become_puff(n);

        SDL_Rect newpos = e.pos;
        newpos.x += e.vel.x;
        newpos.y += e.vel.y;
        if (inner_collide(newpos))
        {
                e.vel.x = 0;
                e.vel.y = 0;
        }
        else
        {
                e.pos = newpos;
        }

}

#undef e
#endif // ZEL_ENEMY_WRENCH_C

#include "zel.c"
#ifndef ENEMY_PIG_C
#define ENEMY_PIG_C
#define e (enemy[n])

void update_pig(int n)
{
        if (got_stabbed(n))
        {
                e.stun = STUN;

                if (--e.hp <= 0)
                {
                        become_puff(n);
                        return;
                }
                e.reel = 30;
                e.reeldir = player[0].dir;
        }

        if (stopped(n) && pct(10))
        {
                e.vel.x = pct(50) ? 2 : -2;
                e.vel.y = pct(50) ? 2 : -2;
        }

        if (e.reel)
        {
                e.reel--;
                reel(n);
        }
        else
        {
                SDL_Rect newpos = e.pos;
                newpos.x += e.vel.x;
                newpos.y += e.vel.y;
                if (world_collide(newpos) || edge_collide(newpos))
                        stop(n);
                else
                        e.pos = newpos;
        }

}

#undef e
#endif

#include "zel.c"
#ifndef ENEMY_BOARD_C
#define ENEMY_BOARD_C
#define e (enemy[n])

void update_board(int n)
{
        if (got_stabbed(n))
        {
                if (--e.hp <= 0)
                {
                        become_puff(n);
                        return;
                }

                e.stun = STUN;
                e.reel = 30;
                e.reeldir = player[0].dir;
        }

        // on a tile exactly and stopped, 10%?
        if (stopped(n) || (pct(10) && e.pos.x % BS2 == 0 && e.pos.y % BS2 == 0))
        {
                if (rand()%2 == 0)
                        e.vel.x = pct(50) ? 2 : -2;
                else
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

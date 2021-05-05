#include "zel.c"
#ifndef ENEMY_SCREW_C
#define ENEMY_SCREW_C
#define e (enemy[n])

void update_screw(int n)
{
        if (got_stabbed(n))
        {
                e.stun = SCREW_STUN; // allow player to screw quickly
                if (e.state == READY)
                {
                        e.state = STUCK;
                }
                else if (e.state == STUCK)
                {
                        e.state = NEUTRALIZED;
                        e.harmful = false;
                }

        }

        if (e.state != READY)
        {
                stop(n);
        }
        else if (frame % 3 == 0)
        {
                stop(n);
                if (pct(50) == 0)
                        e.vel.x = pct(50) ? 2 : -2;
                else
                        e.vel.y = pct(50) ? 2 : -2;
        }

        SDL_Rect newpos = e.pos;
        newpos.x += e.vel.x;
        newpos.y += e.vel.y;
        if (world_collide(newpos) || edge_collide(newpos))
                stop(n);
        else
                e.pos = newpos;
}

#undef e
#endif

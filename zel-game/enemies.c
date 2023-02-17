#include "zel.c"
#ifndef ENEMIES_C
#define ENEMIES_C

enum enemy_states {READY, JUMP, LAND, BARF, SHUT, HURT,
                   REFLECTED, ORPHANED,
                   STUCK, NEUTRALIZED};

#define STUN 50
#define SCREW_STUN 20

#define e (enemy[n])

int got_stabbed(int n)
{
        return player[0].state == PL_STAB
                && e.stun == 0
                && e.hp > 0
                && collide(player[0].hitbox, e.pos);
}

void stop(int n)
{
        e.vel.x = 0;
        e.vel.y = 0;
}

int stopped(int n)
{
        return e.vel.x == 0 && e.vel.y == 0;
}

void become_puff(int n)
{
        e.type = PUFF;
        e.frame = 0;
        e.harmless = true;
        e.vel.x = 0;
        e.vel.y = 0;
        e.pos.w = BS;
        e.pos.h = BS;
}

void reel(int n)
{
        SDL_Rect newpos = e.pos;
        switch (e.reeldir)
        {
                case NORTH: newpos.y -= 10; break;
                case WEST:  newpos.x -= 10; break;
                case EAST:  newpos.x += 10; break;
                case SOUTH: newpos.y += 10; break;
        }

        if (!world_collide(newpos) && e.reel != 0)
        {
                e.pos = newpos;
        }
        else
        {
                e.pos.x = ((e.pos.x + BS2 - 1) / BS2) * BS2;
                e.pos.y = ((e.pos.y + BS2 - 1) / BS2) * BS2;
        }
}

#undef e

#include "enemy_toolbox.c"
#include "enemy_pig.c"
#include "enemy_screw.c"
#include "enemy_board.c"
#include "enemy_wrench.c"
#include "enemy_puff.c"

void update_enemies()
{
        for(int i = 0; i < NR_ENEMIES; i++)
        {
                if (!enemy[i].exists) continue;

                if (enemy[i].stun > 0) enemy[i].stun--;

                if (enemy[i].freeze > 0)
                {
                        enemy[i].freeze--;
                        continue;
                }

                switch (enemy[i].type)
                {
                        case TOOLBOX:    update_toolbox(i); break;
                        case PIG:        update_pig(i);     break;
                        case SCREW:      update_screw(i);   break;
                        case BOARD:      update_board(i);   break;
                        case WRENCH:     update_wrench(i);  break;
                        case PIPEWRENCH: update_wrench(i);  break;
                        case PUFF:       update_puff(i);    break;
                }
        }
}

#endif

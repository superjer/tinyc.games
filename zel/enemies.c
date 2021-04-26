#include "zel.c"
#ifndef ENEMIES_C
#define ENEMIES_C

#define SCREW_STUN 20

enum toolboxstates {TB_READY, TB_JUMP, TB_LAND, TB_OPEN, TB_SHUT, TB_HURT};

void toolbox(struct enemy *e)
{
        switch(e->state)
        {
                case TB_READY:
                        if(rand()%40 == 0)
                        {
                                e->state = TB_JUMP;
                                e->vel.y = -20;
                        }
                        else if(rand()%40 == 0)
                        {
                                e->state = TB_OPEN;
                                e->frame = 0;
                        }
                        break;
                case TB_JUMP:
                        e->vel.y += 2;
                        if(e->vel.y >= 20)
                        {
                                e->state = TB_LAND;
                                e->vel.y = 0;
                        }
                        break;
                case TB_LAND:
                        if(frame%4 == 0)
                                e->frame = rand()%2 + 2;

                        if(rand()%40 == 0)
                        {
                                e->state = TB_READY;
                                e->frame = 0;
                        }
                        break;
                case TB_OPEN:
                        if(e->frame == 0)
                        {
                                int speed = rand()%10 ? 7 : 9;
                                if(e->pos.x < 3*BS)
                                        e->vel.x = speed;
                                else if(e->pos.x > W - 7*BS)
                                        e->vel.x = -speed;
                                else
                                        e->vel.x = rand()%2 ? speed : -speed;
                                e->frame = 1;
                        }
                        if(frame%8 == 0 && rand()%3 == 0)
                        {
                                int slot;
                                if(find_free_slot(&slot))
                                {
                                        enemy[slot].type = rand()%2 ? WRENCH : PIPEWRENCH;
                                        enemy[slot].hp = 2;
                                        enemy[slot].alive = 1;
                                        enemy[slot].pos = (SDL_Rect){
                                                e->pos.x + BS2,
                                                e->pos.y,
                                                BS,
                                                BS2
                                        };
                                        enemy[slot].vel.x = rand()%2 ? 2 : -2;
                                        enemy[slot].vel.y = 2;
                                }
                        }
                        if(e->vel.x == 0)
                        {
                                e->frame = 6;
                                e->state = TB_SHUT;
                        }
                        else if(frame%20 == 0)
                        {
                                if(e->vel.x > 0)
                                        e->vel.x--;
                                else
                                        e->vel.x++;
                        }
                        break;
                case TB_SHUT:
                        if(frame%10 == 0 && rand()%3 == 0)
                        {
                                e->state = TB_READY;
                                e->frame = 0;
                        }
                        break;
                case TB_HURT:
                        if(e->vel.y >= 0)
                        {
                                e->state = TB_OPEN;
                                e->frame = 0;
                        }
                        else if(frame%4 == 0)
                        {
                                e->vel.y++;
                        }
                        break;
        }

        if (e->stun == 0) for (int i = 0; i < NR_ENEMIES; i++)
        {
                if (!enemy[i].alive)
                        continue;
                if (enemy[i].type != WRENCH && enemy[i].type != PIPEWRENCH)
                        continue;
                if (enemy[i].hp != 1)
                        continue;
                if (!collide(e->pos, enemy[i].pos))
                        continue;
                e->stun = 50;
                e->hp--;
                e->state = TB_HURT;
                enemy[i].type = PUFF;
                enemy[i].frame = 0;
                enemy[i].vel.y = 0;
        }
}

void update_enemies()
{
        for(int i = 0; i < NR_ENEMIES; i++)
        {
                struct enemy *e = enemy + i;
                if(!enemy[i].alive)
                        continue;

                if(enemy[i].stun > 0)
                        enemy[i].stun--;

                if(enemy[i].freeze > 0)
                {
                        enemy[i].freeze--;
                        continue;
                }

                if(player[0].state == PL_STAB &&
                                enemy[i].type != PUFF &&
                                enemy[i].stun == 0 &&
                                enemy[i].hp > 0 &&
                                collide(player[0].hitbox, enemy[i].pos))
                {
                        // allow player to screw quickly
                        enemy[i].stun = (enemy[i].type == SCREW ? SCREW_STUN : 50);

                        if(--enemy[i].hp > 0)
                        {
                                if(enemy[i].type == TOOLBOX)
                                {
                                        e->vel.y = -5;
                                        e->state = TB_HURT;
                                        e->frame = 5;
                                }
                                else if (enemy[i].type == WRENCH || enemy[i].type == PIPEWRENCH)
                                {
                                        e->vel.y = -15;
                                        e->vel.x = 0;
                                }
                                else if (enemy[i].type == SCREW)
                                {
                                        ;
                                }
                                else
                                {
                                        enemy[i].reel = 30;
                                        enemy[i].reeldir = player[0].dir;
                                }
                        }
                        else
                        {
                                if(enemy[i].type == TOOLBOX) for(int j = 0; j < NR_ENEMIES; j++)
                                {
                                        enemy[j].type = PUFF;
                                        enemy[j].frame = 0;
                                }

                                if (enemy[i].type != SCREW)
                                {
                                        enemy[i].type = PUFF;
                                        enemy[i].frame = 0;
                                        enemy[i].vel.x = 0;
                                        enemy[i].vel.y = 0;
                                }
                        }
                }

                switch(enemy[i].type)
                {
                        case WRENCH:
                        case PIPEWRENCH:
                                if(enemy[i].vel.x == 0 && enemy[i].vel.y == 0)
                                {
                                        if (enemy[i].hp < 2)
                                        {
                                                enemy[i].type = PUFF;
                                                enemy[i].frame = 0;
                                        }
                                        else
                                        {
                                                enemy[i].vel.x = rand()%2 ? -2 : 2;
                                                enemy[i].vel.y = rand()%2 ? -2 : 2;
                                        }
                                }
                                break;
                        case PIG:
                                if(enemy[i].vel.x == 0 && rand()%10 == 0)
                                {
                                        enemy[i].vel.x = (rand()%2 * 4) - 2;
                                        enemy[i].vel.y = (rand()%2 * 4) - 2;
                                }
                                break;
                        case SCREW:
                                if(enemy[i].hp < 2)
                                {
                                        enemy[i].vel.x = 0;
                                        enemy[i].vel.y = 0;
                                }
                                else if(frame%3 == 0)
                                {
                                        if(rand()%2 == 0)
                                        {
                                                enemy[i].vel.x = (rand()%2 * 4) - 2;
                                                enemy[i].vel.y = 0;
                                        }
                                        else
                                        {
                                                enemy[i].vel.y = (rand()%2 * 4) - 2;
                                                enemy[i].vel.x = 0;
                                        }
                                }
                                break;
                        case BOARD:
                                if((enemy[i].vel.x == 0 && enemy[i].vel.y == 0) ||
                                                (rand()%10 == 0 &&
                                                 enemy[i].pos.x % BS2 == 0 &&
                                                 enemy[i].pos.y % BS2 == 0))
                                {
                                        if(rand()%2 == 0)
                                        {
                                                enemy[i].vel.x = (rand()%2 * 4) - 2;
                                                enemy[i].vel.y = 0;
                                        }
                                        else
                                        {
                                                enemy[i].vel.y = (rand()%2 * 4) - 2;
                                                enemy[i].vel.x = 0;
                                        }
                                }
                                break;
                        case TOOLBOX:
                                toolbox(enemy + i);
                                break;
                }

                SDL_Rect newpos = enemy[i].pos;
                if(enemy[i].reel)
                {
                        enemy[i].reel--;
                        switch(enemy[i].reeldir)
                        {
                                case NORTH: newpos.y -= 10; break;
                                case WEST:  newpos.x -= 10; break;
                                case EAST:  newpos.x += 10; break;
                                case SOUTH: newpos.y += 10; break;
                        }
                        if(!world_collide(newpos) && enemy[i].reel != 0)
                        {
                                enemy[i].pos = newpos;
                        }
                        else
                        {
                                enemy[i].pos.x = ((enemy[i].pos.x + BS2 - 1) / BS2) * BS2;
                                enemy[i].pos.y = ((enemy[i].pos.y + BS2 - 1) / BS2) * BS2;
                        }
                }
                else
                {
                        newpos.x += enemy[i].vel.x;
                        newpos.y += enemy[i].vel.y;
                        if(world_collide(newpos) || edge_collide(newpos))
                        {
                                enemy[i].vel.x = 0;
                                enemy[i].vel.y = 0;
                        }
                        else
                        {
                                enemy[i].pos = newpos;
                        }
                }
        }
}

#endif

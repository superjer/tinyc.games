#include "zel.c"
#ifndef ZEL_ENEMY_TOOLBOX_C
#define ZEL_ENEMY_TOOLBOX_C
#define e (enemy[n])

void update_toolbox(int n)
{
        if (e.state == READY)
        {
                if (pct(2))
                {
                        e.state = JUMP;
                        e.vel.y = -20;
                }
                else if (pct(2))
                {
                        e.state = BARF;
                        e.frame = 0;
                }
                else if (pct(2))
                {
                        e.frame = 4;
                }
        }
        else if (e.state == JUMP)
        {
                e.vel.y += 2;
                if (e.vel.y >= 20)
                {
                        e.state = LAND;
                        e.vel.y = 0;
                }
        }
        else if (e.state == LAND)
        {
                if (global_frame % 4 == 0)
                        e.frame = pct(50) ? 2 : 3;

                if (pct(2))
                {
                        e.state = READY;
                        e.frame = 0;
                }
        }
        else if (e.state == BARF)
        {
                if (e.frame == 0)
                {
                        int speed = pct(10) ? 7 : 9;
                        if (e.pos.x < 3 * BS)
                                e.vel.x = speed;
                        else if (e.pos.x > W - 7 * BS)
                                e.vel.x = -speed;
                        else
                                e.vel.x = pct(50) ? speed : -speed;
                        e.frame = 1;
                }

                if (global_frame % 20 == 0 && pct(60))
                {
                        int slot;
                        if (find_free_slot(&slot))
                        {
                                memset(enemy + slot, 0, sizeof *enemy);
                                enemy[slot].type = pct(60) ? WRENCH : PIPEWRENCH;
                                enemy[slot].exists = true;
                                enemy[slot].hp = 2;
                                enemy[slot].state = READY;
                                enemy[slot].pos = (SDL_Rect){e.pos.x + BS2, e.pos.y, BS, BS};
                                enemy[slot].vel = (struct point){pct(50) ? 2 : -2, 2};
                        }
                }

                if (e.vel.x == 0)
                {
                        e.frame = 6;
                        e.state = SHUT;
                }
                else if (global_frame % 20 == 0)
                {
                        if (e.vel.x > 0)
                                e.vel.x--;
                        else
                                e.vel.x++;
                }
        }
        else if (e.state == SHUT)
        {
                if (global_frame % 10 == 0 && pct(33))
                {
                        e.state = READY;
                        e.frame = 0;
                }
        }
        else if (e.state == HURT)
        {
                if (e.stun == 0)
                {
                        e.state = BARF;
                        e.frame = 0;
                        stop(n);
                }
                else if (global_frame % 6 == 0)
                {
                        e.vel.y++;
                }
        }

        // see if the toolbox gets hit by a reflected wrench
        if (e.stun == 0) for (int i = 0; i < NR_ENEMIES; i++)
        {
                if (!enemy[i].exists)
                        continue;
                if (enemy[i].type != WRENCH && enemy[i].type != PIPEWRENCH)
                        continue;
                if (enemy[i].state != REFLECTED)
                        continue;
                if (!collide(e.pos, enemy[i].pos))
                        continue;
                e.stun = 36;
                e.state = HURT;
                e.frame = 5;
                e.vel.y = -3;
                become_puff(i);

                if (--e.hp == 0)
                {
                        e.pos.x += BS2;
                        e.pos.w = BS;
                        e.pos.h = BS2;
                        become_puff(n);

                        // destroy all projectiles?
                        int amt = 60;
                        for (int j = 0; j < NR_ENEMIES; j++)
                        {
                                if (enemy[j].type != WRENCH && enemy[j].type != PIPEWRENCH)
                                        continue;
                                enemy[j].harmless = true;
                                enemy[j].state = ORPHANED;
                                enemy[j].stun = amt;
                                amt += 20;
                        }
                }
        }

        SDL_Rect newpos = e.pos;
        newpos.x += e.vel.x;
        newpos.y += e.vel.y;

        if (inner_collide(newpos))
                stop(n);
        else
                e.pos = newpos;
}

#undef e
#endif // ZEL_ENEMY_TOOLBOX_C

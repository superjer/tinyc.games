#include "zel.c"
#ifndef PLAYER_C
#define PLAYER_C

enum playerstates {PL_NORMAL, PL_STAB, PL_HURT, PL_DYING, PL_DEAD};

#define p (player[i])

//return false iff we couldn't actually move
int move_player(int i, int velx, int vely, int fake_it, int weave)
{
        SDL_Rect newpos = p.pos;

        if(p.reel)
        {
                p.reel--;
                switch(p.reeldir)
                {
                        case NORTH: newpos.y -= PLYR_SPD * 2; break;
                        case WEST:  newpos.x -= PLYR_SPD * 2; break;
                        case EAST:  newpos.x += PLYR_SPD * 2; break;
                        case SOUTH: newpos.y += PLYR_SPD * 2; break;
                }
        }
        else
        {
                newpos.x += velx;
                newpos.y += vely;
        }

        int already_stuck = 0;
        int would_be_stuck = 0;

        if(world_collide(p.pos) || noclip)
                already_stuck = 1;

        if(world_collide(newpos))
                would_be_stuck = 1;

        // see if we can weave laterally instead?
        if(!already_stuck && would_be_stuck && !fake_it && weave) for(int k = 0; k < LATERAL_STEPS; k++)
        {
                SDL_Rect latpos = newpos;

                if(velx > 0) latpos.x -= velx; // don't move positive,
                if(vely > 0) latpos.y -= vely; // handled by the growing

                if(velx)
                {
                        latpos.w += abs(velx); // grow box
                        latpos.y = newpos.y + k * abs(velx);
                        if(!world_collide(latpos))
                        {
                                //this is the winning position!
                                //move one step laterally!
                                p.pos.y += abs(velx);
                                return true;
                        }
                        latpos.y = newpos.y - k * abs(velx);
                        if(!world_collide(latpos))
                        {
                                p.pos.y -= abs(velx);
                                return true;
                        }
                }
                else if(vely)
                {
                        latpos.h += abs(vely); // grow box
                        latpos.x = newpos.x + k * abs(vely);
                        if(!world_collide(latpos))
                        {
                                p.pos.x += abs(vely);
                                return true;
                        }
                        latpos.x = newpos.x - k * abs(vely);
                        if(!world_collide(latpos))
                        {
                                p.pos.x -= abs(vely);
                                return true;
                        }
                }
        }

        if(!would_be_stuck || already_stuck)
        {
                if(!fake_it) p.pos = newpos;
                return true;
        }

        //don't move, but remember intent to move
        p.ylast = vely ? 1 : 0;
        return false;
}

void update_player(int i)
{
        if (p.stun > 0)
                p.stun--;

        if (p.state == PL_DEAD)
        {
                if(p.stun < 1)
                        new_game();
                return;
        }

        if (p.state == PL_DYING)
        {
                if (global_frame % 6 == 0)
                        p.dir = (p.dir+1) % 4;

                if (p.stun < 1)
                {
                        p.exists = false;
                        p.state = PL_DEAD;
                        p.stun = 100;
                }

                return;
        }

        if(p.state == PL_STAB)
        {
                p.hitbox = p.pos;
                p.hitbox.x -= 2;
                p.hitbox.y -= 2 + BS2;
                p.hitbox.w += 4;
                p.hitbox.h += 4 + BS2;
                switch(p.dir)
                {
                        case WEST:  p.hitbox.x -= BS; // fallthru
                        case EAST:  p.hitbox.w += BS; break;
                        case NORTH: p.hitbox.y -= BS; // fallthru
                        case SOUTH: p.hitbox.h += BS; break;
                }

                if(--p.delay <= 0)
                {
                        p.delay = 0;
                        p.state = PL_NORMAL;
                }
        }
        else if(!p.vel.x ^ !p.vel.y) // moving only one direction
        {
                move_player(i, p.vel.x, p.vel.y, false, true);
        }
        else if((p.ylast || !p.vel.x) && p.vel.y)
        {
                //only move 1 direction, but try the most recently pressed first
                int fake_it = move_player(i, 0, p.vel.y, false, false);
                move_player(i, p.vel.x, 0, fake_it, false);
        }
        else
        {
                int fake_it = move_player(i, p.vel.x, 0, false, false);
                move_player(i, 0, p.vel.y, fake_it, false);
        }

        //check for enemy collisions
        for (int n = 0; n < NR_ENEMIES; n++)
        {
                if (!p.exists)                     continue;
                if (!enemy[n].exists)              continue;
                if (p.state == PL_DYING)           continue;
                if (p.stun)                        continue;
                if (enemy[n].stun)                 continue;
                if (enemy[n].freeze)               continue;
                if (enemy[n].harmless)             continue;
                if (!collide(p.pos, enemy[n].pos)) continue;

                if (--p.hp <= 0)
                {
                        p.state = PL_DYING;
                        p.stun = 100;
                        continue;
                }

                p.stun = 50;
                p.reel = 5;
                switch (p.dir)
                {
                        case NORTH: p.reeldir = SOUTH; break;
                        case SOUTH: p.reeldir = NORTH; break;
                        case EAST:  p.reeldir = WEST;  break;
                        case WEST:  p.reeldir = EAST;  break;
                }
        }

        // check for entering cave, etc
        if (tiles[ytileof(p.pos)][xtileof(p.pos)] == CAVE)
                enter_dungeon();

        // check for leaving screen
        if (p.pos.x < 0)
                screen_scroll(-1, 0);
        else if (p.pos.x > W - PLYR_W)
                screen_scroll(1, 0);
        else if (p.pos.y < PLYR_H)
                screen_scroll(0, -1);
        else if (p.pos.y > H - PLYR_H)
                screen_scroll(0, 1);

        #undef p
}

#endif

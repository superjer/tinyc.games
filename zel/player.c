enum playerstates {PL_NORMAL, PL_STAB, PL_HURT, PL_DYING, PL_DEAD};

//return 0 iff we couldn't actually move
int move_player(int velx, int vely, int fake_it, int weave)
{
        SDL_Rect newpos = player[0].pos;

        if(player[0].reel)
        {
                player[0].reel--;
                switch(player[0].reeldir)
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

        if(world_collide(player[0].pos) || noclip)
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
                                player[0].pos.y += abs(velx);
                                return 1;
                        }
                        latpos.y = newpos.y - k * abs(velx);
                        if(!world_collide(latpos))
                        {
                                //this is the winning position!
                                //move one step laterally!
                                player[0].pos.y -= abs(velx);
                                return 1;
                        }
                }
                else if(vely)
                {
                        latpos.h += abs(vely); // grow box
                        latpos.x = newpos.x + k * abs(vely);
                        if(!world_collide(latpos))
                        {
                                //this is the winning position!
                                //move one step laterally!
                                player[0].pos.x += abs(vely);
                                return 1;
                        }
                        latpos.x = newpos.x - k * abs(vely);
                        if(!world_collide(latpos))
                        {
                                //this is the winning position!
                                //move one step laterally!
                                player[0].pos.x -= abs(vely);
                                return 1;
                        }
                }
        }

        if(!would_be_stuck || already_stuck)
        {
                if(!fake_it) player[0].pos = newpos;
                return 1;
        }

        //don't move, but remember intent to move
        player[0].ylast = vely ? 1 : 0;
        return 0;
}

void update_player()
{
        struct player *p = player + 0;

        if(player[0].stun > 0)
                player[0].stun--;

        if(player[0].state == PL_DEAD)
        {
                if(player[0].stun < 1)
                        new_game();
                return;
        }

        if(player[0].state == PL_DYING)
        {
                if(frame%6 == 0)
                        player[0].dir = (player[0].dir+1) % 4;

                if(player[0].stun < 1)
                {
                        player[0].alive = 0;
                        player[0].state = PL_DEAD;
                        player[0].stun = 100;
                }

                return;
        }

        if(p->state == PL_STAB)
        {
                p->hitbox = p->pos;
                p->hitbox.x -= 2;
                p->hitbox.y -= 2 + BS2;
                p->hitbox.w += 4;
                p->hitbox.h += 4 + BS2;
                switch(p->dir)
                {
                        case WEST:  p->hitbox.x -= BS;
                        case EAST:  p->hitbox.w += BS; break;
                        case NORTH: p->hitbox.y -= BS;
                        case SOUTH: p->hitbox.h += BS; break;
                }

                if(--p->delay <= 0)
                {
                        p->delay = 0;
                        p->state = PL_NORMAL;
                }
        }
        else if(!p->vel.x ^ !p->vel.y) // moving only one direction
        {
                move_player(p->vel.x, p->vel.y, 0, 1);
        }
        else if((p->ylast || !p->vel.x) && p->vel.y)
        {
                //only move 1 direction, but try the most recently pressed first
                int fake_it = move_player(0, p->vel.y, 0, 0);
                move_player(p->vel.x, 0, fake_it, 0);
        }
        else
        {
                int fake_it = move_player(p->vel.x, 0, 0, 0);
                move_player(0, p->vel.y, fake_it, 0);
        }

        //check for enemy collisions
        for(int i = 0; i < NR_ENEMIES; i++)
        {
                if(player[0].alive && enemy[i].alive &&
                                player[0].state != PL_DYING &&
                                player[0].stun < 1 &&
                                enemy[i].stun < 1 &&
                                enemy[i].hp > 0 &&
                                enemy[i].type != PUFF &&
                                collide(player[0].pos, enemy[i].pos))
                {
                        if(--player[0].hp <= 0)
                        {
                                player[0].state = PL_DYING;
                                player[0].stun = 100;
                        }
                        else
                        {
                                player[0].stun = 50;
                                player[0].reel = 5;
                                switch (player[0].dir)
                                {
                                        case NORTH: player[0].reeldir = SOUTH; break;
                                        case SOUTH: player[0].reeldir = NORTH; break;
                                        case EAST:  player[0].reeldir = WEST; break;
                                        case WEST:  player[0].reeldir = EAST; break;
                                }
                        }
                }
        }

        //check for leaving screen
        if(p->pos.x <= 4)
                screen_scroll(-1, 0);
        else if(p->pos.x >= W - PLYR_W - 4)
                screen_scroll(1, 0);

        if(p->pos.y <= 4)
                screen_scroll(0, -1);
        else if(p->pos.y >= H - PLYR_H - 4)
                screen_scroll(0, 1);
}

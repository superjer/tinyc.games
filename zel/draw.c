#include "zel.c"
#ifndef DRAW_C
#define DRAW_C

void draw_doors_lo()
{
        if (!inside) return;

        SDL_Rect src, dest;
        int r = roomy*DUNW + roomx; // current room coordinate
        int *doors = rooms[r].doors;

        //draw right edge
        src  = (SDL_Rect){11*20, 4*20, 4*20, 3*20};
        dest = (SDL_Rect){11*BS, 4*BS, 4*BS, 3*BS};
        SDL_RenderCopy(renderer, edgetex[doors[EAST]], &src, &dest);

        //draw top edge
        src  = (SDL_Rect){6*20, 0*20, 3*20, 4*20};
        dest = (SDL_Rect){6*BS, 0*BS, 3*BS, 4*BS};
        SDL_RenderCopy(renderer, edgetex[doors[NORTH]], &src, &dest);

        //draw left edge
        src  = (SDL_Rect){0*20, 4*20, 4*20, 3*20};
        dest = (SDL_Rect){0*BS, 4*BS, 4*BS, 3*BS};
        SDL_RenderCopy(renderer, edgetex[doors[WEST]], &src, &dest);

        //draw bottom edge
        src  = (SDL_Rect){6*20, 7*20, 3*20, 4*20};
        dest = (SDL_Rect){6*BS, 7*BS, 3*BS, 4*BS};
        SDL_RenderCopy(renderer, edgetex[doors[SOUTH]], &src, &dest);
}

void draw_doors_hi()
{
        if (!inside) return;

        SDL_Rect src, dest;
        int r = roomy*DUNW + roomx; // current room coordinate
        int *doors = rooms[r].doors;

        //draw right edge ABOVE
        src  = (SDL_Rect){14*20, 4*20, 1*20, 3*20};
        dest = (SDL_Rect){14*BS, 4*BS, 1*BS, 3*BS};
        SDL_RenderCopy(renderer, edgetex[doors[EAST]], &src, &dest);

        //draw top edge ABOVE
        src  = (SDL_Rect){6*20, 0*20, 3*20, 1*20};
        dest = (SDL_Rect){6*BS, 0*BS, 3*BS, 1*BS};
        SDL_RenderCopy(renderer, edgetex[doors[NORTH]], &src, &dest);

        //draw left edge ABOVE
        src  = (SDL_Rect){0*20, 4*20, 1*20, 3*20};
        dest = (SDL_Rect){0*BS, 4*BS, 1*BS, 3*BS};
        SDL_RenderCopy(renderer, edgetex[doors[WEST]], &src, &dest);

        //draw bottom edge ABOVE
        src  = (SDL_Rect){6*20, 10*20, 3*20, 1*20};
        dest = (SDL_Rect){6*BS, 10*BS, 3*BS, 1*BS};
        SDL_RenderCopy(renderer, edgetex[doors[SOUTH]], &src, &dest);
}

void draw_clipping_boxes()
{
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
        for(int x = 0; x < TILESW; x++) for(int y = 0; y < TILESH; y++)
        {
                int t = tiles[y][x];
                if(t <= LASTSOLID)
                        SDL_RenderFillRect(renderer, &(SDL_Rect){BS*x+1, BS*y+1, BS-1, BS-1});
                else if(t == HALFCLIP)
                        SDL_RenderFillRect(renderer, &(SDL_Rect){BS*x+1, BS*y+1, BS-1, BS2-1});
        }

        SDL_SetRenderDrawColor(renderer, 255, 80, 80, 255);
        for(int i = 0; i < NR_PLAYERS; i++)
        {
                if(player[i].state == PL_STAB && player[i].alive)
                        SDL_RenderFillRect(renderer, &player[i].hitbox);
        }
}

void draw_map()
{
        int x, y, dx, dy;
        SDL_SetRenderDrawColor(renderer, 180, 180, 180, 255);
        dx = W - 30;
        for (x = DUNW - 1; x >= 0; x--)
        {
                dy = 15;
                for (y = 0; y < DUNH; y++)
                {
                        SDL_RenderFillRect(renderer, &(SDL_Rect){
                                        dx, dy, 12, 12});

                        if (x == roomx && y == roomy)
                        {
                                SDL_SetRenderDrawColor(renderer, 0, 0, 230, 255);
                                SDL_RenderFillRect(renderer, &(SDL_Rect){
                                                dx, dy, 12, 12});
                                SDL_SetRenderDrawColor(renderer, 180, 180, 180, 255);
                        }

                        dy += 14;
                }
                dx -= 14;
        }
}

void draw_health()
{
        int hp = player[0].hp;
        SDL_Rect dest = {10, 10, SCALE*10, SCALE*10};
        SDL_Rect src = {290, 140, 10, 10};
        for(int hc = 20; hc > 0; hc -= 4)
        {
                src.y = 140 + 10 * (hp > 4 ? 4 :
                                    hp < 0 ? 0 : hp);
                SDL_RenderCopy(renderer, sprites, &src, &dest);
                hp -= 4;
                dest.x += SCALE*11;
        }
}

void draw_room_tile(int x, int y)
{
        int t = tiles[y][x];
        int bgtile = t == WATR ? WATR : DIRT;

        if (t != WATR)
        {
                if (x > 0      && tiles[y][x - 1] == WATR) bgtile += L;
                if (x < sW - 1 && tiles[y][x + 1] == WATR) bgtile += R;
                if (y > 0      && tiles[y - 1][x] == WATR) bgtile += U;
                if (y < sH - 1 && tiles[y + 1][x] == WATR) bgtile += D;
        }

        // background tile
        SDL_RenderCopy(renderer, sprites,
                &(SDL_Rect){20*(bgtile%15), 20*(bgtile/15), 20, 20},
                &(SDL_Rect){BS*x, BS*y, BS, BS});

        // foreground tile
        if(t != WATR && t != DIRT && t != OPEN && t != CLIP && t != HALFCLIP)
                SDL_RenderCopy(renderer, sprites,
                        &(SDL_Rect){20*(t%15), 20*(t/15), 20, 20},
                        &(SDL_Rect){BS*x, BS*y, BS, BS});
}

void draw_enemy(int i)
{
        SDL_Rect src;
        SDL_Rect dest;
        struct enemy e = enemy[i];

        if (!e.alive) return;

        if (e.type == SCREW && e.hp < 2)
        {
                if (e.stun == SCREW_STUN)
                        e.frame = e.hp ? 6 : 8;
                else if (e.stun == SCREW_STUN - 5)
                        e.frame = e.hp ? 7 : 9;
        }

        if (frame%10 == 0) switch (e.type)
        {
                case PIG:
                case BOARD:
                        e.frame = (e.frame + 1) % 4;
                        if(e.frame == 0 && rand()%10 == 0)
                                e.frame = 4;
                        break;
                case SCREW:
                        if (e.hp >= 2)
                                e.frame = (e.frame + 1) % 6;
                        else if (e.stun == 0)
                        {
                                if (rand() % 10 == 0)
                                        e.frame = e.hp ? 10 : 11;
                                else
                                        e.frame = e.hp ? 7 : 9;
                        }
                        break;
        }

        if (e.type == TOOLBOX)
        {
                src = (SDL_Rect){20+40*e.frame, 20, 40, 40};
                dest = e.pos;
                dest.y -= BS;
                dest.h += BS;
        }
        else if (e.type == WRENCH)
        {
                src = (SDL_Rect){280, 60+20*((frame/4)%4), 20, 20};
                dest = e.pos;
                dest.y -= BS2;
                dest.h += BS2;
        }
        else if (e.type == PIPEWRENCH)
        {
                src = (SDL_Rect){260, 60+20*((frame/4)%8), 20, 20};
                dest = e.pos;
                dest.y -= BS2;
                dest.h += BS2;
        }
        else
        {
                src = (SDL_Rect){0+20*e.frame, e.type*20, 20, 20};
                dest = e.pos;
                dest.y -= BS2;
                dest.h += BS2;
        }

        if (e.freeze)
        {
                int f = 4 - e.freeze;
                if(f < 0) f = 0;
                src = (SDL_Rect){100+20*f, 140, 20, 20};
        }
        else if (e.type == PUFF)
        {
                src = (SDL_Rect){100+20*e.frame, 140, 20, 20};
                if(frame%8 == 0 && ++e.frame > 4)
                        e.alive = 0;
        }
        else if (e.stun > 0 && e.type != SCREW)
        {
                if((frame/2)%2) return;

                dest.x += (rand()%3 - 1) * SCALE;
                dest.y += (rand()%3 - 1) * SCALE;
        }

        // copy back any changes made
        enemy[i] = e;

        SDL_RenderCopy(renderer, sprites, &src, &dest);
}

void draw_player(int i)
{
        SDL_Rect src;
        SDL_Rect dest;
        struct player p = player[i];

        if(!p.alive) return;

        int animframe = 0;

        if(p.state == PL_STAB)
        {
                animframe = 5;
        }
        else
        {
                if(frame%5 == 0)
                {
                        p.frame = (p.frame + 1) % 4;
                        if(p.frame == 0 && rand()%10 == 0)
                                p.frame = 4;
                }

                animframe = p.frame;
                if(p.vel.x == 0 && p.vel.y == 0 &&
                                p.frame != 4)
                        animframe = 0;
        }

        src = (SDL_Rect){20+20*animframe, 60+20*p.dir, 20, 20};
        dest = p.pos;
        dest.y -= BS2;
        dest.h += BS2;

        if(!p.stun || (frame/2)%2)
                SDL_RenderCopy(renderer, sprites, &src, &dest);

        if(animframe == 5)
        {
                int screwamt = 0;
                int retract = 0;

                if(p.delay < 6)
                        retract = 6 - p.delay;

                if(p.delay > 8)
                        screwamt = (p.delay/2) % 2;

                src.x = 140 + 20*screwamt;
                switch(p.dir)
                {
                        case EAST:  dest.x += BS - retract; break;
                        case NORTH: dest.y -= BS - retract; break;
                        case WEST:  dest.x -= BS - retract; break;
                        case SOUTH: dest.y += BS - retract; break;
                }
                SDL_RenderCopy(renderer, sprites, &src, &dest);
        }

        // copy back any changes
        player[i] = p;
}

//draw everything in the game on the screen
void draw_stuff()
{
        SDL_Rect dest = {0, 0, W, H};
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

        //draw room background
        SDL_RenderCopy(renderer, edgetex[0], NULL, &dest);

        draw_doors_lo();

        for(int x = 0; x < TILESW; x++) for(int y = 0; y < TILESH; y++)
                draw_room_tile(x, y);

        for(int i = 0; i < NR_ENEMIES; i++)
                draw_enemy(i);

        for(int i = 0; i < NR_PLAYERS; i++)
                draw_player(i);

        draw_doors_hi();
        if(drawclip) draw_clipping_boxes();
        draw_health();
        draw_map();

        SDL_RenderPresent(renderer);
}

#endif

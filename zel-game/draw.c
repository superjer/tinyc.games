#include "zel.c"
#ifndef ZEL_DRAW_C
#define ZEL_DRAW_C

#define TO_FRECT(r) (SDL_FRect){(r).x, (r).y, (r).w, (r).h}

void fixed_color_box(SDL_FRect rect, int r, int g, int b, int a)
{
        SDL_SetRenderDrawColor(renderer, r, g, b, a);
        SDL_RenderFillRect(renderer, &rect);
}

void color_box(SDL_FRect rect, int r, int g, int b, int a)
{
        rect.x += scrollx;
        rect.y += scrolly;
        fixed_color_box(rect, r, g, b, a);
}

void sprite(int spr, SDL_FRect dest)
{
        int sz = 20;
        if (spr >= 1000) // extra large sprite?
        {
                sz = 40;
                spr -= 1000;
        }

        SDL_FRect src = (SDL_FRect){(spr * sz) % 300, (spr * sz) / 300 * sz, sz, sz};
        dest.x += scrollx;
        dest.y += scrolly;
        SDL_RenderTexture(renderer, spritetex, &src, &dest);
}

void tile(SDL_FRect src, SDL_FRect dest)
{
        dest.x += scrollx;
        dest.y += scrolly;
        SDL_RenderTexture(renderer, tiletex, &src, &dest);
}

void draw_doors_lo(int rx, int ry, int offx, int offy)
{
        SDL_FRect src, dest;
        int r = ry*DUNW + rx; // current room coordinate
        int *doors = rooms[r].doors;

        offx += scrollx;
        offy += scrolly;

        //draw right edge
        src  = (SDL_FRect){11*20, 4*20, 4*20, 3*20};
        dest = (SDL_FRect){11*BS + offx, 4*BS + offy, 4*BS, 3*BS};
        SDL_RenderTexture(renderer, edgetex[doors[EAST]], &src, &dest);

        //draw top edge
        src  = (SDL_FRect){6*20, 0*20, 3*20, 4*20};
        dest = (SDL_FRect){6*BS + offx, 0*BS + offy, 3*BS, 4*BS};
        SDL_RenderTexture(renderer, edgetex[doors[NORTH]], &src, &dest);

        //draw left edge
        src  = (SDL_FRect){0*20, 4*20, 4*20, 3*20};
        dest = (SDL_FRect){0*BS + offx, 4*BS + offy, 4*BS, 3*BS};
        SDL_RenderTexture(renderer, edgetex[doors[WEST]], &src, &dest);

        //draw bottom edge
        src  = (SDL_FRect){6*20, 7*20, 3*20, 4*20};
        dest = (SDL_FRect){6*BS + offx, 7*BS + offy, 3*BS, 4*BS};
        SDL_RenderTexture(renderer, edgetex[doors[SOUTH]], &src, &dest);
}

void draw_doors_hi(int rx, int ry, int offx, int offy)
{
        SDL_FRect src, dest;
        int r = ry*DUNW + rx; // current room coordinate
        int *doors = rooms[r].doors;

        offx += scrollx;
        offy += scrolly;

        //draw right edge ABOVE
        src  = (SDL_FRect){14*20, 4*20, 1*20, 3*20};
        dest = (SDL_FRect){14*BS + offx, 4*BS + offy, 1*BS, 3*BS};
        SDL_RenderTexture(renderer, edgetex[doors[EAST]], &src, &dest);

        //draw top edge ABOVE
        src  = (SDL_FRect){6*20, 0*20, 3*20, 1*20};
        dest = (SDL_FRect){6*BS + offx, 0*BS + offy, 3*BS, 1*BS};
        SDL_RenderTexture(renderer, edgetex[doors[NORTH]], &src, &dest);

        //draw left edge ABOVE
        src  = (SDL_FRect){0*20, 4*20, 1*20, 3*20};
        dest = (SDL_FRect){0*BS + offx, 4*BS + offy, 1*BS, 3*BS};
        SDL_RenderTexture(renderer, edgetex[doors[WEST]], &src, &dest);

        //draw bottom edge ABOVE
        src  = (SDL_FRect){6*20, 10*20, 3*20, 1*20};
        dest = (SDL_FRect){6*BS + offx, 10*BS + offy, 3*BS, 1*BS};
        SDL_RenderTexture(renderer, edgetex[doors[SOUTH]], &src, &dest);
}

void draw_clipping_boxes()
{
        for (int x = 0; x < TILESW; x++) for(int y = 0; y < TILESH; y++)
        {
                if (tiles[y][x] <= LASTSOLID)
                        color_box((SDL_FRect){BS*x+1, BS*y+1, BS-1, BS-1}, 255, 0, 0, 120);
                else if (tiles[y][x] == HALFCLIP)
                        color_box((SDL_FRect){BS*x+1, BS*y+1, BS-1, BS2-1}, 255, 0, 0, 120);
        }

        for (int i = 0; i < NR_PLAYERS; i++)
        {
                if (player[i].state == PL_STAB && player[i].exists)
                        color_box(TO_FRECT(player[i].hitbox), 255, 80, 80, 120);
                color_box(TO_FRECT(player[i].pos), 80, 200, 80, 120);
        }

        for (int j = 0; j < NR_ENEMIES; j++)
        {
                if (enemy[j].exists)
                        color_box(TO_FRECT(enemy[j].pos), 200, 80, 200, 120);
        }
}

void draw_map()
{
        int x, y, dx, dy;
        dx = W - 30;
        for (x = DUNW - 1; x >= 0; x--)
        {
                dy = 15;
                for (y = 0; y < DUNH; y++)
                {
                        fixed_color_box((SDL_FRect){dx, dy, 12, 12}, 180, 180, 180, 255);

                        if (x == roomx && y == roomy)
                                fixed_color_box((SDL_FRect){dx, dy, 12, 12}, 0, 0, 230, 255);

                        dy += 14;
                }
                dx -= 14;
        }
}

void draw_health()
{
        int hp = player[0].hp;
        SDL_FRect src = (SDL_FRect){220, 200, 10, 10};
        SDL_FRect dest = (SDL_FRect){10, 10, SCALE*10, SCALE*10};
        for(int hc = 20; hc > 0; hc -= 4)
        {
                src.x = 220 + 10 * (hp > 4 ? 4 :
                                    hp < 0 ? 0 : hp);
                SDL_RenderTexture(renderer, spritetex, &src, &dest);
                hp -= 4;
                dest.x += SCALE * 11;
        }
}

void draw_room_tile(int (*ta)[TILESW], int x, int y, int offx, int offy)
{
        int t = ta[y][x];
        int bgtile = t == WATR ? WATR : DIRT;

        if (t != WATR)
        {
                if (x > 0      && ta[y][x - 1] == WATR) bgtile += L;
                if (x < sW - 1 && ta[y][x + 1] == WATR) bgtile += R;
                if (y > 0      && ta[y - 1][x] == WATR) bgtile += U;
                if (y < sH - 1 && ta[y + 1][x] == WATR) bgtile += D;
        }

        // background tile
        if (!inside)
                tile((SDL_FRect){20*(bgtile%15), 20*(bgtile/15), 20, 20},
                     (SDL_FRect){BS*x + offx, BS*y + offy, BS, BS});

        // foreground tile
        if (t != WATR && t != DIRT && t != OPEN && t != CLIP && t != HALFCLIP)
                tile((SDL_FRect){20*(t%15), 20*(t/15), 20, 20},
                     (SDL_FRect){BS*x + offx, BS*y + offy, BS, BS});
}

#define e (enemy[n])

void draw_board(int n)
{
        if (global_frame % 10 == 0)
        {
                e.frame = (e.frame + 1) % 4;
                if(e.frame == 0 && pct(10))
                        e.frame = 4;
        }

        sprite(BOARD + e.frame, TO_FRECT(e.pos));
}

void draw_pig(int n)
{
        if (global_frame % 10 == 0)
        {
                e.frame = (e.frame + 1) % 4;
                if(e.frame == 0 && pct(10))
                        e.frame = 4;
        }

        sprite(PIG + e.frame, TO_FRECT(e.pos));
}

void draw_screw(int n)
{
        if (e.type == SCREW && e.state != READY)
        {
                if (e.stun == SCREW_STUN)
                        e.frame = e.state == STUCK ? 6 : 8;
                else if (e.stun == SCREW_STUN - 5)
                        e.frame = e.state == STUCK ? 7 : 9;
        }

        if (global_frame % 10 == 0)
        {
                if (e.state == READY)
                        e.frame = (e.frame + 1) % 6;
                else if (e.stun == 0)
                {
                        if (rand() % 10 == 0)
                                e.frame = e.state == STUCK ? 10 : 11;
                        else
                                e.frame = e.state == STUCK ? 7 : 9;
                }
        }

        sprite(SCREW + e.frame, TO_FRECT(e.pos));
}

void draw_toolbox(int n)
{
        SDL_FRect dest = TO_FRECT(e.pos);
        dest.y -= BS;
        dest.h = 2 * BS;
        sprite(TOOLBOX + e.frame, dest);
}

void draw_wrench(int n)
{
        if (e.type == WRENCH)
                sprite(WRENCH + (global_frame / 4) % 4, TO_FRECT(e.pos));
        else
                sprite(PIPEWRENCH + (global_frame / 4) % 8, TO_FRECT(e.pos));
}

void draw_enemy(int n)
{
        if (!e.exists) return;

        if (e.freeze)
        {
                int frame = MAX(0, 4 - e.freeze);
                sprite(PUFF + frame, TO_FRECT(e.pos));
                return;
        }
        else if (e.type == PUFF)
        {
                if(global_frame % 8 == 0 && ++e.frame > 4)
                        e.exists = 0;
                sprite(PUFF + e.frame, TO_FRECT(e.pos));
                return;
        }

        // flash when stunned
        if (e.type != SCREW && e.stun > 0 && (global_frame / 2) % 2)
                return;

        switch (e.type)
        {
                case BOARD:      draw_board(n);   break;
                case PIG:        draw_pig(n);     break;
                case SCREW:      draw_screw(n);   break;
                case TOOLBOX:    draw_toolbox(n); break;
                case WRENCH:     draw_wrench(n);  break;
                case PIPEWRENCH: draw_wrench(n);  break;
        }
}

#undef e

void draw_player(int i)
{
        #define p (player[i])

        if (!p.exists) return;

        int animframe = 5;

        if (p.state != PL_STAB)
        {
                if (global_frame % 5 == 0)
                {
                        p.frame = (p.frame + 1) % 4;
                        if (p.frame == 0 && pct(10))
                                p.frame = 4;
                }

                animframe = p.frame;
                if (p.vel.x == 0 && p.vel.y == 0 && p.frame != 4)
                        animframe = 0;
        }

        SDL_FRect src = (SDL_FRect){20 * animframe, 20 * p.dir, 20, 20};
        SDL_FRect dest = (SDL_FRect){p.pos.x, p.pos.y, p.pos.w, p.pos.h};
        dest.y -= BS2;
        dest.h += BS2;
        dest.x += scrollx;
        dest.y += scrolly;

        if (!p.stun || (global_frame / 2)%2)
                SDL_RenderTexture(renderer, spritetex, &src, &dest);

        if (p.state == PL_STAB)
        {
                int slashamt = 0;
                int retract = 0;

                if (p.delay < 6)
                        retract = 6 - p.delay;

                if (p.delay > 8)
                        slashamt = (p.delay / 2) % 2;

                src.x = 120 + 20 * slashamt;
                switch (p.dir)
                {
                        case EAST:  dest.x += BS - retract; break;
                        case NORTH: dest.y -= BS - retract; break;
                        case WEST:  dest.x -= BS - retract; break;
                        case SOUTH: dest.y += BS - retract; break;
                }
                SDL_RenderTexture(renderer, spritetex, &src, &dest);
        }

        #undef p
}

//draw everything in the game on the screen
void draw_stuff()
{
        SDL_FRect whole = {scrollx, scrolly, W, H};
        int offx = -sign(scrollx) * W; // offset for scrolling-away screen
        int offy = -sign(scrolly) * H;
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

        if (inside)
        {
                SDL_RenderTexture(renderer, edgetex[0], NULL, &whole); // room bg
                whole.x += offx;
                whole.y += offy;
                if (scrollx || scrolly)
                        SDL_RenderTexture(renderer, edgetex[0], NULL, &whole); // old room bg

                draw_doors_lo(roomx, roomy, 0, 0);
                if (scrollx || scrolly)
                        draw_doors_lo(oldroomx, oldroomy, offx, offy);
        }

        for (int x = 0; x < TILESW; x++) for (int y = 0; y < TILESH; y++)
                draw_room_tile(tiles, x, y, 0, 0);

        if (scrollx || scrolly)
        {
                for (int x = 0; x < TILESW; x++) for (int y = 0; y < TILESH; y++)
                        draw_room_tile(tiles_old, x, y, offx, offy);
        }

        for (int i = 0; i < NR_ENEMIES; i++)
                draw_enemy(i);

        for (int i = 0; i < NR_PLAYERS; i++)
                draw_player(i);

        if (inside)
        {
                draw_doors_hi(roomx, roomy, 0, 0);
                if (scrollx || scrolly)
                        draw_doors_hi(oldroomx, oldroomy, offx, offy);
        }

        if(drawclip) draw_clipping_boxes();
        draw_health();
        draw_map();

        SDL_RenderPresent(renderer);
}

#endif // ZEL_DRAW_C

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <SDL.h>
#include <SDL_ttf.h>

#define W 600
#define H 440
#define TILESW 15
#define TILESH 11
#define BS 40
#define BS2 (BS/2)
#define PLYR_W 40
#define PLYR_H 20

enum gamestates {READY, ALIVE, GAMEOVER} gamestate = READY;

struct player {
        SDL_Rect pos;
        struct vel {
                int x;
                int y;
        } vel;
        int hearts;
        int bombs;
        int money;
        int flags;
} player[4];

struct tile {
        int solid;
        SDL_Color color;
} room[TILESH][TILESW];

int nr_players = 1;
int idle_time = 30;
int frame = 0;
int drawclip = 0;

enum edge {WALL=0, HOLE, DOOR, LOCKED, SHUTTER, ENTRY, MAXEDGE};

SDL_Event event;
SDL_Renderer *renderer;
SDL_Surface *surf;
SDL_Texture *sprites;
SDL_Texture *edgetex[20];
TTF_Font *font;

void setup();
void new_game();
void key_move(int down);
void update_stuff();
void move_player(int velx, int vely);
int collide(SDL_Rect r0, SDL_Rect r1);
void draw_stuff();
void text(char *fstr, int value, int height);

//the entry point and main game loop
int main()
{
        setup();
        new_game();

        for(;;)
        {
                while(SDL_PollEvent(&event)) switch(event.type)
                {
                        case SDL_QUIT:    exit(0);
                        case SDL_KEYDOWN: key_move(1); break;
                        case SDL_KEYUP:   key_move(0); break;
                }

                update_stuff();
                draw_stuff();
                SDL_Delay(1000 / 60);
                frame++;
        }
}

//initial setup to get the window and rendering going
void setup()
{
        srand(time(NULL));

        SDL_Init(SDL_INIT_VIDEO);
        SDL_Window *win = SDL_CreateWindow("Zel",
                SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, W, H, SDL_WINDOW_SHOWN);
        renderer = SDL_CreateRenderer(win, -1, SDL_RENDERER_PRESENTVSYNC);
        if(!renderer) exit(fprintf(stderr, "Could not create SDL renderer\n"));

        for(int i = 0; i < MAXEDGE; i++)
        {
                char file[80];
                sprintf(file, "res/room-%d.bmp", i);
                surf = SDL_LoadBMP(file);
                SDL_SetColorKey(surf, 1, 0xffff00);
                edgetex[i] = SDL_CreateTextureFromSurface(renderer, surf);
        }

        surf = SDL_LoadBMP("res/sprites.bmp");
        SDL_SetColorKey(surf, 1, 0xffff00);
        sprites = SDL_CreateTextureFromSurface(renderer, surf);

        TTF_Init();
        font = TTF_OpenFont("res/LiberationSans-Regular.ttf", 42);
}

void key_move(int down)
{
        if(event.key.repeat)
                return;

        int amt = down ? 4 : -4;

        switch(event.key.keysym.sym)
        {
                case SDLK_UP:    player[0].vel.y -= amt; break;
                case SDLK_DOWN:  player[0].vel.y += amt; break;
                case SDLK_LEFT:  player[0].vel.x -= amt; break;
                case SDLK_RIGHT: player[0].vel.x += amt; break;
                case SDLK_SPACE: drawclip = !drawclip;   break;
        }
}

//start a new game
void new_game()
{
        gamestate = ALIVE;
        player[0].pos.x = (W - PLYR_W) / 2;
        player[0].pos.y = H - 100;
        player[0].pos.w = PLYR_W;
        player[0].pos.h = PLYR_H;

        for(int x = 0; x < TILESW; x++) for(int y = 0; y < TILESH; y++)
        {
                int edge_x = (x <= 1 || x >= TILESW-2);
                int door_x = (edge_x && y == TILESH/2);
                int edge_y = (y <= 1 || y >= TILESH-2);
                int door_y = (edge_y && x == TILESW/2);

                if((edge_x || edge_y) && !door_x && !door_y)
                {
                        room[y][x].solid = 1;
                        room[y][x].color = (SDL_Color){0, 30, 50, 255};
                }
        }
}

//when we hit something
void game_over()
{
        gamestate = GAMEOVER;
}

//update everything that needs to update on its own, without input
void update_stuff()
{
        move_player(player[0].vel.x, 0);
        move_player(0, player[0].vel.y);
}

void move_player(int velx, int vely)
{
        SDL_Rect newpos = player[0].pos;

        newpos.x += velx;
        newpos.y += vely;

        int already_stuck = 0;
        int would_be_stuck = 0;

        for(int i = 0; i < 2; i++) for(int j = 0; j < 2; j++)
        {
                int bx = player[0].pos.x/BS + i;
                int by = player[0].pos.y/BS + j;
                SDL_Rect block = {BS*bx, BS*by, BS, BS};

                int newbx = newpos.x/BS + i;
                int newby = newpos.y/BS + j;
                SDL_Rect newblock = {BS*newbx, BS*newby, BS, BS};

                if(bx >= 0 && bx < TILESW && by >= 0 && by < TILESH &&
                                room[by][bx].solid &&
                                collide(player[0].pos, block))
                        already_stuck = 1;

                if(newbx >= 0 && newbx < TILESW && newby >= 0 && newby < TILESH &&
                                room[newby][newbx].solid &&
                                collide(newpos, newblock))
                        would_be_stuck = 1;
        }

        /* if(player[0].vel.x || player[0].vel.y) */
        /*         printf("wouldbe %d    already %d\n", would_be_stuck, already_stuck); */

        if(!would_be_stuck || already_stuck)
                player[0].pos = newpos;
}

int collide(SDL_Rect plyr, SDL_Rect block)
{
        /* if(player[0].vel.x || player[0].vel.y) */
        /*         printf("%d %d %d %d     %d %d %d %d\n", */
        /*                 plyr.x, */
        /*                 plyr.y, */
        /*                 plyr.w, */
        /*                 plyr.h, */
        /*                 block.x, */
        /*                 block.y, */
        /*                 block.w, */
        /*                 block.h); */
        int xcollide = block.x + block.w >= plyr.x && block.x < plyr.x + plyr.w;
        int ycollide = block.y + block.h >= plyr.y && block.y < plyr.y + plyr.h;
        return xcollide && ycollide;
}

//draw everything in the game on the screen
void draw_stuff()
{
        SDL_Rect src;
        SDL_Rect dest = {0, 0, W, H};
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

        //draw room background
        SDL_RenderCopy(renderer, edgetex[0], NULL, &dest);

        //draw right edge
        src  = (SDL_Rect){13*20, 4*20, 2*20, 3*20};
        dest = (SDL_Rect){13*BS, 4*BS, 2*BS, 3*BS};
        SDL_RenderCopy(renderer, edgetex[1], &src, &dest);

        //draw top edge
        src  = (SDL_Rect){6*20, 0*20, 3*20, 2*20};
        dest = (SDL_Rect){6*BS, 0*BS, 3*BS, 2*BS};
        SDL_RenderCopy(renderer, edgetex[2], &src, &dest);

        //draw left edge
        src  = (SDL_Rect){0*20, 4*20, 2*20, 3*20};
        dest = (SDL_Rect){0*BS, 4*BS, 2*BS, 3*BS};
        SDL_RenderCopy(renderer, edgetex[3], &src, &dest);

        //draw bottom edge
        src  = (SDL_Rect){6*20, 7*20, 3*20, 4*20};
        dest = (SDL_Rect){6*BS, 7*BS, 3*BS, 4*BS};
        SDL_RenderCopy(renderer, edgetex[5], &src, &dest);

        for(int i = 0; i < nr_players; i++)
        {
                src = (SDL_Rect){0, 6*20, 20, 20};
                dest = player[i].pos;
                dest.y -= 20;
                dest.h += 20;
                SDL_RenderCopy(renderer, sprites, &src, &dest);
        }

        //draw right edge ABOVE
        src  = (SDL_Rect){14*20, 4*20, 1*20, 3*20};
        dest = (SDL_Rect){14*BS, 4*BS, 1*BS, 3*BS};
        SDL_RenderCopy(renderer, edgetex[1], &src, &dest);

        //draw top edge ABOVE
        src  = (SDL_Rect){6*20, 0*20, 3*20, 1*20};
        dest = (SDL_Rect){6*BS, 0*BS, 3*BS, 1*BS};
        SDL_RenderCopy(renderer, edgetex[2], &src, &dest);

        //draw left edge ABOVE
        src  = (SDL_Rect){0*20, 4*20, 1*20, 3*20};
        dest = (SDL_Rect){0*BS, 4*BS, 1*BS, 3*BS};
        SDL_RenderCopy(renderer, edgetex[3], &src, &dest);

        //draw bottom edge ABOVE
        src  = (SDL_Rect){6*20, 10*20, 3*20, 1*20};
        dest = (SDL_Rect){6*BS, 10*BS, 3*BS, 1*BS};
        SDL_RenderCopy(renderer, edgetex[5], &src, &dest);

        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
        for(int x = 0; x < TILESW; x++) for(int y = 0; y < TILESH; y++)
        {
                if(drawclip && room[y][x].solid)
                        SDL_RenderFillRect(renderer, &(SDL_Rect){BS*x, BS*y, BS, BS});
        }

        //text("Zel", 0, 10);

        SDL_RenderPresent(renderer);
}

void text(char *fstr, int value, int height)
{
        if(!font) return;
        int w, h;
        char msg[80];
        snprintf(msg, 80, fstr, value);
        TTF_SizeText(font, msg, &w, &h);
        SDL_Surface *msgsurf = TTF_RenderText_Blended(font, msg, (SDL_Color){255, 255, 255});
        SDL_Texture *msgtex = SDL_CreateTextureFromSurface(renderer, msgsurf);
        SDL_Rect fromrec = {0, 0, msgsurf->w, msgsurf->h};
        SDL_Rect torec = {(W - w)/2, height, msgsurf->w, msgsurf->h};
        SDL_RenderCopy(renderer, msgtex, &fromrec, &torec);
        SDL_DestroyTexture(msgtex);
        SDL_FreeSurface(msgsurf);
}

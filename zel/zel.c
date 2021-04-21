// Zel -- http://tinyc.games -- (c) 2020 Jer Wilson
//
// Zel is a "tiny" adventure game with lots of content. I'll admit it's pushing
// the boundaries of what a "tiny" game should be.

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#define SDL_DISABLE_IMMINTRIN_H
#include <SDL.h>
#include <SDL_ttf.h>

#define SCALE 3                    // 3x magnification
#define W (300*SCALE)              // window width, height
#define H (220*SCALE)              // ^
#define TILESW 15                  // total room width, height
#define TILESH 11                  // ^
#define INNERTILESW 11             // inner room width, height
#define INNERTILESH 7              // ^
#define DUNH 3                     // entire dungeon width, height
#define DUNW 5                     // ^
#define BS (20*SCALE)              // block size
#define BS2 (BS/2)                 // block size in half
#define PLYR_W BS                  // physical width and height of the player
#define PLYR_H BS2                 // ^
#define PLYR_SPD 6                 // units per frame
#define LATERAL_STEPS 8            // how far to check for a way around an obstacle
#define NR_PLAYERS 4
#define NR_ENEMIES 8

#define PIT   0        // pit tile and edges:
#define R     1        // PIT|R for pit with right edge
#define U     2        // PIT|R|U for put with right & upper edge
#define L     4        // also works: just R|U
#define D     8        // ^
#define FACE 30        // the statue face thing

#define TREE 54
#define ROCK 55
#define WATR 56
#define STON 57

#define BLOK 45        // the bevelled block
#define CLIP 58        // invisible but solid tile
#define LASTSOLID CLIP // everything less than here is solid
#define HALFCLIP 59    // this is half solid (upper half)
#define SAND 60        // sand - can walk on like open

#define SPOT 150
#define DIRT 165

#define OPEN 75        // invisible open, walkable space

enum enemytypes {
        PIG = 7,
        SCREW = 8,
        BOARD = 9,
        GLOB = 10,
        TOOLBOX = 11,
        PUFF = 12,
        WRENCH = 13,
        PIPEWRENCH = 14,
};

enum dir {NORTH, WEST, EAST, SOUTH};
enum doors {WALL, LOCKED, SHUTTER, MAXWALL=SHUTTER, DOOR, HOLE, ENTRY, MAXDOOR};

#include "odnar.c"
#include "level_data.c"

int demilitarized_zone[10000];
int inside = 0;
int roomx; // current room x,y
int roomy;
int tiles[TILESH][TILESW];

struct player {
        SDL_Rect pos;
        SDL_Rect hitbox;
        struct point vel;
        int reel;
        int reeldir;
        int dir;
        int ylast; // moved in y direction last?
        int state;
        int delay;
        int frame;
        int alive;
        int hp;
        int stun;
} player[NR_PLAYERS];

struct enemy {
        SDL_Rect pos;
        struct point vel;
        int reel;
        int reeldir;
        int state;
        int type;
        int delay;
        int frame;
        int alive;
        int hp;
        int stun;
        int freeze;
} enemy[NR_ENEMIES];

int idle_time = 30;
int frame = 0;
int drawclip = 0;
int noclip = false;

SDL_Event event;
SDL_Renderer *renderer;
SDL_Surface *surf;
SDL_Texture *sprites;
SDL_Texture *edgetex[20];
TTF_Font *font;

int find_free_slot(int *slot);
int collide(SDL_Rect plyr, SDL_Rect block);
int block_collide(int bx, int by, SDL_Rect plyr);
int world_collide(SDL_Rect plyr);
int edge_collide(SDL_Rect plyr);
void new_game();
void screen_scroll(int dx, int dy);

#include "player.c"
#include "enemies.c"

//prototypes
void setup();
void load_room();
void key_move(int down);
void draw_stuff();
void draw_doors_lo();
void draw_doors_hi();
void draw_clipping_boxes();
void draw_map();

//the entry point and main game loop
int main()
{
        odnar();
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

                update_player();
                update_enemies();
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

        for(int i = 0; i < MAXDOOR; i++)
        {
                char xfile[80];
                sprintf(xfile, "res/room-%d.bmp", i);
                surf = SDL_LoadBMP(xfile);
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
        if(event.key.repeat) return;

        int amt = down ? PLYR_SPD : -PLYR_SPD;

        if(down)
        {
                if(event.key.keysym.sym == SDLK_UP || event.key.keysym.sym == SDLK_DOWN)
                        player[0].ylast = 1;
                else
                        player[0].ylast = 0;
        }

        switch(event.key.keysym.sym)
        {
                case SDLK_UP:
                        player[0].vel.y -= amt;
                        if(down) player[0].dir = NORTH;
                        break;
                case SDLK_DOWN:
                        player[0].vel.y += amt;
                        if(down) player[0].dir = SOUTH;
                        break;
                case SDLK_LEFT:
                        player[0].vel.x -= amt;
                        if(down) player[0].dir = WEST;
                        break;
                case SDLK_RIGHT:
                        player[0].vel.x += amt;
                        if(down) player[0].dir = EAST;
                        break;
                case SDLK_SPACE:
                        if (down) drawclip = !drawclip;
                        break;
                case SDLK_n:
                        if (down) noclip = !noclip;
                        break;
                case SDLK_z:
                case SDLK_x:
                        if(player[0].state == PL_NORMAL)
                        {
                                player[0].state = PL_STAB;
                                player[0].delay = 16;
                        }
                        break;
                case SDLK_ESCAPE:
                        exit(0);
        }
}

//start a new game
void new_game()
{
        //pick key point for start
        static int kp_start = -1;
        if (kp_start == -1)
                kp_start = rand() % NUM_KEY_POINTS;

        memset(player, 0, sizeof player);
        player[0].alive = 1;
        player[0].pos.x = BS * (key_points[kp_start].x % TILESW);
        player[0].pos.y = BS * (key_points[kp_start].y % TILESH) + BS2;
        player[0].pos.w = PLYR_W;
        player[0].pos.h = PLYR_H;
        player[0].dir = SOUTH;
        player[0].hp = 3*4;
        roomx = key_points[kp_start].x / TILESW;
        roomy = key_points[kp_start].y / TILESH;
        load_room();
}

void load_room()
{
        int r = roomy*DUNW + roomx; // current room coordinate

        if (inside)
        {
                for(int x = 0; x < TILESW; x++) for(int y = 0; y < TILESH; y++)
                {
                        int edge_x = (x <= 1 || x >= TILESW-2);
                        int door_x = (edge_x && y == TILESH/2);
                        int edge_y = (y <= 1 || y >= TILESH-2);
                        int door_y = (edge_y && x == TILESW/2);

                        if(edge_x || edge_y)
                                tiles[y][x] = (door_x ? HALFCLIP : door_y ? OPEN : CLIP);
                        else
                                tiles[y][x] = rooms[r].tiles[(y)*TILESW + (x)];
                }

                //set the clipping to match the doors
                if(rooms[r].doors[NORTH] <= MAXWALL) tiles[1][ 7] = CLIP;
                if(rooms[r].doors[WEST ] <= MAXWALL) tiles[5][ 1] = CLIP;
                if(rooms[r].doors[EAST ] <= MAXWALL) tiles[5][13] = CLIP;
                if(rooms[r].doors[SOUTH] <= MAXWALL) tiles[9][ 7] = CLIP;
        }
        else
        {
                for(int x = 0; x < TILESW; x++) for(int y = 0; y < TILESH; y++)
                {
                        int c = charout[y + roomy * TILESH][x + roomx * TILESW];
                        int t = c == 'T' ? TREE :
                                c == 'R' ? ROCK :
                                c == 'S' ? STON :
                                c == 'W' ? WATR :
                                c == '.' ? SPOT : DIRT;
                        tiles[y][x] = t;
                }

                for(int x = 0; x < TILESW; x++) for(int y = 0; y < TILESH; y++)
                {
                        if (tiles[y][x] != DIRT) continue;
                        if (x < TILESW - 1 && tiles[y][x + 1] == WATR) tiles[y][x] += R;
                        if (x > 0          && tiles[y][x - 1] == WATR) tiles[y][x] += L;
                        if (y < TILESH - 1 && tiles[y + 1][x] == WATR) tiles[y][x] += D;
                        if (y > 0          && tiles[y - 1][x] == WATR) tiles[y][x] += U;
                }
        }

        int spawns[] = {
                 7,5,   9,2,   4,4,   6,7,   2,2,  10,3,   2,7,   8,4,  11,4,   2,8,  12,6,
                 9,8,   8,5,   6,4,   3,6,   9,7,  12,7,  10,7,   8,3,   6,3,   6,8,  11,5,
                12,2,  12,3,  11,3,   4,7,  12,5,   9,3,  11,6,   3,3,   7,3,  10,8,   7,8,
                10,5,   9,4,   5,6,   8,7,   6,5,  12,8,  10,2,   3,8,   4,8,   2,3,   4,2,
                 5,4,   2,4,   5,2,  11,7,   6,6,   7,7,  11,8,   3,7,   3,2,   9,5,   4,3,
                 7,2,   2,6,   8,2,   5,5,   4,5,   5,8,  12,4,  10,6,   3,5,   5,7,   4,6,
                 3,4,   8,8,  11,2,   8,6,   2,5,   9,6,   7,6,   7,4,   6,2,  10,4,   5,3,
                 0,0,
        };

        //load enemies
        int j = 0;
        int plx = (player[0].pos.x + BS - 1) / BS;
        int ply = (player[0].pos.y + BS - 1) / BS;
        memset(enemy, 0, sizeof enemy);
        for(int i = 0; i < NR_ENEMIES; i++)
        {
                if(!rooms[r].enemies[i]) continue;

                enemy[i].type = rooms[r].enemies[i];
                enemy[i].alive = 1;
                enemy[i].hp = 1;
                enemy[i].freeze = 20 + rand() % 30;

                //find a good spawn position
                for(int limit = 0; limit < 70; limit++, j += 2)
                {
                        if(spawns[j] == 0)
                                j = 0; //loop around!
                        int x = spawns[j];
                        int y = spawns[j+1];
                        if(tiles[y][x] <= LASTSOLID)
                                continue; //no spawning in solids
                        if(limit == 70 || abs(x-plx) > 5 || abs(y-ply) > 4)
                                enemy[i].pos = (SDL_Rect){BS*x, BS*y + BS2, BS, BS2};
                }

                if(enemy[i].type == TOOLBOX)
                {
                        enemy[i].pos.x = 13*BS2;
                        enemy[i].pos.y = 3*BS;
                        enemy[i].pos.w = 2*BS;
                        enemy[i].pos.h = BS;
                        enemy[i].hp = 7;
                        enemy[i].freeze = 10;
                }

                if(enemy[i].type == SCREW)
                {
                        enemy[i].hp = 2;
                }
        }
}

int find_free_slot(int *slot)
{
        for(*slot = 0; *slot < NR_ENEMIES; (*slot)++)
                if(!enemy[*slot].alive)
                        return 1;
        return 0;
}

//collide a rect with nearby world tiles
int world_collide(SDL_Rect plyr)
{
        for(int i = 0; i < 3; i++) for(int j = 0; j < 2; j++)
        {
                int bx = plyr.x/BS + i;
                int by = plyr.y/BS + j;

                if(block_collide(bx, by, plyr))
                        return true;
        }

        return false;
}

int edge_collide(SDL_Rect plyr)
{
        if (plyr.x + plyr.w >= W)
                return true;
        if (plyr.y + plyr.h >= H)
                return true;
        if (plyr.x <= 0)
                return true;
        if (plyr.y <= 0)
                return true;

        return false;
}

int legit_tile(int x, int y)
{
        return x >= 0 && x < TILESW && y >= 0 && y < TILESH;
}

void screen_scroll(int dx, int dy)
{
        roomx += dx;
        roomy += dy;

        player[0].pos.x -= dx * (W - BS*2);
        player[0].pos.y -= dy * (H - BS*2);

        //bad room! back to start!
        if(roomx < 0 || roomx >= DUNW || roomy < 0 || roomy >= DUNH)
        {
                roomx = 0;
                roomy = 0;
                player[0].pos.x = BS;
                player[0].pos.y = BS;
        }

        load_room();
}

//collide a rect with a rect
int collide(SDL_Rect plyr, SDL_Rect block)
{
        int xcollide = block.x + block.w >= plyr.x && block.x < plyr.x + plyr.w;
        int ycollide = block.y + block.h >= plyr.y && block.y < plyr.y + plyr.h;
        return xcollide && ycollide;
}

//collide a rect with a block
int block_collide(int bx, int by, SDL_Rect plyr)
{
        if(!legit_tile(bx, by))
                return 0;

        if(tiles[by][bx] <= LASTSOLID)
                return collide(plyr, (SDL_Rect){BS*bx, BS*by, BS, BS});

        if(tiles[by][bx] == HALFCLIP)
                return collide(plyr, (SDL_Rect){BS*bx, BS*by, BS, BS2-1});

        return 0;
}

//draw everything in the game on the screen
void draw_stuff()
{
        SDL_Rect src;
        SDL_Rect dest = {0, 0, W, H};
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

        //draw room background
        SDL_RenderCopy(renderer, edgetex[0], NULL, &dest);

        draw_doors_lo();

        //draw room tiles
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
        for(int x = 0; x < TILESW; x++) for(int y = 0; y < TILESH; y++)
        {
                int t = tiles[y][x];
                if(t != OPEN && t != CLIP && t != HALFCLIP)
                        SDL_RenderCopy(renderer, sprites,
                                &(SDL_Rect){20*(t%15), 20*(t/15), 20, 20},
                                &(SDL_Rect){BS*x, BS*y, BS, BS});
        }

        //draw enemies
        for(int i = 0; i < NR_ENEMIES; i++)
        {
                if(!enemy[i].alive)
                        continue;

                if (enemy[i].type == SCREW && enemy[i].hp < 2)
                {
                        if (enemy[i].stun == SCREW_STUN)
                                enemy[i].frame = enemy[i].hp ? 6 : 8;
                        else if (enemy[i].stun == SCREW_STUN - 5)
                                enemy[i].frame = enemy[i].hp ? 7 : 9;
                }

                if(frame%10 == 0) switch(enemy[i].type)
                {
                        case PIG:
                        case BOARD:
                                enemy[i].frame = (enemy[i].frame + 1) % 4;
                                if(enemy[i].frame == 0 && rand()%10 == 0)
                                        enemy[i].frame = 4;
                                break;
                        case SCREW:
                                if (enemy[i].hp >= 2)
                                        enemy[i].frame = (enemy[i].frame + 1) % 6;
                                else if (enemy[i].stun == 0)
                                {
                                        if (rand() % 10 == 0)
                                                enemy[i].frame = enemy[i].hp ? 10 : 11;
                                        else
                                                enemy[i].frame = enemy[i].hp ? 7 : 9;
                                }
                                break;
                }

                if(enemy[i].type == TOOLBOX)
                {
                        src = (SDL_Rect){20+40*enemy[i].frame, 20, 40, 40};
                        dest = enemy[i].pos;
                        dest.y -= BS;
                        dest.h += BS;
                }
                else if(enemy[i].type == WRENCH)
                {
                        src = (SDL_Rect){280, 60+20*((frame/4)%4), 20, 20};
                        dest = enemy[i].pos;
                        dest.y -= BS2;
                        dest.h += BS2;
                }
                else if(enemy[i].type == PIPEWRENCH)
                {
                        src = (SDL_Rect){260, 60+20*((frame/4)%8), 20, 20};
                        dest = enemy[i].pos;
                        dest.y -= BS2;
                        dest.h += BS2;
                }
                else
                {
                        src = (SDL_Rect){0+20*enemy[i].frame, enemy[i].type*20, 20, 20};
                        dest = enemy[i].pos;
                        dest.y -= BS2;
                        dest.h += BS2;
                }

                if(enemy[i].freeze)
                {
                        int f = 4 - enemy[i].freeze;
                        if(f < 0) f = 0;
                        src = (SDL_Rect){100+20*f, 140, 20, 20};
                }
                else if(enemy[i].type == PUFF)
                {
                        src = (SDL_Rect){100+20*enemy[i].frame, 140, 20, 20};
                        if(frame%8 == 0 && ++enemy[i].frame > 4)
                                enemy[i].alive = 0;
                }
                else if(enemy[i].stun > 0 && enemy[i].type != SCREW)
                {
                        if((frame/2)%2) continue;

                        dest.x += (rand()%3 - 1) * SCALE;
                        dest.y += (rand()%3 - 1) * SCALE;
                }

                SDL_RenderCopy(renderer, sprites, &src, &dest);
        }

        //draw players
        for(int i = 0; i < NR_PLAYERS; i++)
        {
                if(!player[i].alive)
                        continue;

                int animframe = 0;

                if(player[i].state == PL_STAB)
                {
                        animframe = 5;
                }
                else
                {
                        if(frame%5 == 0)
                        {
                                player[i].frame = (player[i].frame + 1) % 4;
                                if(player[i].frame == 0 && rand()%10 == 0)
                                        player[i].frame = 4;
                        }

                        animframe = player[i].frame;
                        if(player[i].vel.x == 0 && player[i].vel.y == 0 &&
                                        player[i].frame != 4)
                                animframe = 0;
                }

                src = (SDL_Rect){20+20*animframe, 60+20*player[0].dir, 20, 20};
                dest = player[i].pos;
                dest.y -= BS2;
                dest.h += BS2;

                if(!player[i].stun || (frame/2)%2)
                        SDL_RenderCopy(renderer, sprites, &src, &dest);

                if(animframe == 5)
                {
                        int screwamt = 0;
                        int retract = 0;

                        if(player[0].delay < 6)
                                retract = 6 - player[0].delay;

                        if(player[0].delay > 8)
                                screwamt = (player[0].delay/2) % 2;

                        src.x = 140 + 20*screwamt;
                        switch(player[0].dir)
                        {
                                case EAST:  dest.x += BS - retract; break;
                                case NORTH: dest.y -= BS - retract; break;
                                case WEST:  dest.x -= BS - retract; break;
                                case SOUTH: dest.y += BS - retract; break;
                        }
                        SDL_RenderCopy(renderer, sprites, &src, &dest);
                }
        }

        draw_doors_hi();

        if(drawclip) draw_clipping_boxes();

        //draw health
        int hp = player[0].hp;
        dest = (SDL_Rect){10, 10, SCALE*10, SCALE*10};
        src = (SDL_Rect){290, 140, 10, 10};
        for(int hc = 20; hc > 0; hc -= 4)
        {
                src.y = 140 + 10 * (hp > 4 ? 4 :
                                    hp < 0 ? 0 : hp);
                SDL_RenderCopy(renderer, sprites, &src, &dest);
                hp -= 4;
                dest.x += SCALE*11;
        }

        draw_map();

        SDL_RenderPresent(renderer);
}

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

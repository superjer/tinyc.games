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
#define W 900                      // window width, height
#define H 660                      // ^
#define TILESW 15                  // total room width, height
#define TILESH 11                  // ^
#define INNERTILESW 11             // inner room width, height
#define INNERTILESH 7              // ^
#define DUNH 3                     // entire dungeon width, height
#define DUNW 3                     // ^
#define STARTX 1                   // starting screen
#define STARTY 2                   // ^
#define BS 60                      // block size
#define BS2 (BS/2)                 // block size in half
#define PLYR_W BS                  // physical width and height of the player
#define PLYR_H BS2                 // ^
#define PLYR_SPD 6                 // units per frame
#define STARTPX (7*BS)             // starting position within start screen
#define STARTPY (9*BS)             // ^
#define LATERAL_STEPS 8            // how far to check for a way around an obstacle
#define NR_PLAYERS 4
#define NR_ENEMIES 8

#define PIT   0        // pit tile and edges:
#define R     1        // PIT|R for pit with right edge
#define U     2        // PIT|R|U for put with right & upper edge
#define L     4        // also works: just R|U
#define D     8        // ^
#define FACE 30        // the statue face thing
#define BLOK 45        // the bevelled block
#define CLIP 58        // invisible but solid tile
#define LASTSOLID CLIP // everything less than here is solid
#define HALFCLIP 59    // this is half solid (upper half)
#define SAND 60        // sand - can walk on like open
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
enum playerstates {PL_NORMAL, PL_STAB, PL_HURT, PL_DYING, PL_DEAD};
enum toolboxstates {TB_READY, TB_JUMP, TB_LAND, TB_OPEN, TB_SHUT, TB_HURT};

#include "odnar.c"
#include "level_data.c"

int demilitarized_zone[10000];
int layer; // current layer
int roomx; // current room x,y
int roomy;
int tiles[TILESH][TILESW];

struct point { int x, y; };

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

SDL_Event event;
SDL_Renderer *renderer;
SDL_Surface *surf;
SDL_Texture *sprites;
SDL_Texture *edgetex[20];
TTF_Font *font;

//prototypes
void setup();
void new_game();
void load_room();
int find_free_slot(int *slot);
void key_move(int down);
void update_player();
void update_enemies();
int move_player(int velx, int vely, int fake_it, int weave);
void squishy_move();
int collide(SDL_Rect plyr, SDL_Rect block);
int block_collide(int bx, int by, SDL_Rect plyr);
int world_collide(SDL_Rect plyr);
void screen_scroll(int dx, int dy);
void draw_stuff();
void draw_doors_lo();
void draw_doors_hi();
void draw_clipping_boxes();
void text(char *fstr, int value, int height);

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
                        drawclip = !drawclip;
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
        memset(player, 0, sizeof player);
        player[0].alive = 1;
        player[0].pos.x = STARTPX;
        player[0].pos.y = STARTPY;
        player[0].pos.w = PLYR_W;
        player[0].pos.h = PLYR_H;
        player[0].dir = NORTH;
        player[0].hp = 3*4;
        roomx = STARTX;
        roomy = STARTY;
        load_room();
}

void load_room()
{
        int r = roomy*DUNW + roomx; // current room coordinate

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
                enemy[i].hp = 3;
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
                        enemy[i].hp = 15;
                        enemy[i].freeze = 10;
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
                                player[0].reel = 10;
                                player[0].reeldir = WEST;
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
                                collide(player[0].hitbox, enemy[i].pos))
                {
                        enemy[i].stun = 50;
                        if(--enemy[i].hp > 0)
                        {
                                if(enemy[i].type == TOOLBOX)
                                {
                                        e->vel.y = -5;
                                        e->state = TB_HURT;
                                        e->frame = 5;
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
                                enemy[i].type = PUFF;
                                enemy[i].frame = 0;
                                enemy[i].vel.x = 0;
                                enemy[i].vel.y = 0;
                        }
                }

                switch(enemy[i].type)
                {
                        case WRENCH:
                        case PIPEWRENCH:
                                if(enemy[i].vel.x == 0)
                                {
                                        enemy[i].vel.x = rand()%2 ? -2 : 2;
                                        enemy[i].vel.y = rand()%2 ? -2 : 2;
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
                                if(frame%3 == 0)
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
                        if(world_collide(newpos))
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

//collide a rect with nearby world tiles
int world_collide(SDL_Rect plyr)
{
        for(int i = 0; i < 3; i++) for(int j = 0; j < 2; j++)
        {
                int bx = plyr.x/BS + i;
                int by = plyr.y/BS + j;

                if(block_collide(bx, by, plyr))
                        return 1;
        }

        return 0;
}

//return 0 iff we couldn't actually move
int move_player(int velx, int vely, int fake_it, int weave)
{
        SDL_Rect newpos = player[0].pos;

        newpos.x += velx;
        newpos.y += vely;

        int already_stuck = 0;
        int would_be_stuck = 0;

        if(world_collide(player[0].pos))
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
                roomx = STARTX;
                roomy = STARTY;
                player[0].pos.x = STARTPX;
                player[0].pos.y = STARTPY;
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

                if(frame%10 == 0) switch(enemy[i].type)
                {
                        case PIG:
                        case BOARD:
                                enemy[i].frame = (enemy[i].frame + 1) % 4;
                                if(enemy[i].frame == 0 && rand()%10 == 0)
                                        enemy[i].frame = 4;
                                break;
                        case SCREW:
                                enemy[i].frame = (enemy[i].frame + 1) % 6;
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
                else if(enemy[i].stun > 0)
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
                dest.x += SCALE*10;
        }

        if(drawclip) draw_clipping_boxes();

        SDL_RenderPresent(renderer);
}

void draw_doors_lo()
{
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

#ifndef TINY
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
#endif

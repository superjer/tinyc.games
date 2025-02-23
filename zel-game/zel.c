// Zel -- http://tinyc.games -- (c) 2020 Jer Wilson
//
// Zel is a "tiny" adventure game with lots of content. I'll admit it's pushing
// the boundaries of what a "tiny" game should be.

#ifndef ZEL_C
#define ZEL_C

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#define SDL_DISABLE_IMMINTRIN_H
#include <SDL.h>
#include <SDL_ttf.h>

#define SCALE 3                    // 3x magnification
#define W (300*SCALE)              // window width, height
#define H (220*SCALE)              // ^
#define W2 (W/2)
#define H2 (H/2)
#define TILESW 15                  // total room width, height
#define TILESH 11                  // ^
#define INNERTILESW 11             // inner room width, height
#define INNERTILESH 7              // ^
#define DUNW 5                     // entire dungeon width, height
#define DUNH 4                     // ^
#define BS (20*SCALE)              // block size
#define BS2 (BS/2)                 // block size in half
#define PLYR_W BS                  // physical width and height of the player
#define PLYR_H BS2                 // ^
#define PLYR_SPD 6                 // units per frame
#define LATERAL_STEPS 6            // how far to check for a way around an obstacle
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
#define PANL 70
#define RIVR 71
#define RIV2 72

#define BLOK 45        // the bevelled block
#define CLIP 120       // invisible but solid tile
#define LASTSOLID CLIP // everything less than here is solid
#define HALFCLIP 121   // this is half solid (upper half)
#define SAND 195       // sand - can walk on like open

#define SPOT 150
#define BRGE 151
#define CAVE 152
#define DIRT 165

#define OPEN 180       // invisible open, walkable space

enum enemytypes { // these match their positions in sprites.bmp
        SCREW = 120,
        BOARD = 135,
        PIG = 150,
        PUFF = 155,
        PIPEWRENCH = 165,
        WRENCH = 173,
        TOOLBOX = 1015, // +1000 means large sprites
};

enum dir {NORTH, WEST, EAST, SOUTH};
enum doors {WALL, LOCKED, SHUTTER, MAXWALL=SHUTTER, DOOR, HOLE, ENTRY, MAXDOOR};

int pct(int chance) { return rand() % 100 < chance; }

#include "ow_gen.c"
#include "level_data.c"

int inside = false;
int roomx; // current room x,y
int roomy;
int oldroomx; // previous room x,y
int oldroomy;
int entry_roomx;
int entry_roomy;
int entry_tilex;
int entry_tiley;
int tiles[TILESH][TILESW];
int tiles_old[TILESH][TILESW];
int scrollx;
int scrolly;

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
        int exists;
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
        int exists;
        int hp;
        int stun;
        int freeze;
        int harmless;
} enemy[NR_ENEMIES];

int idle_time = 30;
int global_frame = 0;
int drawclip = 0;
int noclip = false;

SDL_Event event;
SDL_Renderer *renderer;
SDL_Surface *surf;
SDL_Texture *spritetex;
SDL_Texture *tiletex;
SDL_Texture *edgetex[20];
TTF_Font *font;

//prototypes
void setup();
void enter_dungeon();
void load_room();
void key_move(int down);
int find_free_slot(int *slot);
int collide(SDL_Rect plyr, SDL_Rect block);
int block_collide(int bx, int by, SDL_Rect plyr);
int world_collide(SDL_Rect plyr);
int edge_collide(SDL_Rect plyr);
int inner_collide(SDL_Rect plyr);
void new_game();
void screen_scroll(int dx, int dy);
void update_scroll();
int sign(int n) { return n == 0 ? 0 : n > 0 ? 1 : -1; }
int xtileof(SDL_Rect pos) { return (pos.x + (pos.w / 2)) / BS; }
int ytileof(SDL_Rect pos) { return (pos.y + pos.h) / BS; }

#include "player.c"
#include "enemies.c"
#include "draw.c"

//the entry point and main game loop
int main()
{
        ow_gen();
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

                update_player(0);
                update_enemies();
                update_scroll();
                draw_stuff();
                SDL_Delay(1000 / 60);
                global_frame++;
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
        spritetex = SDL_CreateTextureFromSurface(renderer, surf);

        surf = SDL_LoadBMP("res/tiles.bmp");
        SDL_SetColorKey(surf, 1, 0xffff00);
        tiletex = SDL_CreateTextureFromSurface(renderer, surf);

        TTF_Init();
        font = TTF_OpenFont("../common/fonts/LiberationSans-Regular.ttf", 42);
}

void enter_dungeon()
{
        entry_roomx = roomx;
        entry_roomy = roomy;
        entry_tilex = xtileof(player[0].pos);
        entry_tiley = ytileof(player[0].pos);
        inside = true;
        roomx = 1;
        roomy = 2;
        player[0].pos.x = BS * (2 + 5);
        player[0].pos.y = BS * 9;
        load_room();
}

void exit_dungeon()
{
        roomx = entry_roomx;
        roomy = entry_roomy;
        player[0].pos.x = entry_tilex * BS;
        player[0].pos.y = entry_tiley * BS + BS2;
        printf("Player pos set to (%d, %d)\n", player[0].pos.x, player[0].pos.y);
        inside = false;
        load_room();
}

void key_move(int down)
{
        if (event.key.repeat) return;

        int amt = down ? PLYR_SPD : -PLYR_SPD;

        if (down)
        {
                if(event.key.keysym.sym == SDLK_UP || event.key.keysym.sym == SDLK_DOWN)
                        player[0].ylast = 1;
                else
                        player[0].ylast = 0;
        }

        switch (event.key.keysym.sym)
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
        player[0].exists = true;
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
                                c == 'V' ? WATR : //RIVR :
                                c == 'X' ? WATR : //RIV2 :
                                c == 'P' ? PANL :
                                c == 'B' ? BRGE :
                                c == 'C' ? CAVE :
                                c == '.' ? SPOT : DIRT;
                        tiles[y][x] = t;
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
                enemy[i].exists = true;
                enemy[i].hp = 1;
                enemy[i].freeze = 30 + rand() % 30;

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
                                enemy[i].pos = (SDL_Rect){BS*x, BS*y, BS, BS};
                }

                if(enemy[i].type == TOOLBOX)
                {
                        enemy[i].pos = (SDL_Rect){13*BS2, 3*BS, 2*BS, BS};
                        enemy[i].hp = 1;
                        enemy[i].freeze = 30;
                }

                if(enemy[i].type == SCREW)
                {
                        enemy[i].hp = 2;
                }
        }
}

int find_free_slot(int *slot)
{
        for (*slot = 0; *slot < NR_ENEMIES; (*slot)++)
                if (!enemy[*slot].exists)
                        return true;
        return false;
}

// collide a rect with nearby world tiles
int world_collide(SDL_Rect plyr)
{
        for (int i = 0; i < 3; i++) for (int j = 0; j < 2; j++)
        {
                int bx = plyr.x/BS + i;
                int by = plyr.y/BS + j;
                if (block_collide(bx, by, plyr))
                        return true;
        }

        return false;
}

// collide with edge of screen?
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

// collide with inner walls of inside room?
int inner_collide(SDL_Rect plyr)
{
        if (plyr.x + plyr.w >= W - BS * 2)
                return true;
        if (plyr.y + plyr.h >= H - BS * 2)
                return true;
        if (plyr.x <= BS * 2)
                return true;
        if (plyr.y <= BS * 2)
                return true;

        return false;
}

int legit_tile(int x, int y)
{
        return x >= 0 && x < TILESW && y >= 0 && y < TILESH;
}

void screen_scroll(int dx, int dy)
{
        if (inside && dy == 1 && rooms[roomy*DUNW + roomx].doors[3] == ENTRY)
        {
                exit_dungeon();
                return;
        }

        oldroomx = roomx;
        oldroomy = roomy;
        roomx += dx;
        roomy += dy;

        player[0].pos.x -= dx * (W - BS);
        player[0].pos.y -= dy * (H - BS);

        // bad room - reset to known good position
        if(roomx < 0 || roomx >= DUNW || roomy < 0 || roomy >= DUNH)
        {
                roomx = 0;
                roomy = 0;
                player[0].pos.x = BS;
                player[0].pos.y = BS;
        }

        scrollx = dx * TILESW * BS;
        scrolly = dy * TILESH * BS;
        memcpy(tiles_old, tiles, sizeof tiles);
        load_room();
}

void update_scroll()
{
        int xamt = (W - abs(abs(scrollx) - W2)) / 16 + 2;
        int yamt = (H - abs(abs(scrolly) - H2)) / 16 + 2;

        if (scrollx > xamt) scrollx -= xamt;
        else if (scrollx < -xamt) scrollx += xamt;
        else scrollx = 0;

        if (scrolly > yamt) scrolly -= yamt;
        else if (scrolly < -yamt) scrolly += yamt;
        else scrolly = 0;
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

#endif

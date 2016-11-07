#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <SDL.h>
#include <SDL_ttf.h>

#define W 600                      // window width, height
#define H 440                      // ^
#define TILESW 15                  // total room width, height
#define TILESH 11                  // ^
#define INNERTILESW 11             // inner room width, height
#define INNERTILESH 7              // ^
#define DUNH 3                     // entire dungeon width, height
#define DUNW 3                     // ^
#define STARTX 1                   // starting screen
#define STARTY 2                   // ^
#define BS 40                      // block size
#define BS2 (BS/2)                 //block size in half
#define PLYR_W BS                  // physical width and height of the player
#define PLYR_H BS2                 // ^
#define PLYR_SPD 4                 // units per frame
#define STARTPX (7*BS)             // starting position within start screen
#define STARTPY (9*BS)             // ^
#define LATERAL_STEPS 10           // how far to check for a way around an obstacle

#define PIT   0        // pit tile and edges:
#define R     1        // PIT|R for pit with right edge
#define U     2        // PIT|R|U for put with right & upper edge
#define L     4        // also works: just R|U
#define D     8        // ^
#define FACE 30        // the statue face thing
#define BLOK 45        // the bevelled block
#define CLIP 58        // invisible but solid tile
#define LASTSOLID CLIP // everything below here is solid
#define HALFCLIP 59    // this is half solid (upper half)
#define SAND 60        // sand - can walk on like open
#define OPEN 75        // invisible open, walkable space

enum flags {
        ALIVE = 1,
};

enum doors {WALL=0, HOLE, DOOR, LOCKED, SHUTTER, ENTRY, MAXDOOR};

struct room {
        int doors[4];
        int enemies[5];
        int treasure;
        int tiles[INNERTILESH * INNERTILESW];
} rooms[DUNH * DUNW] = {{
        {HOLE, WALL, WALL, DOOR}, // doors for room 0,0
        {0,0,0,0,0}, // enemies
        0, // treasure
        {
                OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN,
                OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN,
                OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN,
                OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN,
                OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN,
                OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN,
                OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN,
        },
},{
        {DOOR, WALL, HOLE, DOOR}, // doors for room 0,1
        {0,0,0,0,0}, // enemies
        0, // treasure
        {
                OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN,
                OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN,
                OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN,
                OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN,
                OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN,
                OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN,
                OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN,
        },
},{
        {WALL, WALL, DOOR, SHUTTER}, // doors for room 0,2
        {0,0,0,0,0}, // enemies
        0, // treasure
        {
                OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN,
                OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN,
                OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN,
                OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN,
                OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN,
                OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN,
                OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN,
        },
},{
        {DOOR, DOOR, WALL, LOCKED}, // doors for room 1,0
        {0,0,0,0,0}, // enemies
        0, // treasure
        {
                OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN,
                OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN,
                OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN,
                OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN,
                OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN,
                OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN,
                OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN,
        },
},{
        {DOOR, DOOR, DOOR, DOOR}, // doors for room 1,1
        {0,0,0,0,0}, // enemies
        0, // treasure
        {
                OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN,
                OPEN, L|U,  PIT|U,PIT|U,PIT|U,PIT|U,PIT|U,PIT|U,PIT|U,U|R,  OPEN,
                OPEN, PIT|L,PIT,  PIT,  PIT,  PIT,  PIT,  PIT,  PIT,  PIT|R,OPEN,
                OPEN, PIT|L,PIT,  FACE, PIT,  PIT,  PIT,  FACE, PIT,  PIT|R,OPEN,
                OPEN, PIT|L,PIT,  PIT|U,PIT,  PIT,  PIT,  PIT|U,PIT,  PIT|R,OPEN,
                OPEN, L|D,  PIT|D,PIT|D,PIT|D,PIT|D,PIT|D,PIT|D,PIT|D,D|R,  OPEN,
                OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN,
        },
},{
        {WALL, SHUTTER, DOOR, HOLE}, // doors for room 1,2
        {0,0,0,0,0}, // enemies
        0, // treasure
        {
                OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN,
                OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN,
                OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN,
                OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN,
                OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN,
                OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN,
                OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN,
        },
},{
        {DOOR, LOCKED, WALL, WALL}, // doors for room 2,0
        {0,0,0,0,0}, // enemies
        0, // treasure
        {
                OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN,
                OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN,
                OPEN, OPEN, OPEN, BLOK, BLOK, BLOK, BLOK, BLOK, OPEN, OPEN, OPEN,
                OPEN, OPEN, OPEN, BLOK, BLOK, BLOK, BLOK, BLOK, OPEN, OPEN, OPEN,
                OPEN, OPEN, OPEN, BLOK, BLOK, BLOK, BLOK, BLOK, OPEN, OPEN, OPEN,
                OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN,
                OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN,
        },
},{
        {DOOR, DOOR, DOOR, ENTRY}, // doors for room 2,1
        {0,0,0,0,0}, // enemies
        0, // treasure
        {
                OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN,
                OPEN, FACE, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, FACE, OPEN,
                OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN, OPEN,
                OPEN, OPEN, OPEN, OPEN, SAND, SAND, SAND, OPEN, OPEN, OPEN, OPEN,
                OPEN, OPEN, OPEN, SAND, SAND, SAND, SAND, SAND, OPEN, OPEN, OPEN,
                BLOK, FACE, OPEN, SAND, OPEN, OPEN, OPEN, SAND, OPEN, FACE, BLOK,
                BLOK, BLOK, OPEN, SAND, OPEN, OPEN, OPEN, SAND, OPEN, BLOK, BLOK,
        },
},{
        {WALL, HOLE, DOOR, WALL}, // doors for room 2,2
        {0,0,0,0,0}, // enemies
        0, // treasure
        {
                SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND,
                SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND,
                SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND,
                SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND,
                SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND,
                SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND,
                SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND,
        },
}};

int roomx; // current room x,y
int roomy;
int tiles[TILESH][TILESW];

struct player {
        SDL_Rect pos;
        struct {
                int x;
                int y;
        } vel;
        int ylast; // moved in y direction last?
        int hearts;
        int bombs;
        int money;
        int flags;
} player[4];

struct enemy {
        SDL_Rect pos;
        struct {
                int x;
                int y;
        } vel;
        int frame;
        int flags;
} enemy[10];

int nr_players = 1;
int idle_time = 30;
int frame = 0;
int drawclip = 0;
int bx0, by0, bx1, by1; // for squishy_move

SDL_Event event;
SDL_Renderer *renderer;
SDL_Surface *surf;
SDL_Texture *sprites;
SDL_Texture *edgetex[20];
TTF_Font *font;

void setup();
void new_game();
void load_room();
void key_move(int down);
void update_stuff();
int move_player(int velx, int vely, int fake_it, int weave);
void squishy_move();
int collide(SDL_Rect r0, SDL_Rect r1);
int block_collide(int bx, int by, SDL_Rect plyr);
int world_collide(SDL_Rect plyr);
void scroll(int dx, int dy);
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
        if(event.key.repeat)
                return;

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
        memset(player, 0, sizeof *player);
        player[0].flags |= ALIVE;
        player[0].pos.x = STARTPX;
        player[0].pos.y = STARTPY;
        player[0].pos.w = PLYR_W;
        player[0].pos.h = PLYR_H;
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
                        tiles[y][x] = rooms[r].tiles[(y-2)*INNERTILESW + (x-2)];
        }

        //load all the doors
        if(rooms[r].doors[0] == LOCKED || rooms[r].doors[0] == SHUTTER || rooms[r].doors[0] == WALL)
                tiles[5][13] = CLIP;

        if(rooms[r].doors[1] == LOCKED || rooms[r].doors[1] == SHUTTER || rooms[r].doors[1] == WALL)
                tiles[1][7] = CLIP;

        if(rooms[r].doors[2] == LOCKED || rooms[r].doors[2] == SHUTTER || rooms[r].doors[2] == WALL)
                tiles[5][1] = CLIP;

        if(rooms[r].doors[3] == LOCKED || rooms[r].doors[3] == SHUTTER || rooms[r].doors[3] == WALL)
                tiles[9][7] = CLIP;

        //spawn enemies
        memset(enemy, 0, sizeof *enemy);
        enemy[0].flags |= ALIVE;
        enemy[0].pos = (SDL_Rect){100, 100, BS, BS2};
        enemy[1].flags |= ALIVE;
        enemy[1].pos = (SDL_Rect){200, 100, BS, BS2};
        enemy[2].flags |= ALIVE;
        enemy[2].pos = (SDL_Rect){300, 100, BS, BS2};
}

void update_player()
{
        struct player *p = player + 0;

        if(!p->vel.x ^ !p->vel.y) // moving only one direction
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

        //check for leaving screen
        if(p->pos.x <= 4)
                scroll(-1, 0);
        else if(p->pos.x >= W - PLYR_W - 4)
                scroll(1, 0);
        if(p->pos.y <= 4)
                scroll(0, -1);
        else if(p->pos.y >= H - PLYR_H - 4)
                scroll(0, 1);
}

void update_enemies()
{
        for(int i = 0; i < 10; i++)
        {
                if(!(enemy[i].flags & ALIVE))
                        continue;

                if(enemy[i].vel.x == 0 && rand()%10 == 0)
                {
                        enemy[i].vel.x = (rand()%2 * 4) - 2;
                        enemy[i].vel.y = (rand()%2 * 4) - 2;
                }

                SDL_Rect newpos = enemy[i].pos;
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


//update everything that needs to update on its own, without input
void update_stuff()
{
        update_player();
        update_enemies();
}

//collide a rect with nearby world tiles
int world_collide(SDL_Rect plyr)
{
        for(int i = 0; i < 2; i++) for(int j = 0; j < 2; j++)
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

void scroll(int dx, int dy)
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

        int r = roomy*DUNW + roomx; // current room coordinate

        //draw right edge
        int *doors = rooms[r].doors;
        src  = (SDL_Rect){13*20, 4*20, 2*20, 3*20};
        dest = (SDL_Rect){13*BS, 4*BS, 2*BS, 3*BS};
        SDL_RenderCopy(renderer, edgetex[doors[0]], &src, &dest);

        //draw top edge
        src  = (SDL_Rect){6*20, 0*20, 3*20, 2*20};
        dest = (SDL_Rect){6*BS, 0*BS, 3*BS, 2*BS};
        SDL_RenderCopy(renderer, edgetex[doors[1]], &src, &dest);

        //draw left edge
        src  = (SDL_Rect){0*20, 4*20, 2*20, 3*20};
        dest = (SDL_Rect){0*BS, 4*BS, 2*BS, 3*BS};
        SDL_RenderCopy(renderer, edgetex[doors[2]], &src, &dest);

        //draw bottom edge
        src  = (SDL_Rect){6*20, 7*20, 3*20, 4*20};
        dest = (SDL_Rect){6*BS, 7*BS, 3*BS, 4*BS};
        SDL_RenderCopy(renderer, edgetex[doors[3]], &src, &dest);

        //draw room tiles
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
        for(int x = 0; x < TILESW; x++) for(int y = 0; y < TILESH; y++)
        {
                int t = tiles[y][x];
                if(t != OPEN && t != CLIP)
                        SDL_RenderCopy(renderer, sprites,
                                &(SDL_Rect){20*(t%15), 20*(t/15), 20, 20},
                                &(SDL_Rect){BS*x, BS*y, BS, BS});
        }

        //draw enemies
        for(int i = 0; i < 10; i++)
        {
                if(frame%10 == 0)
                {
                        enemy[i].frame = (enemy[i].frame + 1) % 4;
                        if(enemy[i].frame == 0 && rand()%3 == 0)
                                enemy[i].frame = 4;
                }

                src = (SDL_Rect){0+20*enemy[i].frame, 7*20, 20, 20};
                dest = enemy[i].pos;
                dest.y -= 20;
                dest.h += 20;
                SDL_RenderCopy(renderer, sprites, &src, &dest);
        }

        //draw players
        for(int i = 0; i < 4; i++)
        {
                if(!(player[i].flags & ALIVE))
                        continue;
                src = (SDL_Rect){0, 6*20, 20, 20};
                dest = player[i].pos;
                dest.y -= 20;
                dest.h += 20;
                SDL_RenderCopy(renderer, sprites, &src, &dest);
        }

        //draw right edge ABOVE
        src  = (SDL_Rect){14*20, 4*20, 1*20, 3*20};
        dest = (SDL_Rect){14*BS, 4*BS, 1*BS, 3*BS};
        SDL_RenderCopy(renderer, edgetex[doors[0]], &src, &dest);

        //draw top edge ABOVE
        src  = (SDL_Rect){6*20, 0*20, 3*20, 1*20};
        dest = (SDL_Rect){6*BS, 0*BS, 3*BS, 1*BS};
        SDL_RenderCopy(renderer, edgetex[doors[1]], &src, &dest);

        //draw left edge ABOVE
        src  = (SDL_Rect){0*20, 4*20, 1*20, 3*20};
        dest = (SDL_Rect){0*BS, 4*BS, 1*BS, 3*BS};
        SDL_RenderCopy(renderer, edgetex[doors[2]], &src, &dest);

        //draw bottom edge ABOVE
        src  = (SDL_Rect){6*20, 10*20, 3*20, 1*20};
        dest = (SDL_Rect){6*BS, 10*BS, 3*BS, 1*BS};
        SDL_RenderCopy(renderer, edgetex[doors[3]], &src, &dest);

        //draw clipping boxes
        if(drawclip)
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
                SDL_SetRenderDrawColor(renderer, 255, 127, 0, 255);
                SDL_RenderFillRect(renderer, &(SDL_Rect){BS*bx0+1, BS*by0+1, BS-1, BS-1});
                SDL_SetRenderDrawColor(renderer, 127, 255, 0, 255);
                SDL_RenderFillRect(renderer, &(SDL_Rect){BS*bx1+1, BS*by1+1, BS-1, BS-1});
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

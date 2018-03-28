// Blocko -- http://tinyc.games -- (c) 2018 Jer Wilson
//
// Blocko is a barebones 3D platformer using OpenGL via GLEW.
//
// Using OpenGL on Windows requires the Windows SDK.
// The run-windows.bat script will try hard to find the SDK files it needs,
// otherwise it will tell you what to do.

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#define GL3_PROTOTYPES 1
#include <GL/glew.h>
#include <SDL.h>

#define SCALE 3                    // x magnification
#define W (300*SCALE)              // window width, height
#define H (220*SCALE)              // ^
#define TILESW 15                  // total level width, height
#define TILESH 11                  // ^
#define BS (20*SCALE)              // block size
#define BS2 (BS/2)                 // block size in half
#define PLYR_W (16*SCALE)          // physical width and height of the player
#define PLYR_H (18*SCALE)          // ^
#define PLYR_SPD (2*SCALE)         // units per frame
#define STARTPX (3*BS)             // starting position within start screen
#define STARTPY (8*BS)             // ^
#define LATERAL_STEPS 8           // how far to check for a way around an obstacle
#define NR_PLAYERS 1
#define NR_ENEMIES 8
#define GRAV_JUMP 0
#define GRAV_ZERO 24
#define GRAV_MAX 42

#define BLOK 45        // the bevelled block
#define LASTSOLID BLOK // everything less than here is solid
#define HALFCLIP 59    // this is half solid (upper half)
#define OPEN 75        // invisible open, walkable space

int gravity[] = { -30,-27,-24,-21,-19,-17,-15,-13,-11,-10,
                   -9, -8, -7, -6, -5, -4, -4, -3, -3, -2,
                   -2, -1, -1, -1,  0,  1,  2,  3,  4,  5,
                    6,  7,  8,  9, 10, 11, 12, 13, 14, 16,
                   18, 20, 22, };

enum enemytypes {
        PIG = 7,
        PUFF = 12,
};

enum dir {NORTH, WEST, EAST, SOUTH};
enum playerstates {PL_NORMAL, PL_HURT, PL_DYING, PL_DEAD};

int tiles[TILESH][TILESW];

struct point { int x, y; };

struct player {
        SDL_Rect pos;
        SDL_Rect hitbox;
        struct point vel;
        int goingl;
        int goingr;
        int grav;
        int ground;
        int reel;
        int reeldir;
        int dir;
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
        int goingl;
        int goingr;
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
SDL_Window *win;
SDL_GLContext ctx;
SDL_Renderer *renderer;
SDL_Surface *surf;
SDL_Texture *sprites;

//prototypes
void setup();
void new_game();
void load_level();
void key_move(int down);
void update_player();
void update_enemies();
int move_player(int velx, int vely);
int collide(SDL_Rect plyr, SDL_Rect block);
int block_collide(int bx, int by, SDL_Rect plyr);
int world_collide(SDL_Rect plyr);
void draw_stuff();

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
        win = SDL_CreateWindow("Blocko",
                SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, W, H, SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL);
        if(!win) exit(fprintf(stderr, "%s\n", SDL_GetError()));
        ctx = SDL_GL_CreateContext(win);
        if(!ctx) exit(fprintf(stderr, "Could not create GL context\n"));
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        SDL_GL_SetSwapInterval(1);
        #ifndef __APPLE__
        glewExperimental = GL_TRUE;
        glewInit();
        #endif

        surf = SDL_LoadBMP("res/sprites.bmp");
        SDL_SetColorKey(surf, 1, 0xffff00);
        sprites = SDL_CreateTextureFromSurface(renderer, surf);
}

void key_move(int down)
{
        if(event.key.repeat) return;

        switch(event.key.keysym.sym)
        {
                case SDLK_UP:
                        break;
                case SDLK_DOWN:
                        break;
                case SDLK_LEFT:
                        player[0].goingl = down;
                        if(down) player[0].dir = WEST;
                        break;
                case SDLK_RIGHT:
                        player[0].goingr = down;
                        if(down) player[0].dir = EAST;
                        break;
                case SDLK_SPACE:
                        drawclip = !drawclip;
                        break;
                case SDLK_z:
                case SDLK_x:
                        if(player[0].state == PL_NORMAL
                                && player[0].ground
                                && down)
                        {
                                player[0].grav = GRAV_JUMP;
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
        player[0].grav = GRAV_ZERO;
        load_level();
}

void load_level()
{
        for(int x = 0; x < TILESW; x++) for(int y = 0; y < TILESH; y++)
        {
                if(y > TILESH - 3)
                        tiles[y][x] = rand() % 8 == 1 ? OPEN : BLOK;
                else
                        tiles[y][x] = rand() % 8 == 1 ? BLOK : OPEN;
        }

        //load enemies
        memset(enemy, 0, sizeof enemy);
        for(int i = 0; i < NR_ENEMIES && i < 3; i++)
        {
                enemy[i].type = PIG;
                enemy[i].alive = 1;
                enemy[i].hp = 3;
                enemy[i].freeze = 50;

                //find a good spawn position
                int x = i*4 + 2;
                int y = 0;
                enemy[i].pos = (SDL_Rect){BS*x, BS*y + BS2, BS, BS2};
        }
}

void update_player()
{
        struct player *p = player + 0;

        if(player[0].stun > 0)
                player[0].stun--;

        if(player[0].state == PL_DEAD || player[0].pos.y > H + 100)
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

        if(p->goingl && !p->goingr)
                p->vel.x--;
        else if(p->vel.x < 0)
                p->vel.x++;

        if(p->goingr && !p->goingl)
                p->vel.x++;
        else if(p->vel.x > 0)
                p->vel.x--;

        if(p->vel.x > PLYR_SPD)
                p->vel.x = PLYR_SPD;

        if(p->vel.x < -PLYR_SPD)
                p->vel.x = -PLYR_SPD;

        if(!move_player(p->vel.x, p->vel.y))
                p->vel.x = 0;

        //gravity
        if(!p->ground || p->grav < GRAV_ZERO)
        {
                if(!move_player(0, gravity[p->grav]))
                        p->grav = GRAV_ZERO;
                else if(p->grav < GRAV_MAX)
                        p->grav++;
        }

        //detect ground
        SDL_Rect foot = (SDL_Rect){p->pos.x, p->pos.y + p->pos.h, p->pos.w, 1};
        p->ground = world_collide(foot);

        if(p->ground)
                p->grav = GRAV_ZERO;

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

                if(enemy[i].vel.y < 10)
                        enemy[i].vel.y += 1;

                if(enemy[i].goingr)
                        enemy[i].vel.x = 1;
                else if(enemy[i].goingl)
                        enemy[i].vel.x = -1;
                else
                        enemy[i].vel.x = 1;

                SDL_Rect newpos = enemy[i].pos;
                newpos.y += enemy[i].vel.y;

                // try falling
                if(world_collide(newpos))
                {
                        if(!enemy[i].goingl)
                                enemy[i].goingr = 1;
                        enemy[i].vel.y = 0;
                }
                else
                {
                        enemy[i].pos = newpos;
                }

                newpos = enemy[i].pos;
                newpos.x += enemy[i].vel.x;

                if(world_collide(newpos))
                {
                        if(enemy[i].goingr)
                        {
                                enemy[i].goingl = 1;
                                enemy[i].goingr = 0;
                        }
                        else
                        {
                                enemy[i].goingl = 0;
                                enemy[i].goingr = 1;
                        }
                }
                else
                {
                        enemy[i].pos = newpos;
                }

                //check if enemy fell too far
                if(enemy[i].pos.y > H + 100)
                {
                        enemy[i].pos.y = 0;
                }

                // or went left/right too far
                if(enemy[i].pos.x > W + 100)
                {
                        enemy[i].pos.x = 0;
                }

                if(enemy[i].pos.x < -100)
                {
                        enemy[i].pos.x = W-BS;
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
int move_player(int velx, int vely)
{
        int last_was_x = 0;
        int already_stuck = 0;
        int moved = 0;

        if(!velx && !vely)
                return 1;

        if(world_collide(player[0].pos))
                already_stuck = 1;

        while(velx || vely)
        {
                SDL_Rect testpos = player[0].pos;
                int amt;

                if(!velx || last_was_x && vely)
                {
                        amt = vely > 0 ? 1 : -1;
                        testpos.y += amt;
                        vely -= amt;
                        last_was_x = 0;
                }
                else
                {
                        amt = velx > 0 ? 1 : -1;
                        testpos.x += amt;
                        velx -= amt;
                        last_was_x = 1;
                }

                int would_be_stuck = 0;

                if(world_collide(testpos))
                        would_be_stuck = 1;
                else
                        already_stuck = 0;

                if(would_be_stuck && !already_stuck)
                {
                        if(last_was_x)
                                velx = 0;
                        else
                                vely = 0;
                        continue;
                }

                player[0].pos = testpos;
                moved = 1;
        }

        return moved;
}

int legit_tile(int x, int y)
{
        return x >= 0 && x < TILESW && y >= 0 && y < TILESH;
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

void rainbowbox(float x, float y, float z)
{
        glBegin(GL_TRIANGLE_FAN);
        glColor3f(0, 0, 0); glVertex3f(x*BS   ,y*BS   ,z*BS   );
        glColor3f(1, 0, 0); glVertex3f(x*BS+BS,y*BS   ,z*BS   );
        glColor3f(1, 0, 1); glVertex3f(x*BS+BS,y*BS   ,z*BS+BS);
        glColor3f(0, 0, 1); glVertex3f(x*BS   ,y*BS   ,z*BS+BS);
        glColor3f(0, 1, 1); glVertex3f(x*BS   ,y*BS+BS,z*BS+BS);
        glColor3f(0, 1, 0); glVertex3f(x*BS   ,y*BS+BS,z*BS   );
        glColor3f(1, 1, 0); glVertex3f(x*BS+BS,y*BS+BS,z*BS   );
        glColor3f(1, 0, 0); glVertex3f(x*BS+BS,y*BS   ,z*BS   );
        glEnd();
        glBegin(GL_TRIANGLE_FAN);
        glColor3f(1, 1, 1); glVertex3f(x*BS+BS,y*BS+BS,z*BS+BS);
        glColor3f(0, 1, 1); glVertex3f(x*BS   ,y*BS+BS,z*BS+BS);
        glColor3f(0, 1, 0); glVertex3f(x*BS   ,y*BS+BS,z*BS   );
        glColor3f(1, 1, 0); glVertex3f(x*BS+BS,y*BS+BS,z*BS   );
        glColor3f(1, 0, 0); glVertex3f(x*BS+BS,y*BS   ,z*BS   );
        glColor3f(1, 0, 1); glVertex3f(x*BS+BS,y*BS   ,z*BS+BS);
        glColor3f(0, 0, 1); glVertex3f(x*BS   ,y*BS   ,z*BS+BS);
        glColor3f(0, 1, 1); glVertex3f(x*BS   ,y*BS+BS,z*BS+BS);
        glEnd();
}

void graybox(float x, float y, float z)
{
        glBegin(GL_TRIANGLE_FAN);
        glColor3f(0.4, 0.4, 0.4); glVertex3f(x*BS   ,y*BS   ,z*BS   );
        glColor3f(0.5, 0.4, 0.4); glVertex3f(x*BS+BS,y*BS   ,z*BS   );
        glColor3f(0.5, 0.4, 0.5); glVertex3f(x*BS+BS,y*BS   ,z*BS+BS);
        glColor3f(0.4, 0.4, 0.5); glVertex3f(x*BS   ,y*BS   ,z*BS+BS);
        glColor3f(0.4, 0.5, 0.5); glVertex3f(x*BS   ,y*BS+BS,z*BS+BS);
        glColor3f(0.4, 0.5, 0.4); glVertex3f(x*BS   ,y*BS+BS,z*BS   );
        glColor3f(0.5, 0.5, 0.4); glVertex3f(x*BS+BS,y*BS+BS,z*BS   );
        glColor3f(0.5, 0.4, 0.4); glVertex3f(x*BS+BS,y*BS   ,z*BS   );
        glEnd();
        glBegin(GL_TRIANGLE_FAN);
        glColor3f(0.5, 0.5, 0.5); glVertex3f(x*BS+BS,y*BS+BS,z*BS+BS);
        glColor3f(0.4, 0.5, 0.5); glVertex3f(x*BS   ,y*BS+BS,z*BS+BS);
        glColor3f(0.4, 0.5, 0.4); glVertex3f(x*BS   ,y*BS+BS,z*BS   );
        glColor3f(0.5, 0.5, 0.4); glVertex3f(x*BS+BS,y*BS+BS,z*BS   );
        glColor3f(0.5, 0.4, 0.4); glVertex3f(x*BS+BS,y*BS   ,z*BS   );
        glColor3f(0.5, 0.4, 0.5); glVertex3f(x*BS+BS,y*BS   ,z*BS+BS);
        glColor3f(0.4, 0.4, 0.5); glVertex3f(x*BS   ,y*BS   ,z*BS+BS);
        glColor3f(0.4, 0.5, 0.5); glVertex3f(x*BS   ,y*BS+BS,z*BS+BS);
        glEnd();
}

void redbox(float x, float y, float z)
{
        glBegin(GL_TRIANGLE_FAN);
        glColor3f(0.7, 0.1, 0.1); glVertex3f(x*BS   ,y*BS   ,z*BS   );
        glColor3f(0.8, 0.1, 0.1); glVertex3f(x*BS+BS,y*BS   ,z*BS   );
        glColor3f(0.8, 0.1, 0.2); glVertex3f(x*BS+BS,y*BS   ,z*BS+BS);
        glColor3f(0.7, 0.1, 0.2); glVertex3f(x*BS   ,y*BS   ,z*BS+BS);
        glColor3f(0.7, 0.2, 0.2); glVertex3f(x*BS   ,y*BS+BS,z*BS+BS);
        glColor3f(0.7, 0.2, 0.1); glVertex3f(x*BS   ,y*BS+BS,z*BS   );
        glColor3f(0.8, 0.2, 0.1); glVertex3f(x*BS+BS,y*BS+BS,z*BS   );
        glColor3f(0.8, 0.1, 0.1); glVertex3f(x*BS+BS,y*BS   ,z*BS   );
        glEnd();
        glBegin(GL_TRIANGLE_FAN);
        glColor3f(0.8, 0.2, 0.2); glVertex3f(x*BS+BS,y*BS+BS,z*BS+BS);
        glColor3f(0.7, 0.2, 0.2); glVertex3f(x*BS   ,y*BS+BS,z*BS+BS);
        glColor3f(0.7, 0.2, 0.1); glVertex3f(x*BS   ,y*BS+BS,z*BS   );
        glColor3f(0.8, 0.2, 0.1); glVertex3f(x*BS+BS,y*BS+BS,z*BS   );
        glColor3f(0.8, 0.1, 0.1); glVertex3f(x*BS+BS,y*BS   ,z*BS   );
        glColor3f(0.8, 0.1, 0.2); glVertex3f(x*BS+BS,y*BS   ,z*BS+BS);
        glColor3f(0.7, 0.1, 0.2); glVertex3f(x*BS   ,y*BS   ,z*BS+BS);
        glColor3f(0.7, 0.2, 0.2); glVertex3f(x*BS   ,y*BS+BS,z*BS+BS);
        glEnd();
}

//draw everything in the game on the screen
void draw_stuff()
{
        glClearColor(0.05, 0.07, 0.03, 1.0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glFrustum(-16, 16, -9, 9, 16, 9999);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        static float theta = 1.55;
        /*
        theta += 0.01;
        if(theta > 2.9)
                theta = 0.2;
        */
        float e0 = (TILESW/2)*BS, e1 = (TILESH/2-1)*BS, e2 = -10*BS;
        float t0 = e0 + cos(theta), t1 = e1 + 0.1, t2 = e2 + sin(theta);
        float f0 = t0-e0, f1 = t1-e1, f2 = t2-e2;
        float fm = sqrt(f0*f0 + f1*f1 + f2*f2);
        f0 /= fm;
        f1 /= fm;
        f2 /= fm;
        float s0 = f1*0 - f2*-1, s1 = f2*0 - f0*0, s2 = f0*-1 - f1*0;
        float sm = sqrt(s0*s0 + s1*s1 + s2*s2);
        float z0 = s0/sm;
        float z1 = s1/sm;
        float z2 = s2/sm;
        float u0 = z1*f2 - z2*f1, u1 = z2*f0 - z0*f2, u2 = z0*f1 - z1*f0;
        float M[] = {
                s0, u0,-f0, 0,
                s1, u1,-f1, 0,
                s2, u2,-f2, 0,
                 0,  0,  0, 1
        };
        glMultMatrixf(M);
        glTranslated(-e0, -e1, -e2);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);

        for(int x = 0; x < TILESW; x++) for(int y = 0; y < TILESH; y++) for(int z = 0; z < 1; z++)
        {
                int t = tiles[y][x];
                if(t != OPEN)
                        graybox(x, y, z);
        }

        SDL_Rect src, dest;

        //draw enemies
        for(int i = 0; i < NR_ENEMIES; i++)
        {
                if(!enemy[i].alive)
                        continue;

                if(frame%10 == 0) switch(enemy[i].type)
                {
                        case PIG:
                                enemy[i].frame = (enemy[i].frame + 1) % 4;
                                if(enemy[i].frame == 0 && rand()%10 == 0)
                                        enemy[i].frame = 4;
                                break;
                }

                src = (SDL_Rect){0+20*enemy[i].frame, enemy[i].type*20, 20, 20};
                dest = enemy[i].pos;
                dest.y -= BS2;
                dest.h += BS2;

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
                redbox((float)dest.x / BS, (float)dest.y / BS, 0);
        }

        //draw players
        for(int i = 0; i < NR_PLAYERS; i++)
        {
                if(!player[i].alive)
                        continue;

                int animframe = 0;

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

                src = (SDL_Rect){20+20*animframe, 60+20*player[0].dir, 20, 20};
                dest = player[i].pos;
                dest.y -= BS - PLYR_H;
                dest.x -= (BS - PLYR_W) / 2;
                dest.w += (BS - PLYR_W) / 2;
                dest.h = BS;

                rainbowbox((float)dest.x / BS, (float)dest.y / BS, 0);
        }

        SDL_GL_SwapWindow(win);
}

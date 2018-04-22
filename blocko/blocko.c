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

#ifdef __APPLE__
#include <gl.h>
#else
#include <GL/glew.h>
#endif

#include <SDL.h>
#define STBI_NO_SIMD
#define STB_IMAGE_IMPLEMENTATION
#include "../_stb/stb_image.h"

#define SCALE 3                    // x magnification
#define W 1366                     // window width, height
#define H 768                      // ^
#define TILESW 45                  // total level width, height
#define TILESH 11                  // ^
#define TILESD 45                  // ^
#define BS (20*SCALE)              // block size
#define BS2 (BS/2)                 // block size in half
#define PLYR_W (16*SCALE)          // physical width and height of the player
#define PLYR_H (BS)                // ^
#define PLYR_SPD (2*SCALE)         // units per frame
#define STARTPX (3*BS)             // starting position within start screen
#define STARTPY (4*BS)             // ^
#define STARTPZ 0                  // ^
#define NR_PLAYERS 1
#define GRAV_JUMP 0
#define GRAV_ZERO 24
#define GRAV_MAX 42

#define GRAS 46
#define LASTSOLID GRAS // everything less than here is solid
#define OPEN 75        // invisible open, walkable space

int gravity[] = { -30,-27,-24,-21,-19,-17,-15,-13,-11,-10,
                   -9, -8, -7, -6, -5, -4, -4, -3, -3, -2,
                   -2, -1, -1, -1,  0,  1,  2,  3,  4,  5,
                    6,  7,  8,  9, 10, 11, 12, 13, 14, 16,
                   18, 20, 22, };

int tiles[TILESD][TILESH][TILESW];

struct box { float x, y, z, w, h ,d; };
struct point { float x, y, z; };

struct player {
        struct box pos;
        struct point vel;
        float yaw;
        float pitch;
        int goingf;
        int goingb;
        int goingl;
        int goingr;
        int fvel;
        int rvel;
        int grav;
        int ground;
} player[NR_PLAYERS];

int idle_time = 30;
int frame = 0;

int mouselook = 1;
int target_x, target_y, target_z;
int place_x, place_y, place_z;
int screenw = W;
int screenh = H;

SDL_Event event;
SDL_Window *win;
SDL_GLContext ctx;
SDL_Renderer *renderer;
SDL_Surface *surf;

SDL_Texture *top;
SDL_Texture *side;
SDL_Texture *bottom;

//prototypes
void setup();
void resize();
void new_game();
void load_level();
void key_move(int down);
void mouse_move();
void mouse_button(int down);
void update_player();
int move_player(int velx, int vely, int velz);
int collide(struct box plyr, struct box block);
int block_collide(int bx, int by, int bz, struct box plyr);
int world_collide(struct box plyr);
void draw_stuff();
void debrief();

//the entry point and main game loop
int main()
{
        setup();
        new_game();

        for(;;)
        {
                while(SDL_PollEvent(&event)) switch(event.type)
                {
                        case SDL_QUIT:            exit(0);
                        case SDL_KEYDOWN:         key_move(1);       break;
                        case SDL_KEYUP:           key_move(0);       break;
                        case SDL_MOUSEMOTION:     mouse_move();      break;
                        case SDL_MOUSEBUTTONDOWN: mouse_button(1);   break;
                        case SDL_MOUSEBUTTONUP:   mouse_button(0);   break;
                        case SDL_WINDOWEVENT:
                                switch(event.window.event) {
                                        case SDL_WINDOWEVENT_SIZE_CHANGED:
                                                resize();
                                                break;
                                }
                                break;
                }

                update_player();
                draw_stuff();
                debrief();
                SDL_Delay(1000 / 60);
                frame++;
        }
}

//initial setup to get the window and rendering going
void setup()
{
        srand(time(NULL));

        SDL_Init(SDL_INIT_VIDEO);
        win = SDL_CreateWindow("Blocko", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                W, H, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL);
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

        // load all the textures
        int x, y, n, mode;
        GLuint texid = 0;
        unsigned char *pixels;
        char *files[] = { "res/top.png", "res/side.png", "res/bottom.png", "" };
        for(int f = 0; files[f][0]; f++)
        {
                pixels = stbi_load(files[f], &x, &y, &n, 0);
                glGenTextures(1, &texid);
                glBindTexture(GL_TEXTURE_2D, texid);
                mode = n == 4 ? GL_RGBA : GL_RGB;
                glTexImage2D(GL_TEXTURE_2D, 0, mode, x, y, 0, mode, GL_UNSIGNED_BYTE, pixels);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        }

        SDL_SetRelativeMouseMode(SDL_TRUE);
}

void resize()
{
        screenw = event.window.data1;
        screenh = event.window.data2;
}

void key_move(int down)
{
        if(event.key.repeat) return;

        switch(event.key.keysym.sym)
        {
                case SDLK_w:
                        player[0].goingf = down;
                        break;
                case SDLK_s:
                        player[0].goingb = down;
                        break;
                case SDLK_a:
                        player[0].goingl = down;
                        break;
                case SDLK_d:
                        player[0].goingr = down;
                        break;
                case SDLK_SPACE:
                        if(player[0].ground && down)
                                player[0].grav = GRAV_JUMP;
                        break;
                case SDLK_ESCAPE:
                        SDL_SetRelativeMouseMode(SDL_FALSE);
                        mouselook = 0;
                        break;
        }
}

void mouse_move()
{
        if(!mouselook) return;

        float pitchlimit = 3.1415926535 * 0.5 - 0.001;
        player[0].yaw += event.motion.xrel * 0.001;
        player[0].pitch += event.motion.yrel * 0.001;

        if(player[0].pitch > pitchlimit)
                player[0].pitch = pitchlimit;

        if(player[0].pitch < -pitchlimit)
                player[0].pitch = -pitchlimit;
}

void mouse_button(int down)
{
        if(!down) return;

        if(event.button.button == SDL_BUTTON_LEFT)
        {
                if(!mouselook)
                {
                        SDL_SetRelativeMouseMode(SDL_TRUE);
                        mouselook = 1;
                }
                else
                {
                        tiles[target_z][target_y][target_x] = OPEN;
                }
        }
        else if(event.button.button == SDL_BUTTON_RIGHT)
        {
                tiles[place_z][place_y][place_x] = GRAS;
        }
}

//start a new game
void new_game()
{
        memset(player, 0, sizeof player);
        player[0].pos.x = STARTPX;
        player[0].pos.y = STARTPY;
        player[0].pos.z = STARTPZ;
        player[0].pos.w = PLYR_W;
        player[0].pos.h = PLYR_H;
        player[0].pos.d = PLYR_W;
        player[0].goingf = 0;
        player[0].goingb = 0;
        player[0].goingl = 0;
        player[0].goingr = 0;
        player[0].fvel = 0;
        player[0].rvel = 0;
        player[0].yaw = 3.1415926535 * 0.5;
        player[0].pitch = 0;
        player[0].grav = GRAV_ZERO;
        load_level();
}

void load_level()
{
        for(int x = 0; x < TILESW; x++) for(int y = 0; y < TILESH; y++) for(int z = 0; z < TILESD; z++)
        {
                float h =  3 + 3*sin(0.1 * x) + 3*cos(0.2 * z);
                float s = -14 + 12*sin(1 + 0.14 * x) + 13*cos(2 + 0.18 * z);
                if(y == TILESH-1 || y > TILESH - h || y > TILESH - s)
                        tiles[z][y][x] = GRAS;
                else
                        tiles[z][y][x] = OPEN;

                if(z == 4 && x + y < 10)
                        tiles[z][y][x] = GRAS;

                if(x == 7 && z + y > 10 && z < 14)
                        tiles[z][y][x] = GRAS;
        }
}

void update_player()
{
        struct player *p = player + 0;

        if(player[0].pos.y > TILESH*BS + 6000)
        {
                new_game();
                return;
        }

        if(p->goingf && !p->goingb) { p->fvel++; }
        else if(p->fvel > 0)        { p->fvel--; }

        if(p->goingb && !p->goingf) { p->fvel--; }
        else if(p->fvel < 0)        { p->fvel++; }

        if(p->goingr && !p->goingl) { p->rvel++; }
        else if(p->rvel > 0)        { p->rvel--; }

        if(p->goingl && !p->goingr) { p->rvel--; }
        else if(p->rvel < 0)        { p->rvel++; }

        //limit speed
        float totalvel = sqrt(p->fvel * p->fvel + p->rvel * p->rvel);
        if(totalvel > PLYR_SPD)
        {
                totalvel = PLYR_SPD / totalvel;
                p->fvel *= totalvel;
                p->rvel *= totalvel;
        }

        float fwdx = sin(p->yaw);
        float fwdz = cos(p->yaw);

        p->vel.x = fwdx * p->fvel + fwdz * p->rvel;
        p->vel.z = fwdz * p->fvel - fwdx * p->rvel;

        if(!move_player(p->vel.x, p->vel.y, p->vel.z))
        {
                p->fvel = 0;
                p->rvel = 0;
        }

        //gravity
        if(!p->ground || p->grav < GRAV_ZERO)
        {
                if(!move_player(0, gravity[p->grav], 0))
                        p->grav = GRAV_ZERO;
                else if(p->grav < GRAV_MAX)
                        p->grav++;
        }

        //detect ground
        struct box foot = (struct box){
                p->pos.x, p->pos.y + PLYR_H, p->pos.z,
                PLYR_W, 1, PLYR_W};
        p->ground = world_collide(foot);

        if(p->ground)
                p->grav = GRAV_ZERO;

}

//collide a box with nearby world tiles
int world_collide(struct box box)
{
        for(int i = -1; i < 2; i++) for(int j = -1; j < 3; j++) for(int k = -1; k < 2; k++)
        {
                int bx = box.x/BS + i;
                int by = box.y/BS + j;
                int bz = box.z/BS + k;

                if(block_collide(bx, by, bz, box))
                        return 1;
        }

        return 0;
}

//return 0 iff we couldn't actually move
int move_player(int velx, int vely, int velz)
{
        int last_was_x = 0;
        int last_was_z = 0;
        int already_stuck = 0;
        int moved = 0;

        if(!velx && !vely && !velz)
                return 1;

        if(world_collide(player[0].pos))
                already_stuck = 1;

        while(velx || vely || velz)
        {
                struct box testpos = player[0].pos;
                int amt;

                if((!velx && !velz) || ((last_was_x || last_was_z) && vely))
                {
                        amt = vely > 0 ? 1 : -1;
                        testpos.y += amt;
                        vely -= amt;
                        last_was_x = 0;
                        last_was_z = 0;
                }
                else if(!velz || (last_was_z && velx))
                {
                        amt = velx > 0 ? 1 : -1;
                        testpos.x += amt;
                        velx -= amt;
                        last_was_z = 0;
                        last_was_x = 1;
                }
                else
                {
                        amt = velz > 0 ? 1 : -1;
                        testpos.z += amt;
                        velz -= amt;
                        last_was_x = 0;
                        last_was_z = 1;
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
                        else if(last_was_z)
                                velz = 0;
                        else
                                vely = 0;
                        continue;
                }

                player[0].pos = testpos;
                moved = 1;
        }

        return moved;
}

int legit_tile(int x, int y, int z)
{
        return x >= 0 && x < TILESW
            && y >= 0 && y < TILESH
            && z >= 0 && z < TILESD;
}

//collide a rect with a rect
int collide(struct box l, struct box r)
{
        int xcollide = l.x + l.w >= r.x && l.x < r.x + r.w;
        int ycollide = l.y + l.h >= r.y && l.y < r.y + r.h;
        int zcollide = l.z + l.d >= r.z && l.z < r.z + r.d;
        return xcollide && ycollide && zcollide;
}

//collide a rect with a block
int block_collide(int bx, int by, int bz, struct box box)
{
        if(!legit_tile(bx, by, bz))
                return 0;

        if(tiles[bz][by][bx] <= LASTSOLID)
                return collide(box, (struct box){BS*bx, BS*by, BS*bz, BS, BS, BS});

        return 0;
}

void blacklines(float x, float y, float z)
{
        glBegin(GL_LINE_STRIP);
        glColor3f(0, 0, 0);
        glVertex3f(x*BS   ,y*BS   ,z*BS   ); //top
        glVertex3f(x*BS+BS,y*BS   ,z*BS   );
        glVertex3f(x*BS+BS,y*BS   ,z*BS+BS);
        glVertex3f(x*BS   ,y*BS   ,z*BS+BS);
        glVertex3f(x*BS   ,y*BS   ,z*BS   );

        glVertex3f(x*BS   ,y*BS+BS,z*BS   ); //bottom
        glVertex3f(x*BS+BS,y*BS+BS,z*BS   );
        glVertex3f(x*BS+BS,y*BS+BS,z*BS+BS);
        glVertex3f(x*BS   ,y*BS+BS,z*BS+BS);
        glVertex3f(x*BS   ,y*BS+BS,z*BS   );

        glVertex3f(x*BS   ,y*BS+BS,z*BS+BS); //missing sides
        glVertex3f(x*BS   ,y*BS   ,z*BS+BS);
        glVertex3f(x*BS+BS,y*BS   ,z*BS+BS);
        glVertex3f(x*BS+BS,y*BS+BS,z*BS+BS);
        glVertex3f(x*BS+BS,y*BS+BS,z*BS   );
        glVertex3f(x*BS+BS,y*BS   ,z*BS   );
        glEnd();
}

void grasstop(float x, float y, float z)
{
	glBindTexture(GL_TEXTURE_2D, 1);
        glColor3f(0.9, 0.9, 0.9); 

        glBegin(GL_TRIANGLE_FAN);
	glTexCoord2i(0, 0); glVertex3f(x*BS   ,y*BS   ,z*BS   );
        glTexCoord2i(1, 0); glVertex3f(x*BS+BS,y*BS   ,z*BS   );
        glTexCoord2i(1, 1); glVertex3f(x*BS+BS,y*BS   ,z*BS+BS);
        glTexCoord2i(0, 1); glVertex3f(x*BS   ,y*BS   ,z*BS+BS);
        glEnd();
}

void grasssouth(float x, float y, float z)
{
	glBindTexture(GL_TEXTURE_2D, 2);
        glColor3f(0.7, 0.7, 0.7); 

        glBegin(GL_TRIANGLE_FAN);
	glTexCoord2i(0, 1); glVertex3f(x*BS   ,y*BS+BS,z*BS   );
        glTexCoord2i(1, 1); glVertex3f(x*BS+BS,y*BS+BS,z*BS   );
        glTexCoord2i(1, 0); glVertex3f(x*BS+BS,y*BS   ,z*BS   );
        glTexCoord2i(0, 0); glVertex3f(x*BS   ,y*BS   ,z*BS   );
        glEnd();
}

void grassnorth(float x, float y, float z)
{
	glBindTexture(GL_TEXTURE_2D, 2);
        glColor3f(0.7, 0.7, 0.7); 

        glBegin(GL_TRIANGLE_FAN);
        glTexCoord2i(0, 0); glVertex3f(x*BS   ,y*BS   ,z*BS+BS);
        glTexCoord2i(1, 0); glVertex3f(x*BS+BS,y*BS   ,z*BS+BS);
        glTexCoord2i(1, 1); glVertex3f(x*BS+BS,y*BS+BS,z*BS+BS);
	glTexCoord2i(0, 1); glVertex3f(x*BS   ,y*BS+BS,z*BS+BS);
        glEnd();
}

void grasswest(float x, float y, float z)
{
	glBindTexture(GL_TEXTURE_2D, 2);
        glColor3f(0.5, 0.5, 0.5); 

        glBegin(GL_TRIANGLE_FAN);
        glTexCoord2i(0, 0); glVertex3f(x*BS   ,y*BS   ,z*BS   );
        glTexCoord2i(1, 0); glVertex3f(x*BS   ,y*BS   ,z*BS+BS);
        glTexCoord2i(1, 1); glVertex3f(x*BS   ,y*BS+BS,z*BS+BS);
	glTexCoord2i(0, 1); glVertex3f(x*BS   ,y*BS+BS,z*BS   );
        glEnd();
}

void grasseast(float x, float y, float z)
{
	glBindTexture(GL_TEXTURE_2D, 2);
        glColor3f(0.5, 0.5, 0.5); 

        glBegin(GL_TRIANGLE_FAN);
	glTexCoord2i(0, 1); glVertex3f(x*BS+BS,y*BS+BS,z*BS   );
        glTexCoord2i(1, 1); glVertex3f(x*BS+BS,y*BS+BS,z*BS+BS);
        glTexCoord2i(1, 0); glVertex3f(x*BS+BS,y*BS   ,z*BS+BS);
        glTexCoord2i(0, 0); glVertex3f(x*BS+BS,y*BS   ,z*BS   );
        glEnd();
}

void grassbottom(float x, float y, float z)
{
	glBindTexture(GL_TEXTURE_2D, 3);
        glColor3f(0.3, 0.3, 0.3); 

        glBegin(GL_TRIANGLE_FAN);
        glTexCoord2i(0, 1); glVertex3f(x*BS   ,y*BS+BS,z*BS+BS);
        glTexCoord2i(1, 1); glVertex3f(x*BS+BS,y*BS+BS,z*BS+BS);
        glTexCoord2i(1, 0); glVertex3f(x*BS+BS,y*BS+BS,z*BS   );
	glTexCoord2i(0, 0); glVertex3f(x*BS   ,y*BS+BS,z*BS   );
        glEnd();
}

//select block from eye following vector f
void rayshot(float eye0, float eye1, float eye2, float f0, float f1, float f2)
{
        int x = (int)(eye0 / BS);
        int y = (int)(eye1 / BS);
        int z = (int)(eye2 / BS);

        for(int i = 0; ; i++)
        {
                float a0 = (BS * (x + (f0 > 0 ? 1 : 0)) - eye0) / f0;
                float a1 = (BS * (y + (f1 > 0 ? 1 : 0)) - eye1) / f1;
                float a2 = (BS * (z + (f2 > 0 ? 1 : 0)) - eye2) / f2;
                float amt = 0;

                place_x = x;
                place_y = y;
                place_z = z;

                if(a0 < a1 && a0 < a2) { x += (f0 > 0 ? 1 : -1); amt = a0; }
                else if(a1 < a2)       { y += (f1 > 0 ? 1 : -1); amt = a1; }
                else                   { z += (f2 > 0 ? 1 : -1); amt = a2; }

                eye0 += amt * f0 * 1.0001;
                eye1 += amt * f1 * 1.0001;
                eye2 += amt * f2 * 1.0001;

                if(x < 0 || y < 0 || z < 0 || x >= TILESW || y >= TILESH || z >= TILESD)
                        goto bad;

                if(tiles[z][y][x] != OPEN)
                        break;

                if(i == 6)
                        goto bad;
        }

        target_x = x;
        target_y = y;
        target_z = z;

        return;

        bad:
        target_x = target_y = target_z = 0;
        place_x = place_y = place_z = 0;
}

//draw everything in the game on the screen
void draw_stuff()
{
        glViewport(0, 0, screenw, screenh);
        glClearColor(0.3, 0.9, 1.0, 1.0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        float frustw = 9.0 * screenw / screenh;
        glFrustum(-frustw, frustw, -9, 9, 16, 9999);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        float eye0, eye1, eye2;
        eye0 = player[0].pos.x + PLYR_W / 2;
        eye1 = player[0].pos.y - BS * 3 / 4;
        eye2 = player[0].pos.z + PLYR_W / 2;
        float f0, f1, f2;
        f0 = cos(player[0].pitch) * sin(player[0].yaw);
        f1 = sin(player[0].pitch);
        f2 = cos(player[0].pitch) * cos(player[0].yaw);
        float wing0, wing1, wing2;
        wing0 = -cos(player[0].yaw);
        wing1 = 0;
        wing2 = sin(player[0].yaw);
        float up0, up1, up2;
        up0 = f1*wing2 - f2*wing1;
        up1 = f2*wing0 - f0*wing2;
        up2 = f0*wing1 - f1*wing0;
        float upm = sqrt(up0*up0 + up1*up1 + up2*up2);
        up0 /= upm;
        up1 /= upm;
        up2 /= upm;
        float s0, s1, s2;
        s0 = f1*up2 - f2*up1;
        s1 = f2*up0 - f0*up2;
        s2 = f0*up1 - f1*up0;
        float sm = sqrt(s0*s0 + s1*s1 + s2*s2);
        float z0, z1, z2;
        z0 = s0/sm;
        z1 = s1/sm;
        z2 = s2/sm;
        float u0, u1, u2;
        u0 = z1*f2 - z2*f1;
        u1 = z2*f0 - z0*f2;
        u2 = z0*f1 - z1*f0;

        float M[] = {
                s0, u0,-f0, 0,
                s1, u1,-f1, 0,
                s2, u2,-f2, 0,
                 0,  0,  0, 1
        };

        rayshot(eye0, eye1, eye2, f0, f1, f2);

        glMultMatrixf(M);
        glTranslated(-eye0, -eye1, -eye2);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_TEXTURE_2D);
        glEnable(GL_CULL_FACE);
        glDepthFunc(GL_LEQUAL);

        // draw world
        for(int x = 0; x < TILESW; x++) for(int y = 0; y < TILESH; y++) for(int z = 0; z < TILESD; z++)
        {
                if(tiles[z][y][x] == GRAS && (y == 0 || tiles[z][y-1][x] == OPEN))
                        grasstop(x, y, z);
        }
        for(int x = 0; x < TILESW; x++) for(int y = 0; y < TILESH; y++) for(int z = 0; z < TILESD; z++)
        {
                if(tiles[z][y][x] == GRAS && (z == 0 || tiles[z-1][y][x] == OPEN))
                        grasssouth(x, y, z);
                if(tiles[z][y][x] == GRAS && (z == TILESD-1 || tiles[z+1][y][x] == OPEN))
                        grassnorth(x, y, z);
                if(tiles[z][y][x] == GRAS && (x == 0 || tiles[z][y][x-1] == OPEN))
                        grasswest(x, y, z);
                if(tiles[z][y][x] == GRAS && (x == TILESW-1 || tiles[z][y][x+1] == OPEN))
                        grasseast(x, y, z);
        }
        for(int x = 0; x < TILESW; x++) for(int y = 0; y < TILESH; y++) for(int z = 0; z < TILESD; z++)
        {
                if(tiles[z][y][x] == GRAS && (y == TILESH-1 || tiles[z][y+1][x] == OPEN))
                        grassbottom(x, y, z);
        }

        glDisable(GL_TEXTURE_2D);
        glDisable(GL_CULL_FACE);
        blacklines(target_x, target_y, target_z);

        SDL_GL_SwapWindow(win);
}

void debrief()
{
        printf("%f %f\n", player[0].pos.x, player[0].pos.z);
}

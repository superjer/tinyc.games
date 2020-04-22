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

#define SDL_DISABLE_IMMINTRIN_H
#include <SDL.h>
#define STBI_NO_SIMD
#define STB_IMAGE_IMPLEMENTATION
#include "../_stb/stb_image.h"

#include "./timer.c"
#include "./shader.c"
#include "./perlin.c"

#define SCALE 3                    // x magnification
#define W 1920                     // window width, height
#define H 1080                     // ^
#define CHUNKW 16                  // chunk size (vao size)
#define CHUNKD 16                  // ^
#define VAOW 64                    // how many VAOs wide
#define VAOD 64                    // how many VAOs deep
#define VAOS (VAOW*VAOD)           // total nr of vbos
#define TILESW (CHUNKW*VAOW)       // total level width, height
#define TILESH 128                 // ^
#define TILESD (CHUNKD*VAOD)       // ^
#define BS (20*SCALE)              // block size
#define BS2 (BS/2)                 // block size in half
#define PLYR_W (14*SCALE)          // physical width and height of the player
#define PLYR_H (36*SCALE)          // ^
#define PLYR_SPD (2*SCALE)         // units per frame
#define PLYR_SPD_R (4*SCALE)       // units per frame
#define PLYR_SPD_S (1*SCALE)       // units per frame
#define EYEDOWN 10                 // how far down are the eyes from the top of the head
#define STARTPX (TILESW*BS2)       // starting position within start screen
#define STARTPY 0                  // ^
#define STARTPZ (TILESD*BS2)       // ^
#define NR_PLAYERS 1
#define JUMP_BUFFER_FRAMES 6
#define GRAV_JUMP 0
#define GRAV_FLOAT 4
#define GRAV_ZERO 14
#define GRAV_MAX 49

#define UP    1
#define EAST  2
#define NORTH 3
#define WEST  4
#define SOUTH 5
#define DOWN  6

#define STON 39
#define ORE  40
#define SAND 41
#define DIRT 42

#define GRG1 45
#define GRG2 46
#define GRAS 47

#define BARR 48

#define LASTSOLID BARR // everything less than or eq here is solid
#define OPEN 75        // invisible open, walkable space
#define WATR 76

#define VERTEX_BUFLEN 100000
#define SUNQLEN 1000

#define CLAMP(v, l, u) { if (v < l) v = l; else if (v > u) v = u; }

struct vbufv { // vertex buffer vertex
        float tex;
        float orient;
        float x, y, z;
        float illum0, illum1, illum2, illum3;
        float alpha;
};

int gravity[] = { -20, -17, -14, -12, -10, -8, -6, -5, -4, -3,
                   -2,  -2,  -1,  -1,   0,  1,  1,  2,  2,  3,
                    3,   4,   4,   5,   5,  6,  6,  7,  7,  8,
                    9,  10,  10,  11,  12, 12, 13, 14, 15, 16,
                   17,  18,  19,  20,  21, 22, 23, 24, 25, 26};

unsigned char tiles[TILESD][TILESH][TILESW];
unsigned char sunlight[TILESD+1][TILESH+1][TILESW+1];
unsigned char gndheight[TILESD][TILESW];
float cornlight[TILESD+2][TILESH+2][TILESW+2];
char already_generated[VAOW][VAOD];

struct box { float x, y, z, w, h ,d; };
struct point { float x, y, z; };

struct qitem { int x, y, z; };
struct qitem sunq0_[SUNQLEN+1];
struct qitem sunq1_[SUNQLEN+1];
struct qitem *sunq_curr = sunq0_;
struct qitem *sunq_next = sunq1_;
size_t sq_curr_len;
size_t sq_next_len;

struct player {
        struct box pos;
        struct point vel;
        float yaw;
        float pitch;
        int wet;
        int goingf;
        int goingb;
        int goingl;
        int goingr;
        int jumping;
        int sneaking;
        int running;
        int breaking;
        int building;
        int cooldown;
        int fvel;
        int rvel;
        int grav;
        int ground;
} player[NR_PLAYERS];

struct player camplayer;

int frame = 0;
int noisy = 1;
int polys = 0;

int mouselook = 1;
int target_x, target_y, target_z;
int place_x, place_y, place_z;
int screenw = W;
int screenh = H;
int zooming = 0;
float zoom_amt = 1.f;
float fast = 1.f;

SDL_Event event;
SDL_Window *win;
SDL_GLContext ctx;

unsigned int vbo[VAOS], vao[VAOS];
size_t vbo_len[VAOS];
struct vbufv vbuf[VERTEX_BUFLEN + 1000]; // vertex buffer + padding
struct vbufv *v_limit = vbuf + VERTEX_BUFLEN;
struct vbufv *v = vbuf;

//prototypes
void setup();
void resize();
void new_game();
void create_hmap();
void gen_chunk();
void sun_enqueue(int x, int y, int z, int base, unsigned char incoming_light);
void recalc_gndheight(int x, int z);
void step_sunlight();
void recalc_corner_lighting(int xlo, int xhi, int zlo, int zhi);
void key_move(int down);
void mouse_move();
void mouse_button(int down);
void update_world();
void update_player(struct player * p, int real, float amt);
int move_player(struct player * p, int velx, int vely, int velz);
int collide(struct box plyr, struct box block);
int block_collide(int bx, int by, int bz, struct box plyr, int wet);
int world_collide(struct box plyr, int wet);
void draw_stuff();
void debrief();

void GLAPIENTRY
MessageCallback(GLenum source, GLenum type, GLuint id, GLenum severity,
                GLsizei length, const GLchar* message, const void* userParam)
{
        if (type != GL_DEBUG_TYPE_ERROR) return; // too much yelling
        fprintf( stderr, "GL CALLBACK: %s type = 0x%x, severity = 0x%x, message = %s\n",
                        ( type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : "" ),
                        type, severity, message );
}

//the entry point and main game loop
int main()
{
        setup();
        new_game();

        for (;;)
        {
                while (SDL_PollEvent(&event)) switch (event.type)
                {
                        case SDL_QUIT:            exit(0);
                        case SDL_KEYDOWN:         key_move(1);       break;
                        case SDL_KEYUP:           key_move(0);       break;
                        case SDL_MOUSEMOTION:     mouse_move();      break;
                        case SDL_MOUSEBUTTONDOWN: mouse_button(1);   break;
                        case SDL_MOUSEBUTTONUP:   mouse_button(0);   break;
                        case SDL_WINDOWEVENT:
                                switch (event.window.event)
                                {
                                        case SDL_WINDOWEVENT_SIZE_CHANGED:
                                                resize();
                                                break;
                                }
                                break;
                }

                float interval = 1000.f / 60.f;
                static float accumulated_elapsed = 0.f;
                static int last_ticks = 0;
                int ticks = SDL_GetTicks();
                accumulated_elapsed += ticks - last_ticks;
                last_ticks = ticks;
                CLAMP(accumulated_elapsed, 0, interval * 3 - 1);
                while (accumulated_elapsed >= interval)
                {
                        TIMECALL(update_player, (&player[0], 1, 1.f));
                        TIMECALL(update_world, ());
                        accumulated_elapsed -= interval;
                }

                camplayer = player[0];
                float amt = accumulated_elapsed / interval;
                printf("amt %f\n", amt);

                TIMECALL(update_player, (&camplayer, 0, amt));
                TIMECALL(step_sunlight, ());
                draw_stuff();
                debrief();
                frame++;
        }
}

//initial setup to get the window and rendering going
void setup()
{
        srand(time(NULL));
        init_perlin();

        SDL_Init(SDL_INIT_VIDEO);
        win = SDL_CreateWindow("Blocko", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                W, H, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL);
        if (!win) exit(fprintf(stderr, "%s\n", SDL_GetError()));
        ctx = SDL_GL_CreateContext(win);
        if (!ctx) exit(fprintf(stderr, "Could not create GL context\n"));
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        SDL_GL_SetSwapInterval(0);
        #ifndef __APPLE__
        glewExperimental = GL_TRUE;
        glewInit();
        #endif

        // enable debug output
        glEnable              ( GL_DEBUG_OUTPUT );
        glDebugMessageCallback( MessageCallback, 0 );

        // load all the textures
        glActiveTexture(GL_TEXTURE0);
        int x, y, n, mode;
        GLuint texid = 0;
        glGenTextures(1, &texid);
        glBindTexture(GL_TEXTURE_2D_ARRAY, texid);

        unsigned char *texels;
        char *files[] = {
                "res/grass_top.png",
                "res/grass_side.png",
                "res/dirt.png",
                "res/grass_grow1_top.png",
                "res/grass_grow2_top.png",
                "res/stone.png",
                "res/sand.png",
                "res/water.png",
                "res/ore.png",
                ""
        };
        for (int f = 0; files[f][0]; f++)
        {
                texels = stbi_load(files[f], &x, &y, &n, 0);
                mode = n == 4 ? GL_RGBA : GL_RGB;
                if (f == 0)
                        glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_RGBA8, x, y, 256);
                glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, f, x, y, 1, mode, GL_UNSIGNED_BYTE, texels);
                stbi_image_free(texels);
        }

        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_LEVEL, 1);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);

        load_shaders();

        glGenVertexArrays(VAOS, vao);
        glGenBuffers(VAOS, vbo);
        for (int i = 0; i < VAOS; i++)
        {
                glBindVertexArray(vao[i]);
                glBindBuffer(GL_ARRAY_BUFFER, vbo[i]);
                // tex number
                glVertexAttribPointer(0, 1, GL_FLOAT, GL_FALSE, sizeof (struct vbufv), (void*)&((struct vbufv *)NULL)->tex);
                glEnableVertexAttribArray(0);
                // orientation
                glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, sizeof (struct vbufv), (void*)&((struct vbufv *)NULL)->orient);
                glEnableVertexAttribArray(1);
                // position
                glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof (struct vbufv), (void*)&((struct vbufv *)NULL)->x);
                glEnableVertexAttribArray(2);
                // illum
                glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof (struct vbufv), (void*)&((struct vbufv *)NULL)->illum0);
                glEnableVertexAttribArray(3);
                // alpha
                glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, sizeof (struct vbufv), (void*)&((struct vbufv *)NULL)->alpha);
                glEnableVertexAttribArray(4);
        }

        glUseProgram(prog_id);
        glUniform1i(glGetUniformLocation(prog_id, "tarray"), 0);
        SDL_SetRelativeMouseMode(SDL_TRUE);
}

void resize()
{
        screenw = event.window.data1;
        screenh = event.window.data2;
}

void jump(int down)
{
        if (player[0].wet)
                player[0].jumping = down;
        else if (down)
                player[0].jumping = JUMP_BUFFER_FRAMES;
}

void key_move(int down)
{
        if (event.key.repeat) return;

        switch (event.key.keysym.sym)
        {
                // continuous movement stuff
                case SDLK_w:      player[0].goingf   = down; break;
                case SDLK_s:      player[0].goingb   = down; break;
                case SDLK_a:      player[0].goingl   = down; break;
                case SDLK_d:      player[0].goingr   = down; break;
                case SDLK_LSHIFT: player[0].sneaking = down; break;
                case SDLK_LCTRL:  player[0].running  = down; break;
                case SDLK_z:      zooming            = down; break;

                // instantaneous movement
                case SDLK_SPACE:
                        jump(down);
                        break;

                // menu stuff
                case SDLK_ESCAPE:
                        SDL_SetRelativeMouseMode(SDL_FALSE);
                        mouselook = 0;
                        break;

                // debug stuff
                case SDLK_q: // go up alot
                        if (!down)
                        {
                                player[0].pos.y -= 1000;
                                player[0].grav = GRAV_ZERO;
                        }
                        break;
                case SDLK_f: // go fast
                        if (down)
                                fast = (fast == 1.f) ? 8.f : 1.f;
                        break;
                case SDLK_F3: // show FPS and timings etc.
                        if (!down) noisy = !noisy;
                        break;
        }
}

void mouse_move()
{
        if (!mouselook) return;

        float pitchlimit = 3.1415926535 * 0.5 - 0.001;
        player[0].yaw += event.motion.xrel * 0.001;
        player[0].pitch += event.motion.yrel * 0.001;

        if (player[0].pitch > pitchlimit)
                player[0].pitch = pitchlimit;

        if (player[0].pitch < -pitchlimit)
                player[0].pitch = -pitchlimit;
}

void mouse_button(int down)
{
        if (!mouselook)
        {
                if (down)
                {
                        SDL_SetRelativeMouseMode(SDL_TRUE);
                        mouselook = 1;
                }
        }
        else if (event.button.button == SDL_BUTTON_LEFT)
        {
                player[0].breaking = down;
        }
        else if (event.button.button == SDL_BUTTON_RIGHT)
        {
                player[0].building = down;
        }
        else if (event.button.button == SDL_BUTTON_X1)
        {
                jump(down);
        }
}

void move_to_ground(float *inout, int x, int y, int z)
{
        *inout = gndheight[z][x] * BS - PLYR_H - 1;
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
        player[0].yaw = 3.1415926535 * 0.23;
        player[0].grav = GRAV_ZERO;
        TIMECALL(create_hmap, ());
        recalc_gndheight(STARTPX/BS, STARTPZ/BS);
        move_to_ground(&player[0].pos.y, STARTPX/BS, STARTPY/BS, STARTPZ/BS);
}

int hmap[TILESW][TILESD];
int hmap2[TILESW][TILESD];

void gen_hmap(int x0, int x2, int z0, int z2)
{
        // pick corners if they aren't set
        if (hmap[x0][z0] == 0) hmap[x0][z0] = 64 + rand() % 64;
        if (hmap[x0][z2] == 0) hmap[x0][z2] = 64 + rand() % 64;
        if (hmap[x2][z0] == 0) hmap[x2][z0] = 64 + rand() % 64;
        if (hmap[x2][z2] == 0) hmap[x2][z2] = 64 + rand() % 64;

        int x1 = (x0 + x2) / 2;
        int z1 = (z0 + z2) / 2;
        int w = (x2 - x0) / 4;
        int d = (z2 - z0) / 4;
        w = w ? w : 1;
        d = d ? d : 1;

        int r = w > 2 ? 1 : 0;

        // edges middles
        if (!hmap[x0][z1])
                hmap[x0][z1] = (hmap[x0][z0] + hmap[x0][z2]) / 2 + r * ((rand() % d) - d / 2);
        if (!hmap[x2][z1])
                hmap[x2][z1] = (hmap[x2][z0] + hmap[x2][z2]) / 2 + r * ((rand() % d) - d / 2);
        if (!hmap[x1][z0])
                hmap[x1][z0] = (hmap[x0][z0] + hmap[x2][z0]) / 2 + r * ((rand() % d) - d / 2);
        if (!hmap[x1][z2])
                hmap[x1][z2] = (hmap[x0][z2] + hmap[x2][z2]) / 2 + r * ((rand() % d) - d / 2);

        // middle middle
        hmap[x1][z1] = (hmap[x0][z1] + hmap[x2][z1] + hmap[x1][z0] + hmap[x1][z2]) / 4 + r * ((rand() % (d * 2)) - d);

        // recurse if there are any unfilled spots
        if(x1 - x0 > 1 || x2 - x1 > 1 || z1 - z0 > 1 || z2 - z1 > 1)
        {
                gen_hmap(x0, x1, z0, z1);
                gen_hmap(x0, x1, z1, z2);
                gen_hmap(x1, x2, z0, z1);
                gen_hmap(x1, x2, z1, z2);
        }
}

void smooth_hmap()
{
        for (int x = 0; x < TILESW; x++) for (int z = 0; z < TILESD; z++)
        {
                float p365 = improved_perlin_noise(x, 0, -z, 365);
                int radius = p365 < 0.0f ? 3 :
                             p365 < 0.2f ? 2 : 1;
                int x0 = x - radius;
                int x1 = x + radius + 1;
                int z0 = z - radius;
                int z1 = z + radius + 1;
                CLAMP(x0, 0, TILESW-1);
                CLAMP(x1, 0, TILESW-1);
                CLAMP(z0, 0, TILESD-1);
                CLAMP(z1, 0, TILESD-1);
                int sum = 0, n = 0;
                for (int i = x0; i < x1; i++) for (int j = z0; j < z1; j++)
                {
                        sum += hmap[i][j];
                        n++;
                }
                int res = sum / n;

                float p800 = improved_perlin_noise(x, 0, z, 800);
                float p777 = improved_perlin_noise(z, 0, x, 777);
                float p301 = improved_perlin_noise(x, 0, z, 301);
                float p204 = improved_perlin_noise(x, 0, z, 204);
                float p33 = improved_perlin_noise(x, 0, z, 32 * (1.1 + p301));
                float swoosh = p33 > 0.3 ? (10 - 30 * (p33 - 0.3)) : 0;

                float times = (p204 * 20.f) + 30.f;
                float plus = (-p204 * 40.f) + 60.f;
                CLAMP(times, 20.f, 40.f);
                CLAMP(plus, 40.f, 80.f);
                int beach_ht = (1.f - p777) * times + plus;
                CLAMP(beach_ht, 90, 100);

                if (res > beach_ht) // beaches
                {
                        if (res > beach_ht + 21) res -= 18;
                        else res = ((res - beach_ht) / 7) + beach_ht;
                }

                float s = (1 + p204) * 0.2;
                if (p800 > 0.0 + s)
                {
                        float t = (p800 - 0.0 - s) * 10;
                        CLAMP(t, 0.f, 1.f);
                        res = lerp(t, res, 102);
                        if (res == 102 && swoosh) res = 101;
                }

                hmap2[x][z] = res < TILESH - 1 ? res : TILESH - 1;
        }
}

void create_hmap()
{
        // generate in pieces
        for (int i = 0; i < 8; i++) for (int j = 0; j < 8; j++)
        {
                int x0 = (i  ) * TILESW / 8;
                int x1 = (i+1) * TILESW / 8;
                int z0 = (j  ) * TILESD / 8;
                int z1 = (j+1) * TILESD / 8;
                CLAMP(x1, 0, TILESW-1);
                CLAMP(z1, 0, TILESD-1);
                gen_hmap(x0, x1, z0 , z1);
        }

        smooth_hmap();
}

void gen_chunk(int xlo, int xhi, int zlo, int zhi)
{
        CLAMP(xlo, 0, TILESW);
        CLAMP(xhi, 0, TILESW);
        CLAMP(zlo, 0, TILESD);
        CLAMP(zhi, 0, TILESD);

        static char column_already_generated[TILESW][TILESD];

        for (int x = xlo; x < xhi; x++) for (int z = zlo; z < zhi; z++)
        {
                if (column_already_generated[x][z])
                        continue;
                column_already_generated[x][z] = 1;

                float p530 = improved_perlin_noise(z, 0, x, 530);
                float p630 = improved_perlin_noise(-z, 0, x, 629);
                float p200 = improved_perlin_noise(x, 0, z, 200);
                float p80 = improved_perlin_noise(x, 0, z, 80);

                if (p200 > 0.2f)
                {
                        float flatten = (p200 - 0.2f) * 80;
                        CLAMP(flatten, 1, 12);
                        hmap2[x][z] -= 100;
                        hmap2[x][z] /= flatten;
                        hmap2[x][z] += 100;
                }

                int depth = 0;
                int slicey_bit = 0;
                for (int y = 0; y < TILESH; y++)
                {
                        if (y == TILESH - 1) { tiles[z][y][x] = STON; continue; }

                        float p300 = improved_perlin_noise(x, y, z, 300);
                        float p32 = improved_perlin_noise(x, y, z, 16 + 16 * (1.1 + p300));
                        float lumpy = p32 > 0.3 ? (10 - 30 * (p32 * p32 * p32 - 0.3)) : 0;
                        float ore = 0;

                        float p90 = improved_perlin_noise(x, y, z, 90);
                        float p91 = improved_perlin_noise(x+1000, y+1000, z+1000, 91);
                        float p42 = improved_perlin_noise(x, y*(p300 + 1), z, 42);
                        float p9  = improved_perlin_noise(x, y*0.05, z, 9);

                        if (p300 < -0.5) { lumpy = -lumpy; }
                        else if (p300 < 0.5) { lumpy = 0; }

                        int cave = (p90 < -0.24 || p91 < -0.24) && (p42 > 0.5 && p9 < 0.4);

                        if (y > hmap2[x][z] - ((p80 + 1) * 20) && p90 > 0.4 && p91 > 0.4 && p42 > 0.01 && p42 < 0.09 && p300 > 0.3)
                                slicey_bit = 1;
                        if (cave || y < hmap2[x][z] + lumpy)
                        {
                                if (!slicey_bit || rand() % 20 == 0)
                                {
                                        int watr = hmap2[x][z] > 99 ? WATR : OPEN; //only allow water below low heightmap
                                        tiles[z][y][x] = y > 100 ? watr : OPEN;
                                        sunlight[z][y][x] = 14;
                                        depth = 0;
                                        slicey_bit = 0;
                                        continue;
                                }
                        }
                        else
                                slicey_bit = 0;

                        depth++;
                        float p16 = improved_perlin_noise(x, y, z, 16);
                        int slv = 76 + p530 * 20;
                        int dlv = 86 + p630 * 20;

                        if      (slicey_bit)        tiles[z][y][x] = SAND;
                        else if (ore)               tiles[z][y][x] = ORE;
                        else if (depth > 5 + 5 * p16) tiles[z][y][x] = STON;
                        else if (y < slv - 5 * p16) tiles[z][y][x] = STON;
                        else if (y < dlv - 5 * p16) tiles[z][y][x] = p80 > (-depth * 0.1f) ? DIRT : OPEN; // erosion
                        else if (y < 100 - 5 * p16) tiles[z][y][x] = depth == 1 ? GRAS : DIRT;
                        else                        tiles[z][y][x] = SAND;

                        /* cave test
                        tiles[z][y][x] = cave ? GRAS : OPEN;
                        */
                }
        }
}

void sun_enqueue(int x, int y, int z, int base, unsigned char incoming_light)
{
        if (sunlight[z][y][x] >= incoming_light)
                return; // already brighter

        if (tiles[z][y][x] != OPEN)
                return; // no lighting for solid blocks (FIXME: hmmmm?)

        sunlight[z][y][x] = incoming_light;

        if (sq_next_len >= SUNQLEN)
        {
                //printf("out of room in sun queue\n");
                return;
        }

        for (size_t i = base; i < sq_curr_len; i++)
                if (sunq_curr[i].x == x && sunq_curr[i].y == y && sunq_curr[i].z == z)
                        return; // already queued in current queue

        for (size_t i = 0; i < sq_next_len; i++)
                if (sunq_next[i].x == x && sunq_next[i].y == y && sunq_next[i].z == z)
                        return; // already queued in next queue

        sunq_next[sq_next_len].x = x;
        sunq_next[sq_next_len].y = y;
        sunq_next[sq_next_len].z = z;
        sq_next_len++;
}

void recalc_gndheight(int x, int z)
{
        int y;
        for (y = 0; y < TILESH; y++)
        {
                if (tiles[z][y][x] != OPEN)
                {
                        gndheight[z][x] = y;
                        if (y)
                        {
                                sunlight[z][y-1][x] = 0; // prevent short out:
                                sun_enqueue(x, y-1, z, SUNQLEN, 15);
                        }
                        break;
                }
                sunlight[z][y][x] = 15; // light pure sky
        }

        // continue darkening
        for (; y < TILESH; y++)
        {
                sunlight[z][y][x] = 0;
        }
}

void step_sunlight()
{
        // swap the queues
        sunq_curr = sunq_next;
        sq_curr_len = sq_next_len;
        sq_next_len = 0;
        sunq_next = (sunq_curr == sunq0_) ? sunq1_ : sunq0_;

        for (size_t i = 0; i < sq_curr_len; i++)
        {
                int x = sunq_curr[i].x;
                int y = sunq_curr[i].y;
                int z = sunq_curr[i].z;
                char pass_on = sunlight[z][y][x];
                if (pass_on) pass_on--; else continue;
                if (x           ) sun_enqueue(x-1, y  , z  , i+1, pass_on);
                if (y           ) sun_enqueue(x  , y-1, z  , i+1, pass_on);
                if (z           ) sun_enqueue(x  , y  , z-1, i+1, pass_on);
                if (x < TILESW-1) sun_enqueue(x+1, y  , z  , i+1, pass_on);
                if (y < TILESH-1) sun_enqueue(x  , y+1, z  , i+1, pass_on);
                if (z < TILESD-1) sun_enqueue(x  , y  , z+1, i+1, pass_on);
        }

}

void recalc_corner_lighting(int xlo, int xhi, int zlo, int zhi)
{
        for (int z = zlo; z <= zhi; z++) for (int y = 0; y <= TILESH; y++) for (int x = xlo; x <= xhi; x++)
        {
                int x_ = (x == 0) ? 0 : x - 1;
                int y_ = (y == 0) ? 0 : y - 1;
                int z_ = (z == 0) ? 0 : z - 1;
                cornlight[z][y][x] = 0.008f * (
                                sunlight[z_][y_][x_] + sunlight[z_][y_][x ] + sunlight[z_][y ][x_] + sunlight[z_][y ][x ] +
                                sunlight[z ][y_][x_] + sunlight[z ][y_][x ] + sunlight[z ][y ][x_] + sunlight[z ][y ][x ]);
        }
}

void update_world()
{
        int i, x, y, z;
        for (i = 0; i < 500; i++) {
                x = 1 + rand() % (TILESW - 2);
                z = 1 + rand() % (TILESD - 2);

                for (y = 1; y < TILESH - 1; y++) {
                        if (0) ;
                        else if (tiles[z][y][x] == GRG1) tiles[z][y][x] = GRG2;
                        else if (tiles[z][y][x] == GRG2) tiles[z][y][x] = GRAS;
                        else if (tiles[z][y][x] == DIRT) {
                                if (tiles[z  ][y-1][x  ] == OPEN && (
                                    (tiles[z+1][y  ][x  ] | 1) == GRAS ||
                                    (tiles[z-1][y  ][x  ] | 1) == GRAS ||
                                    (tiles[z  ][y  ][x+1] | 1) == GRAS ||
                                    (tiles[z  ][y  ][x-1] | 1) == GRAS ||
                                    (tiles[z+1][y+1][x  ] | 1) == GRAS ||
                                    (tiles[z-1][y+1][x  ] | 1) == GRAS ||
                                    (tiles[z  ][y+1][x+1] | 1) == GRAS ||
                                    (tiles[z  ][y+1][x-1] | 1) == GRAS ||
                                    (tiles[z+1][y-1][x  ] | 1) == GRAS ||
                                    (tiles[z-1][y-1][x  ] | 1) == GRAS ||
                                    (tiles[z  ][y-1][x+1] | 1) == GRAS ||
                                    (tiles[z  ][y-1][x-1] | 1) == GRAS) ) {
                                        tiles[z][y][x] = GRG1;
                                }
                                break;
                        }
                }

                if (rand() % 10 == 0) recalc_gndheight(x, z);
        }
}

void personal_light(int x, int y, int z)
{
        sunlight[z][y][x] = 0;
}

void update_player(struct player *p, int real, float amt)
{
        if (real && p->pos.y > TILESH*BS + 6000) // fell too far
        {
                new_game();
                return;
        }

        if (p->jumping && p->wet)
        {
                p->grav = GRAV_JUMP;
        }
        else if (p->jumping)
        {
                p->jumping--; // reduce buffer frames
                if (p->ground)
                {
                        p->grav = GRAV_JUMP;
                        p->jumping = 0;
                }
        }

        if (p->cooldown) p->cooldown--;

        if (real && p->breaking && !p->cooldown && target_x >= 0)
        {
                int x = target_x;
                int y = target_y;
                int z = target_z;
                tiles[z][y][x] = OPEN;
                recalc_gndheight(x, z);
                unsigned char max = 0;
                if (sunlight[z-1][y  ][x  ] > max) max = sunlight[z-1][y  ][x  ];
                if (sunlight[z+1][y  ][x  ] > max) max = sunlight[z+1][y  ][x  ];
                if (sunlight[z  ][y-1][x  ] > max) max = sunlight[z  ][y-1][x  ];
                if (sunlight[z  ][y+1][x  ] > max) max = sunlight[z  ][y+1][x  ];
                if (sunlight[z  ][y  ][x-1] > max) max = sunlight[z  ][y  ][x-1];
                if (sunlight[z  ][y  ][x+1] > max) max = sunlight[z  ][y  ][x+1];
                sun_enqueue(place_x, place_y, place_z, 0, max ? max - 1 : 0);
                p->cooldown = 5;
        }

        if (real && p->building && !p->cooldown && place_x >= 0) {
                if (!collide(p->pos, (struct box){ place_x * BS, place_y * BS, place_z * BS, BS, BS, BS }))
                        tiles[place_z][place_y][place_x] = DIRT;
                p->cooldown = 10;
        }

        if (p->goingf && !p->goingb) { p->fvel++; }
        else if (p->fvel > 0)        { p->fvel--; }

        if (p->goingb && !p->goingf) { p->fvel--; }
        else if (p->fvel < 0)        { p->fvel++; }

        if (p->goingr && !p->goingl) { p->rvel++; }
        else if (p->rvel > 0)        { p->rvel--; }

        if (p->goingl && !p->goingr) { p->rvel--; }
        else if (p->rvel < 0)        { p->rvel++; }

        //limit speed
        float totalvel = sqrt(p->fvel * p->fvel + p->rvel * p->rvel);
        float limit = p->running  ? PLYR_SPD_R :
                      p->sneaking ? PLYR_SPD_S :
                                    PLYR_SPD;
        limit *= fast;
        if (totalvel > limit)
        {
                limit /= totalvel;
                if (p->fvel > 4 || p->fvel < -4) p->fvel *= limit;
                if (p->rvel > 4 || p->rvel < -4) p->rvel *= limit;
        }

        float fwdx = sin(p->yaw);
        float fwdz = cos(p->yaw);

        p->vel.x = fwdx * p->fvel + fwdz * p->rvel;
        p->vel.z = fwdz * p->fvel - fwdx * p->rvel;

        if (!move_player(p, round(p->vel.x * amt), round(p->vel.y * amt), round(p->vel.z * amt)))
        {
                p->fvel = 0;
                p->rvel = 0;
        }

        //detect water
        int was_wet = p->wet;
        p->wet = world_collide(p->pos, 1);
        if (was_wet && !p->wet && p->grav < GRAV_FLOAT)
                p->grav = GRAV_FLOAT;

        //gravity
        if (!p->ground || p->grav < GRAV_ZERO)
        {
                float fall_dist = gravity[p->grav] / (p->wet ? 3 : 1);
                if (!move_player(p, 0, fall_dist * amt, 0))
                        p->grav = GRAV_ZERO;
                else if (p->grav < GRAV_MAX)
                        p->grav++;
        }

        //detect ground
        struct box foot = (struct box){
                p->pos.x, p->pos.y + PLYR_H, p->pos.z,
                PLYR_W, 1, PLYR_W};
        p->ground = world_collide(foot, 0);

        if (p->ground)
                p->grav = GRAV_ZERO;

        if (real && place_x >= 0)
                personal_light(place_x, place_y, place_z);

        //zooming
        zoom_amt *= pow(zooming ? 0.9f : 1.2f, amt);
        CLAMP(zoom_amt, 0.25f, 1.0f);
}

//collide a box with nearby world tiles
int world_collide(struct box box, int wet)
{
        for (int i = -1; i < 2; i++) for (int j = -1; j < 3; j++) for (int k = -1; k < 2; k++)
        {
                int bx = box.x/BS + i;
                int by = box.y/BS + j;
                int bz = box.z/BS + k;

                if (block_collide(bx, by, bz, box, wet))
                        return 1;
        }

        return 0;
}

//return 0 iff we couldn't actually move
int move_player(struct player *p, int velx, int vely, int velz)
{
        int last_was_x = 0;
        int last_was_z = 0;
        int already_stuck = 0;
        int moved = 0;

        if (!velx && !vely && !velz)
                return 1;

        if (world_collide(p->pos, 0))
                already_stuck = 1;

        while (velx || vely || velz)
        {
                struct box testpos = p->pos;
                int amt;

                if ((!velx && !velz) || ((last_was_x || last_was_z) && vely))
                {
                        amt = vely > 0 ? 1 : -1;
                        testpos.y += amt;
                        vely -= amt;
                        last_was_x = 0;
                        last_was_z = 0;
                }
                else if (!velz || (last_was_z && velx))
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

                if (world_collide(testpos, 0))
                        would_be_stuck = 1;
                else
                        already_stuck = 0;

                if (would_be_stuck && !already_stuck)
                {
                        if (last_was_x)
                                velx = 0;
                        else if (last_was_z)
                                velz = 0;
                        else
                                vely = 0;
                        continue;
                }

                p->pos = testpos;
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
int block_collide(int bx, int by, int bz, struct box box, int wet)
{
        if (!legit_tile(bx, by, bz))
                return 0;

        if (wet && tiles[bz][by][bx] == WATR)
                return collide(box, (struct box){BS*bx, BS*by, BS*bz, BS, BS, BS});

        if (!wet && tiles[bz][by][bx] <= LASTSOLID)
                return collide(box, (struct box){BS*bx, BS*by, BS*bz, BS, BS, BS});

        return 0;
}

//select block from eye following vector f
void rayshot(float eye0, float eye1, float eye2, float f0, float f1, float f2)
{
        int x = (int)(eye0 / BS);
        int y = (int)(eye1 / BS);
        int z = (int)(eye2 / BS);

        for (int i = 0; ; i++)
        {
                float a0 = (BS * (x + (f0 > 0 ? 1 : 0)) - eye0) / f0;
                float a1 = (BS * (y + (f1 > 0 ? 1 : 0)) - eye1) / f1;
                float a2 = (BS * (z + (f2 > 0 ? 1 : 0)) - eye2) / f2;
                float amt = 0;

                place_x = x;
                place_y = y;
                place_z = z;

                if (a0 < a1 && a0 < a2) { x += (f0 > 0 ? 1 : -1); amt = a0; }
                else if (a1 < a2)       { y += (f1 > 0 ? 1 : -1); amt = a1; }
                else                    { z += (f2 > 0 ? 1 : -1); amt = a2; }

                eye0 += amt * f0 * 1.0001;
                eye1 += amt * f1 * 1.0001;
                eye2 += amt * f2 * 1.0001;

                if (x < 0 || y < 0 || z < 0 || x >= TILESW || y >= TILESH || z >= TILESD)
                        goto bad;

                if (tiles[z][y][x] != OPEN)
                        break;

                if (i == 6)
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

        // compute proj matrix
        float near = 8.f;
        float far = 99999.f;
        float frustw = 4.5f * zoom_amt * screenw / screenh;
        float frusth = 4.5f * zoom_amt;
        float frustM[] = {
                near/frustw,           0,                                  0,  0,
                          0, near/frusth,                                  0,  0,
                          0,           0,       -(far + near) / (far - near), -1,
                          0,           0, -(2.f * far * near) / (far - near),  0
        };
        glUniformMatrix4fv(glGetUniformLocation(prog_id, "proj"), 1, GL_FALSE, frustM);

        // compute view matrix
        float eye0, eye1, eye2;
        eye0 = camplayer.pos.x + PLYR_W / 2;
        eye1 = camplayer.pos.y + EYEDOWN * (camplayer.sneaking ? 2 : 1);
        eye2 = camplayer.pos.z + PLYR_W / 2;
        float f0, f1, f2;
        f0 = cos(camplayer.pitch) * sin(camplayer.yaw);
        f1 = sin(camplayer.pitch);
        f2 = cos(camplayer.pitch) * cos(camplayer.yaw);
        float wing0, wing1, wing2;
        wing0 = -cos(camplayer.yaw);
        wing1 = 0;
        wing2 = sin(camplayer.yaw);
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
        float zz0, zz1, zz2;
        zz0 = s0/sm;
        zz1 = s1/sm;
        zz2 = s2/sm;
        float u0, u1, u2;
        u0 = zz1*f2 - zz2*f1;
        u1 = zz2*f0 - zz0*f2;
        u2 = zz0*f1 - zz1*f0;
        float viewM[] = {
                s0, u0,-f0, 0,
                s1, u1,-f1, 0,
                s2, u2,-f2, 0,
                 0,  0,  0, 1
        };

        //glUniform4f(glGetUniformLocation(prog_id, "eye"), eye0, eye1, eye2, 1);
        glUniform4f(glGetUniformLocation(prog_id, "eye"), 0, 0, 0, 1);

        // find where we are pointing at
        rayshot(eye0, eye1, eye2, f0, f1, f2);

        // translate by hand
        viewM[12] = (viewM[0] * -eye0) + (viewM[4] * -eye1) + (viewM[ 8] * -eye2);
        viewM[13] = (viewM[1] * -eye0) + (viewM[5] * -eye1) + (viewM[ 9] * -eye2);
        viewM[14] = (viewM[2] * -eye0) + (viewM[6] * -eye1) + (viewM[10] * -eye2);

        glUniformMatrix4fv(glGetUniformLocation(prog_id, "view"), 1, GL_FALSE, viewM);

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthFunc(GL_LEQUAL);

        // identity for model view for world drawing
        float modelM[] = {
                1, 0, 0, 0,
                0, 1, 0, 0,
                0, 0, 1, 0,
                0, 0, 0, 1
        };
        glUniformMatrix4fv(glGetUniformLocation(prog_id, "model"), 1, GL_FALSE, modelM);

        glUniform1f(glGetUniformLocation(prog_id, "BS"), BS);

        // determine which chunks to send to gl
        TIMER(rings)
        int x0 = ((int)camplayer.pos.x - BS2 * CHUNKW) / (BS * CHUNKW);
        int z0 = ((int)camplayer.pos.z - BS2 * CHUNKD) / (BS * CHUNKD);
        CLAMP(x0, 0, VAOW - 2);
        CLAMP(z0, 0, VAOD - 2);
        int x1 = x0 + 1;
        int z1 = z0 + 1;

        // initialize with ring0 chunks
        struct qitem fresh[104] = {{x0, 0, z0}, {x0, 0, z1}, {x1, 0, z0}, {x1, 0, z1}};
        size_t fresh_len = 4;

        // position within each ring that we're at this frame
	static struct qitem ringpos[VAOW + VAOD] = {0};
        for (int r = 1; r < VAOW + VAOD; r++)
        {
		// expand ring in all directions
		x0--; x1++; z0--; z1++;

                // freshen farther rings less and less often
                if (r >= 3 && r <= 6 && frame % 2 != r % 2)   continue;
                if (r >= 7 && r <= 14 && frame % 4 != r % 4)  continue;
                if (r >= 15 && r <= 30 && frame % 8 != r % 8) continue;
                if (r >= 31 && frame % 16 != r % 16)          continue;

                int *x = &ringpos[r].x;
                int *z = &ringpos[r].z;

                // move to next chunk, maybe on ring
                --(*x);

                // wrap around the ring
		int x_too_low = (*x < x0);
                if (x_too_low) { *x = x1; --(*z); }

                // reset if out of the ring
		int z_too_low = (*z < z0);
                if (z_too_low) { *x = x1; *z = z1; }

                // get out of the middle
		int is_on_ring = (*z == z0 || *z == z1 || *x == x1);
                if (!is_on_ring) { *x = x0; }

                // render if in bounds
                if (*x >= 0 && *x < VAOW && *z >= 0 && *z < VAOD)
                {
                        fresh[fresh_len].x = *x;
                        fresh[fresh_len].z = *z;
                        fresh_len++;
                }
        }

        // render non-fresh chunks
        TIMER(drawstale)
        for (int i = 0; i < VAOW; i++) for (int j = 0; j < VAOD; j++)
        {
                // skip chunks we will draw fresh this frame
                for (size_t k = 0; k < fresh_len; k++)
                        if (fresh[k].x == i && fresh[k].z == j)
                                goto skip;

                int myvbo = i * VAOD + j;
                glBindVertexArray(vao[myvbo]);
                glDrawArrays(GL_POINTS, 0, vbo_len[myvbo]);
                polys += vbo_len[myvbo];

                skip: ;
        }

        // build and render fresh chunks (while the stales are rendering!)
        TIMER(buildvbo);
        for (size_t my = 0; my < fresh_len; my++)
        {
                int myx = fresh[my].x;
                int myz = fresh[my].z;
                int myvbo = myx * VAOD + myz;

                glBindVertexArray(vao[myvbo]);
                glBindBuffer(GL_ARRAY_BUFFER, vbo[myvbo]);
                v = vbuf; // reset vertex buffer pointer

                int xlo = myx * CHUNKW;
                int xhi = xlo + CHUNKW;
                int zlo = myz * CHUNKD;
                int zhi = zlo + CHUNKD;

                if (!already_generated[myx][myz])
                {
                        TIMECALL(gen_chunk, (xlo-1, xhi+1, zlo-1, zhi+1));
                        already_generated[myx][myz] = 1;
                }

                TIMECALL(recalc_corner_lighting, (xlo, xhi, zlo, zhi));
                TIMER(buildvbo);

                for (int z = zlo; z < zhi; z++) for (int y = 0; y < TILESH; y++) for (int x = xlo; x < xhi; x++)
                {
                        if (v >= v_limit) break; //shouldnt reasonably happen

                        if (tiles[z][y][x] == OPEN) continue;

                        //lighting
                        float usw = cornlight[z  ][y  ][x  ];
                        float use = cornlight[z  ][y  ][x+1];
                        float unw = cornlight[z+1][y  ][x  ];
                        float une = cornlight[z+1][y  ][x+1];
                        float dsw = cornlight[z  ][y+1][x  ];
                        float dse = cornlight[z  ][y+1][x+1];
                        float dnw = cornlight[z+1][y+1][x  ];
                        float dne = cornlight[z+1][y+1][x+1];
                        int t = tiles[z][y][x];
                        if (t == GRAS)
                        {
                                if (y == 0        || tiles[z  ][y-1][x  ] >= OPEN) *v++ = (struct vbufv){ 0,    UP, x, y, z, usw, use, unw, une, 1 };
                                if (z == 0        || tiles[z-1][y  ][x  ] >= OPEN) *v++ = (struct vbufv){ 1, SOUTH, x, y, z, use, usw, dse, dsw, 1 };
                                if (z == TILESD-1 || tiles[z+1][y  ][x  ] >= OPEN) *v++ = (struct vbufv){ 1, NORTH, x, y, z, unw, une, dnw, dne, 1 };
                                if (x == 0        || tiles[z  ][y  ][x-1] >= OPEN) *v++ = (struct vbufv){ 1,  WEST, x, y, z, usw, unw, dsw, dnw, 1 };
                                if (x == TILESW-1 || tiles[z  ][y  ][x+1] >= OPEN) *v++ = (struct vbufv){ 1,  EAST, x, y, z, une, use, dne, dse, 1 };
                                if (y <  TILESH-1 && tiles[z  ][y+1][x  ] >= OPEN) *v++ = (struct vbufv){ 2,  DOWN, x, y, z, dse, dsw, dne, dnw, 1 };
                        }
                        else if (t == DIRT || t == GRG1 || t == GRG2)
                        {
                                int u = (t == DIRT) ? 2 :
                                        (t == GRG1) ? 3 : 4;
                                if (y == 0        || tiles[z  ][y-1][x  ] >= OPEN) *v++ = (struct vbufv){ u,    UP, x, y, z, usw, use, unw, une, 1 };
                                if (z == 0        || tiles[z-1][y  ][x  ] >= OPEN) *v++ = (struct vbufv){ 2, SOUTH, x, y, z, use, usw, dse, dsw, 1 };
                                if (z == TILESD-1 || tiles[z+1][y  ][x  ] >= OPEN) *v++ = (struct vbufv){ 2, NORTH, x, y, z, unw, une, dnw, dne, 1 };
                                if (x == 0        || tiles[z  ][y  ][x-1] >= OPEN) *v++ = (struct vbufv){ 2,  WEST, x, y, z, usw, unw, dsw, dnw, 1 };
                                if (x == TILESW-1 || tiles[z  ][y  ][x+1] >= OPEN) *v++ = (struct vbufv){ 2,  EAST, x, y, z, une, use, dne, dse, 1 };
                                if (y <  TILESH-1 && tiles[z  ][y+1][x  ] >= OPEN) *v++ = (struct vbufv){ 2,  DOWN, x, y, z, dse, dsw, dne, dnw, 1 };
                        }
                        else if (t == STON || t == SAND || t == ORE)
                        {
                                int f = (t == STON) ? 5 :
                                        (t == SAND) ? 6 : 8;
                                if (y == 0        || tiles[z  ][y-1][x  ] >= OPEN) *v++ = (struct vbufv){ f,    UP, x, y, z, usw, use, unw, une, 1 };
                                if (z == 0        || tiles[z-1][y  ][x  ] >= OPEN) *v++ = (struct vbufv){ f, SOUTH, x, y, z, use, usw, dse, dsw, 1 };
                                if (z == TILESD-1 || tiles[z+1][y  ][x  ] >= OPEN) *v++ = (struct vbufv){ f, NORTH, x, y, z, unw, une, dnw, dne, 1 };
                                if (x == 0        || tiles[z  ][y  ][x-1] >= OPEN) *v++ = (struct vbufv){ f,  WEST, x, y, z, usw, unw, dsw, dnw, 1 };
                                if (x == TILESW-1 || tiles[z  ][y  ][x+1] >= OPEN) *v++ = (struct vbufv){ f,  EAST, x, y, z, une, use, dne, dse, 1 };
                                if (y <  TILESH-1 && tiles[z  ][y+1][x  ] >= OPEN) *v++ = (struct vbufv){ f,  DOWN, x, y, z, dse, dsw, dne, dnw, 1 };
                        }
                        else if (t == WATR)
                        {
                                if (y == 0        || tiles[z  ][y-1][x  ] == OPEN) {
                                                                                   *v++ = (struct vbufv){ 7,    UP, x, y+0.06f, z, usw, use, unw, une, 0.5f };
                                                                                   *v++ = (struct vbufv){ 7,  DOWN, x, y-0.94f, z, dse, dsw, dne, dnw, 0.5f };
                                }
                        }
                }

                vbo_len[myvbo] = v - vbuf;
                polys += vbo_len[myvbo];
                TIMER(glBufferData)
                glBufferData(GL_ARRAY_BUFFER, vbo_len[myvbo] * sizeof *vbuf, vbuf, GL_STATIC_DRAW);
                TIMER(glDrawArrays)
                glDrawArrays(GL_POINTS, 0, vbo_len[myvbo]); // draw the newly buffered verts
        }

        TIMER(swapwindow);
        SDL_GL_SwapWindow(win);
        TIMER();
}

void debrief()
{
        static unsigned last_ticks = 0;
        static unsigned last_frame = 0;
        unsigned ticks = SDL_GetTicks();

        if (ticks - last_ticks >= 1000) {
                if (noisy) {
                        float elapsed = ((float)ticks - last_ticks);
                        float frames = frame - last_frame;
                        printf("%.1f FPS\n", 1000.f * frames / elapsed );
                        printf("%.1f polys/sec\n", 1000.f * (float)polys / elapsed);
                        printf("%.1f polys/frame\n", (float)polys / frames);
                        printf("player pos X=%0.0f Y=%0.0f Z=%0.0f\n", player[0].pos.x, player[0].pos.y, player[0].pos.z);
                        printf("player block X=%0.0f Y=%0.0f Z=%0.0f\n", player[0].pos.x / BS, player[0].pos.y / BS, player[0].pos.z / BS);
                        timer_print();
                        printf("perlin calls %lld\n", perlin_calls);
                }
                last_ticks = ticks;
                last_frame = frame;
                polys = 0;
        }
}

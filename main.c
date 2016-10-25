#!/usr/bin/env bash

#if 0
tcc -I/usr/include/SDL2 -lSDL2 -run $0
exit
#endif

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <SDL.h>

#define SHAPES 7
#define BWIDTH 10  // board width, height
#define BHEIGHT 20
#define BIT(i, j) (1 << (4*(3-(j)) + (3-(i))))

unsigned int shapes[] = {
        0x0   , 0x0   , 0x0   , 0x0   , //          Example for J piece:
        0x0226, 0x0047, 0x0644, 0x0071, // J        8421                
        0x0446, 0x0017, 0x0622, 0x0074, // L       *----*               
        0x0066, 0x0066, 0x0066, 0x0066, // square  |    | -> 0          
        0x0264, 0x0063, 0x0264, 0x0063, // Z       |  X | -> 2          
        0x0462, 0x0036, 0x0462, 0x0036, // S       |  X | -> 2 -> 0x0226
        0x0027, 0x0464, 0x0072, 0x0262, // T       | XX | -> 6          
        0x2222, 0x000F, 0x2222, 0x000F, // line    *----*               
};

unsigned char colors[] = {
         25,  40,  35,
        242, 245, 237,
        255, 194,   0,
        255,  91,   0,
        184,   0,  40,
        132,   0,  46,
         74, 192, 242,
        221,  30,  47, // J
        235, 176,  53, // L
          6, 162, 203, // square
         33, 133,  89, // Z
        162,  89,  33, // S
         33,   6, 177, // T
        208, 198, 177, // line
};

unsigned char board[BWIDTH][BHEIGHT];

int falling_x;
int falling_y;
int falling_shape;
int falling_spin;

int idle_time;

SDL_Event event;
SDL_Renderer *renderer;
int running = 1;

// enginey protos
void setup();
void key_down();
void update_stuff();
void draw_stuff();
void set_shape_color(int shape, int shade);

// gamey protos
void new_piece();
void move(int dx, int dy);
int collide(int x, int y);
void bake();
void slam();
void spin(int dir);

int main()
{
        setup();

        while(running)
        {
                while(SDL_PollEvent(&event)) switch(event.type)
                {
                        case SDL_QUIT: running = 0; break;
                        case SDL_KEYDOWN: key_down(); break;
                }

                update_stuff();
                draw_stuff();
                SDL_Delay(10);
                idle_time++;
        }

        return 0;
}

void setup()
{
        srand(time(NULL));

        SDL_Init(SDL_INIT_VIDEO);

        SDL_Window *win = SDL_CreateWindow("Tet",
                        100, 100, 640, 480, SDL_WINDOW_SHOWN);

        renderer = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);

        if(!renderer)
        {
                fprintf(stderr, "Could not create SDL renderer for some reason\n");
                exit(-1);
        }
}

void key_down()
{
        switch(event.key.keysym.sym)
        {
                case SDLK_a: case SDLK_LEFT:  move(-1, 0); break;
                case SDLK_d: case SDLK_RIGHT: move( 1, 0); break;
                case SDLK_w: case SDLK_UP:    slam();      break;
                case SDLK_s: case SDLK_DOWN:  move( 0, 1); break;
                case SDLK_q: case SDLK_z:     spin(-1);    break;
                case SDLK_e: case SDLK_x:     spin( 1);    break;
        }
}

void update_stuff()
{
        if(!falling_shape)
        {
                new_piece();
        }

        if(idle_time >= 30)
        {
                move(0, 1);
        }
}

void new_piece()
{
        falling_shape = rand() % SHAPES + 1;
        falling_x = 3;
        falling_y = -3;
}

void move(int dx, int dy)
{
        if(!collide(falling_x + dx, falling_y + dy))
        {
                falling_x += dx;
                falling_y += dy;
        }
        else if(dy)
        {
                bake();
                falling_shape = 0;
        }

        if(dy)
        {
                idle_time = 0;
        }
}

int collide(int x, int y)
{
        for(int i = 0; i < 4; i++) for(int j = 0; j < 4; j++)
        {
                int world_i = i + x;
                int world_j = j + y;

                if(!(shapes[falling_shape*4 + falling_spin] & BIT(i, j)))
                        continue;

                if(world_j < 0)
                        continue;

                if(world_i < 0 || world_i >= BWIDTH || world_j >= BHEIGHT)
                        return 1;

                if(board[world_i][world_j])
                        return 1;
        }

        return 0;
}

void bake()
{
        for(int i = 0; i < 4; i++) for(int j = 0; j < 4; j++)
        {
                int world_i = i + falling_x;
                int world_j = j + falling_y;

                if(!(shapes[falling_shape*4 + falling_spin] & BIT(i, j)))
                        continue;

                if(world_i < 0 || world_i >= BWIDTH || world_j < 0 || world_j >= BHEIGHT)
                        continue;

                board[world_i][world_j] = falling_shape;
        }
}

void slam()
{
        falling_y++;
        idle_time = 0;
}

void spin(int dir)
{
        falling_spin++;
        falling_spin %= 4;
}

void draw_stuff()
{
        //draw green everywhere (not so racist)
        SDL_SetRenderDrawColor(renderer, 17, 143, 7, 255);
        SDL_RenderClear(renderer);

        SDL_SetRenderDrawColor(renderer, 25, 40, 35, 255);
        SDL_RenderFillRect(renderer, &(SDL_Rect){10, 10, 200, 400});

        //draw falling piece
        for(int i = 0; i < 4; i++) for(int j = 0; j < 4; j++)
        {
                int world_i = i + falling_x;
                int world_j = j + falling_y;

                if(!(shapes[falling_shape*4 + falling_spin] & BIT(i, j)))
                        continue;

                if(world_j < 0)
                        continue;

                set_shape_color(falling_shape, -25);
                SDL_RenderDrawRect(renderer, &(SDL_Rect){
                        10 + 20 * world_i,
                        10 + 20 * world_j,
                        20,
                        20
                });

                set_shape_color(falling_shape, 0);
                SDL_RenderFillRect(renderer, &(SDL_Rect){
                        11 + 20 * world_i,
                        11 + 20 * world_j,
                        18,
                        18
                });
        }

        for(int i = 0; i < BWIDTH; i++) for(int j = 0; j < BHEIGHT; j++)
        {
                if(!board[i][j])
                        continue;

                set_shape_color(board[i][j], -25);
                SDL_RenderDrawRect(renderer, &(SDL_Rect){
                        10 + 20 * i,
                        10 + 20 * j,
                        20,
                        20
                });

                set_shape_color(board[i][j], 0);
                SDL_RenderFillRect(renderer, &(SDL_Rect){
                        11 + 20 * i,
                        11 + 20 * j,
                        18,
                        18
                });
        }

        //done drawing stuff
        SDL_RenderPresent(renderer);
}

void set_shape_color(int shape, int shade)
{
        int r = colors[shape*3 + 0] + shade;
        int g = colors[shape*3 + 1] + shade;
        int b = colors[shape*3 + 2] + shade;

        if(r > 255) r = 255;
        if(r <   0) r =   0;
        if(g > 255) g = 255;
        if(g <   0) g =   0;
        if(b > 255) b = 255;
        if(b <   0) b =   0;

        SDL_SetRenderDrawColor(renderer, r, g, b, 255);
}

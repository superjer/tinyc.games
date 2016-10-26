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
        0x4444, 0x00F0, 0x4444, 0x00F0, // line    *----*
};

unsigned char colors[] = {
         25,  40,  35,
        242, 245, 237, // J
        255, 194,   0, // L
         15, 127, 127, // square
        255,  91,   0, // Z
        184,   0,  40, // S
         74, 192, 242, // T
        132,   0,  46, // line
        221,  30,  47,
        235, 176,  53,
          6, 162, 203,
         33, 133,  89,
        162,  89,  33,
         33,   6, 177,
        208, 198, 177,
        255, 255, 255,
};

unsigned char board[BHEIGHT][BWIDTH];

int killy_lines[BHEIGHT];

int falling_x;
int falling_y;
int falling_shape;
int falling_rot;

int idle_time;
int shine_time;
int dead_time;

SDL_Event event;
SDL_Renderer *renderer;
int running = 1;

// enginey protos
void setup();
void key_down();
void update_stuff();
void draw_stuff();
void draw_square(int x, int y, int shape);
void set_shape_color(int shape, int shade);

// gamey protos
void new_piece();
void move(int dx, int dy);
int collide(int x, int y, int rot);
void bake();
void check_lines();
void check_dead();
void shine_line(int y);
void kill_lines();
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
                        100, 100, 420, 820, SDL_WINDOW_SHOWN);

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
        if(!falling_shape && !shine_time && !dead_time)
                new_piece();

        if(shine_time > 0)
        {
                shine_time--;
                if(shine_time == 0)
                        kill_lines();
        }

        if(dead_time > 0)
        {
                int x = (dead_time-100) % BWIDTH;
                int y = (dead_time-100) / BWIDTH;

                if(y >= 0)
                        board[y][x] = rand() % 7 + 1;

                dead_time--;

                if(dead_time == 0)
                        memset(board, 0, sizeof board);
        }

        if(idle_time >= 30)
                move(0, 1);
}

void new_piece()
{
        falling_shape = rand() % SHAPES + 1;
        falling_x = 3;
        falling_y = -3;
}

void move(int dx, int dy)
{
        if(!collide(falling_x + dx, falling_y + dy, falling_rot))
        {
                falling_x += dx;
                falling_y += dy;
        }
        else if(dy)
        {
                bake();
        }

        if(dy)
                idle_time = 0;
}

int collide(int x, int y, int rot)
{
        for(int i = 0; i < 4; i++) for(int j = 0; j < 4; j++)
        {
                int world_i = i + x;
                int world_j = j + y;

                if(!(shapes[falling_shape*4 + rot] & BIT(i, j)))
                        continue;

                if(world_j < 0)
                        continue;

                if(world_i < 0 || world_i >= BWIDTH || world_j >= BHEIGHT)
                        return 1;

                if(board[world_j][world_i])
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

                if(!(shapes[falling_shape*4 + falling_rot] & BIT(i, j)))
                        continue;

                if(world_i < 0 || world_i >= BWIDTH || world_j < 0 || world_j >= BHEIGHT)
                        continue;

                if(board[world_j][world_i])
                        dead_time = BWIDTH * BHEIGHT + 99;

                board[world_j][world_i] = falling_shape;
        }

        check_lines();
        falling_shape = 0;
}

void check_lines()
{
        for(int j = BHEIGHT - 1; j >= 0; j--)
        {
                for(int i = 0; i < BWIDTH; i++)
                {
                        if(!board[j][i])
                                break;

                        if(i == BWIDTH - 1)
                                shine_line(j);
                }
        }
}

void shine_line(int y)
{
        shine_time = 100;
        killy_lines[y] = 1;
        for(int i = 0; i < BWIDTH; i++)
                board[y][i] = 8;
}

void kill_lines()
{
        for(int y = 0; y < BHEIGHT; y++)
        {
                if(!killy_lines[y])
                        continue;

                killy_lines[y] = 0;
                memset(board[0], 0, sizeof *board);

                for(int j = y; j > 0; j--)
                {
                        for(int i = 0; i < BWIDTH; i++)
                                board[j][i] = board[j-1][i];
                }
        }
}

void slam()
{
        while(!collide(falling_x, falling_y + 1, falling_rot))
                falling_y++;

        idle_time = 0;
        bake();
}

void spin(int dir)
{
        int new_rot = (falling_rot + 1) % 4;

        if(!collide(falling_x, falling_y, new_rot))
        {
                falling_rot = new_rot;
        }
        else if(!collide(falling_x - 1, falling_y, new_rot))
        {
                falling_x -= 1;
                falling_rot = new_rot;
        }

}

void draw_stuff()
{
        //draw green everywhere (not so racist)
        SDL_SetRenderDrawColor(renderer, 25, 40, 35, 255);
        SDL_RenderClear(renderer);

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderFillRect(renderer, &(SDL_Rect){10, 10, 400, 800});

        //draw falling piece
        for(int i = 0; i < 4; i++) for(int j = 0; j < 4; j++)
        {
                int world_i = i + falling_x;
                int world_j = j + falling_y;

                if(!(shapes[falling_shape*4 + falling_rot] & BIT(i, j)))
                        continue;

                if(world_j < 0)
                        continue;

                draw_square(world_i, world_j, falling_shape);
        }

        //draw board
        for(int i = 0; i < BWIDTH; i++) for(int j = 0; j < BHEIGHT; j++)
        {
                if(!board[j][i])
                        continue;

                draw_square(i, j, board[j][i]);
        }

        //done drawing stuff
        SDL_RenderPresent(renderer);
}

void draw_square(int x, int y, int shape)
{
        set_shape_color(shape, -25);
        SDL_RenderDrawRect(renderer, &(SDL_Rect){
                10 + 40 * x,
                10 + 40 * y,
                40,
                40
        });

        set_shape_color(shape, 0);
        SDL_RenderFillRect(renderer, &(SDL_Rect){
                11 + 40 * x,
                11 + 40 * y,
                38,
                38
        });
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

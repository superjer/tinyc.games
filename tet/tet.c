#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <SDL.h>
#include <SDL_ttf.h>

#define BWIDTH 10  // board width, height
#define BHEIGHT 20
#define BS 30      // size of one block
#define BS2 (BS/2) // size of half a block
#define PREVIEW_BOX_X (10 + BS * BWIDTH + 10 + BS2)
#define MAX(a,b) ((a)>(b)?(a):(b))

char shapes[] =
        ".... .... .... .... .... .... .... .O.. "
        ".... .OO. .OO. .OO. ..O. .O.. .... .O.. "
        ".... .O.. ..O. .OO. .OO. .OO. OOO. .O.. "
        ".... .O.. ..O. .... .O.. ..O. .O.. .O.. "
        ".... .... .... .... .... .... .... .... "
        ".... .O.. ..O. .OO. OO.. .OO. .O.. .... "
        ".... .OOO OOO. .OO. .OO. OO.. .OO. OOOO "
        ".... .... .... .... .... .... .O.. .... "
        ".... .... .... .... .... .... .... .O.. "
        ".... ..O. .O.. .OO. ..O. .O.. .O.. .O.. "
        ".... ..O. .O.. .OO. .OO. .OO. OOO. .O.. "
        ".... .OO. .OO. .... .O.. ..O. .... .O.. "
        ".... .... .... .... .... .... .... .... "
        ".... .OOO OOO. .OO. OO.. .OO. .O.. .... "
        ".... ...O O... .OO. .OO. OO.. OO.. OOOO "
        ".... .... .... .... .... .... .O.. .... ";

int center[] = { // helps center shapes in preview box
         0,0, 0,0, 0,0, 0,1, 0,0, 0,0, 1,-1,1,1,
};

unsigned char colors[] = {
        0,     0,   0, // unused
        242, 245, 237, // J-piece
        255, 194,   0, // L-piece
        15,  127, 127, // square
        255,  91,   0, // Z
        184,   0,  40, // S
        74,  192, 242, // T
        132,   0,  46, // line-piece
        255, 255, 255, // shine color
};

unsigned char board[BHEIGHT][BWIDTH];
int killy_lines[BHEIGHT];

int falling_x;
int falling_y;
int falling_shape;
int falling_rot;
int next_shape;
int lines;
int score;
int best;
int idle_time;
int shine_time;
int dead_time;

SDL_Event event;
SDL_Renderer *renderer;
TTF_Font *font;

//prototypes
void setup();
void key_down();
void update_stuff();
void draw_stuff();
void draw_square(int x, int y, int shape);
void set_color_from_shape(int shape, int shade);
void text(char *fstr, int value, int x, int y);
void new_game();
void new_piece();
void move(int dx, int dy);
int is_solid_part(int shape, int rot, int i, int j);
int collide(int x, int y, int rot);
void bake();
void check_lines();
void shine_line(int y);
void kill_lines();
void slam();
void spin(int dir);

//the entry point and main game loop
int main()
{
        setup();
        new_game();

        for(;;)
        {
                while(SDL_PollEvent(&event)) switch(event.type)
                {
                        case SDL_QUIT: exit(0);
                        case SDL_KEYDOWN: key_down(); break;
                }

                update_stuff();
                draw_stuff();
                SDL_Delay(10);
                idle_time++;
        }
}

//initial setup to get the window and rendering going
void setup()
{
        srand(time(NULL));
        SDL_Init(SDL_INIT_VIDEO);

        SDL_Window *win = SDL_CreateWindow("Tet",
                        SDL_WINDOWPOS_UNDEFINED,
                        SDL_WINDOWPOS_UNDEFINED,
                        10 + BWIDTH * BS + 10 + 5 * BS + 10,
                        10 + BHEIGHT * BS + 10,
                        SDL_WINDOW_SHOWN);

        renderer = SDL_CreateRenderer(win, -1, 0);
        if(!renderer)
        {
                fprintf(stderr, "Could not create SDL renderer for some reason\n");
                exit(-1);
        }

        TTF_Init();
        font = TTF_OpenFont("res/LiberationSans-Regular.ttf", 28);
}

//handle a key press from the player
void key_down()
{
        if(falling_shape) switch(event.key.keysym.sym)
        {
                case SDLK_a: case SDLK_LEFT:  move(-1, 0); break;
                case SDLK_d: case SDLK_RIGHT: move( 1, 0); break;
                case SDLK_w: case SDLK_UP:    slam();      break;
                case SDLK_s: case SDLK_DOWN:  move( 0, 1); break;
                case SDLK_q: case SDLK_z:     spin(-1);    break;
                case SDLK_e: case SDLK_x:     spin( 1);    break;
        }
}

//update everything that needs to update on its own, without input
void update_stuff()
{
        if(!falling_shape && !shine_time && !dead_time)
        {
                new_piece();
                falling_x = 3;
                falling_y = -3;
                falling_rot = 0;
        }

        if(shine_time > 0)
        {
                shine_time--;
                if(shine_time == 0)
                        kill_lines();
        }

        if(dead_time > 0)
        {
                int x = (dead_time) % BWIDTH;
                int y = (dead_time) / BWIDTH;

                if(y >= 0 && y < BHEIGHT && x >= 0 && x < BWIDTH)
                        board[y][x] = rand() % 7 + 1;

                if(--dead_time == 0)
                        new_game();
        }

        if(idle_time >= 30)
                move(0, 1);
}

//reset score and pick one extra random piece
void new_game()
{
        memset(board, 0, sizeof board);
        new_piece();
        if(best < score) best = score;
        score = 0;
        lines = 0;
        falling_shape = 0;
}

//randomly pick a new next piece, and put the old on in play
void new_piece()
{
        falling_shape = next_shape;
        next_shape = rand() % 7 + 1; // 7 shapes
}

//move the falling piece left, right, or down
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
                falling_shape = 0;
        }

        if(dy) idle_time = 0;
}

//check if a sub-part of the falling shape is solid at a particular rotation
int is_solid_part(int shape, int rot, int i, int j)
{
        int base = shape*5 + rot*5*8*4;
        return shapes[base + j*5*8 + i] == 'O';
}

//check if the falling piece would collide at a certain position and rotation
int collide(int x, int y, int rot)
{
        for(int i = 0; i < 4; i++) for(int j = 0; j < 4; j++)
        {
                int world_i = i + x;
                int world_j = j + y;

                if(!is_solid_part(falling_shape, rot, i, j))
                        continue;

                if(world_i < 0 || world_i >= BWIDTH || world_j >= BHEIGHT)
                        return 1;

                if(world_j < 0)
                        continue;

                if(board[world_j][world_i])
                        return 1;
        }
        return 0;
}

//bake the falling piece into the background/board
void bake()
{
        for(int i = 0; i < 4; i++) for(int j = 0; j < 4; j++)
        {
                int world_i = i + falling_x;
                int world_j = j + falling_y;

                if(!is_solid_part(falling_shape, falling_rot, i, j))
                        continue;

                if(world_i < 0 || world_i >= BWIDTH || world_j < 0 || world_j >= BHEIGHT)
                        continue;

                if(board[world_j][world_i]) // already a block here? game over
                {
                        dead_time = BWIDTH * BHEIGHT;
                        next_shape = 0;
                }

                board[world_j][world_i] = falling_shape;
        }

        //check if there are any completed horizontal lines
        for(int j = BHEIGHT - 1; j >= 0; j--)
        {
                for(int i = 0; i < BWIDTH && board[j][i]; i++)
                {
                        if(i == BWIDTH - 1) shine_line(j);
                }
        }
}

//make a completed line "shine" and mark it to be removed
void shine_line(int y)
{
        shine_time = 50;
        killy_lines[y] = 1;
        for(int i = 0; i < BWIDTH; i++)
                board[y][i] = 8; //shiny!
}

//remove lines that were marked to be removed by shine_line()
void kill_lines()
{
        int new_lines = 0;
        for(int y = 0; y < BHEIGHT; y++)
        {
                if(!killy_lines[y])
                        continue;

                lines++;
                new_lines++;
                killy_lines[y] = 0;
                memset(board[0], 0, sizeof *board);

                for(int j = y; j > 0; j--)
                {
                        for(int i = 0; i < BWIDTH; i++)
                                board[j][i] = board[j-1][i];
                }
        }

        switch(new_lines)
        {
                case 1: score += 100;  break;
                case 2: score += 250;  break;
                case 3: score += 500;  break;
                case 4: score += 1000; break;
        }
}

//move the falling piece as far down as it will go
void slam()
{
        for(; !collide(falling_x, falling_y + 1, falling_rot); falling_y++)
                idle_time = 0;
}

//spin the falling piece left or right, if possible
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

//draw everything in the game on the screen
void draw_stuff()
{
        // draw background, black boxes
        SDL_SetRenderDrawColor(renderer, 25, 40, 35, 255);
        SDL_RenderClear(renderer);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderFillRect(renderer, &(SDL_Rect){10, 10, BS * BWIDTH, BS * BHEIGHT});
        SDL_RenderFillRect(renderer, &(SDL_Rect){10 + BS * BWIDTH + 10, 10, BS * 5, BS * 5});

        //draw falling piece & shadow
        for(int i = 0; i < 4; i++) for(int j = 0; j < 4; j++)
        {
                int world_i = i + falling_x;
                int world_j = j + falling_y;
                int shadow_j = MAX(world_j + 1, 0);

                if(!is_solid_part(falling_shape, falling_rot, i, j))
                        continue;

                SDL_SetRenderDrawColor(renderer, 8, 13, 12, 255);
                SDL_RenderFillRect(renderer, &(SDL_Rect){
                        10 + BS * world_i,
                        10 + BS * shadow_j,
                        BS,
                        BS * (BHEIGHT - shadow_j)
                });

                if(world_j >= 0)
                        draw_square(10 + BS * world_i, 10 + BS * world_j, falling_shape);
        }

        //draw next piece, centered in the preview box
        for(int i = 0; i < 4; i++) for(int j = 0; j < 4; j++)
        {
                if(is_solid_part(next_shape, 0, i, j))
                        draw_square(
                                PREVIEW_BOX_X + BS * i + BS2 * center[2*next_shape],
                                10 + BS * j + BS2 * center[2*next_shape + 1],
                                next_shape
                        );
        }

        //draw board pieces
        for(int i = 0; i < BWIDTH; i++) for(int j = 0; j < BHEIGHT; j++)
        {
                if(board[j][i])
                        draw_square(10 + BS * i, 10 + BS * j, board[j][i]);
        }

        //draw counters and instructions
        text("Lines:",       0, 10 + BS * BWIDTH + 10, 10 + BS * 5 + 10 +   0);
        text("%d"    ,   lines, 10 + BS * BWIDTH + 10, 10 + BS * 5 + 10 +  30);
        text("Score:",       0, 10 + BS * BWIDTH + 10, 10 + BS * 5 + 10 +  70);
        text("%d"    ,   score, 10 + BS * BWIDTH + 10, 10 + BS * 5 + 10 + 100);
        text("Best:" ,       0, 10 + BS * BWIDTH + 10, 10 + BS * 5 + 10 + 140);
        text("%d"    ,    best, 10 + BS * BWIDTH + 10, 10 + BS * 5 + 10 + 170);
        text("Controls:",    0, 10 + BS * BWIDTH + 10, 10 + BS * 5 + 10 + 370);
        text("arrows, z, x", 0, 10 + BS * BWIDTH + 10, 10 + BS * 5 + 10 + 400);

        SDL_RenderPresent(renderer);
}

//draw a single square/piece of a shape
void draw_square(int x, int y, int shape)
{
        set_color_from_shape(shape, -25);
        SDL_RenderDrawRect(renderer, &(SDL_Rect){x, y, BS, BS});
        set_color_from_shape(shape, 0);
        SDL_RenderFillRect(renderer, &(SDL_Rect){1 + x, 1 + y, BS - 2, BS - 2});
}

//set the current draw color to the color assoc. with a shape
void set_color_from_shape(int shape, int shade)
{
        int r = MAX(colors[shape*3 + 0] + shade, 0);
        int g = MAX(colors[shape*3 + 1] + shade, 0);
        int b = MAX(colors[shape*3 + 2] + shade, 0);
        SDL_SetRenderDrawColor(renderer, r, g, b, 255);
}

//render a centered line of text optionally with a %d value in it
void text(char *fstr, int value, int x, int y)
{
        if(!font) return;
        int w, h;
        char msg[80];
        snprintf(msg, 80, fstr, value);
        TTF_SizeText(font, msg, &w, &h);
        SDL_Surface *msgsurf = TTF_RenderText_Blended(font, msg, (SDL_Color){80, 90, 85});
        SDL_Texture *msgtex = SDL_CreateTextureFromSurface(renderer, msgsurf);
        SDL_Rect fromrec = {0, 0, msgsurf->w, msgsurf->h};
        SDL_Rect torec = {x, y, msgsurf->w, msgsurf->h};
        SDL_RenderCopy(renderer, msgtex, &fromrec, &torec);
        SDL_DestroyTexture(msgtex);
        SDL_FreeSurface(msgsurf);
}

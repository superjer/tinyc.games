#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <SDL.h>
#include <SDL_ttf.h>

#define SHAPES 7
#define BWIDTH 10  // board width, height
#define BHEIGHT 20
#define BS 30
#define EXTRA_DEAD 50

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
        ".... .... .... .... .... .... .O.. .... "
        ;

unsigned char colors[][9*3] = {
        {
                25,   40,  35, // "black"
                242, 245, 237, // J
                255, 194,   0, // L
                15,  127, 127, // square
                255,  91,   0, // Z
                184,   0,  40, // S
                74,  192, 242, // T
                132,   0,  46, // line
                255, 255, 255, // shine
        }, {
                  0,   0,   0, // "black"
                221,  30,  47, // J
                235, 176,  53, // L
                  6, 162, 203, // square
                 33, 133,  89, // Z
                162,  89,  33, // S
                 33,   6, 177, // T
                208, 198, 177, // line
                255, 255, 200, // shine
        }
};

int palette = 0;

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

//enginey protos
void setup();
void key_down();
void update_stuff();
void draw_stuff();
void draw_square(int x, int y, int shape);
void draw_board_square(int bx, int by, int shape);
void set_shape_color(int shape, int shade);
void text(char *fstr, int value, int x, int y);

//gamey protos
void new_game();
void new_piece();
void move(int dx, int dy);
int is_solid_part(int shape, int rot, int i, int j);
int collide(int x, int y, int rot);
void bake();
void check_lines();
void check_dead();
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
                case SDLK_SPACE:              palette = palette ? 0 : 1; break;
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
                int x = (dead_time-EXTRA_DEAD) % BWIDTH;
                int y = (dead_time-EXTRA_DEAD) / BWIDTH;

                if(y >= 0 && y < BHEIGHT && x >= 0 && x < BWIDTH)
                        board[y][x] = rand() % 7 + 1;

                dead_time--;

                if(dead_time == 0)
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

//out of space?
void game_over()
{
        dead_time = BWIDTH * BHEIGHT + EXTRA_DEAD;
        next_shape = 0;
}

//randomly pick a new falling piece
void new_piece()
{
        falling_shape = next_shape;
        next_shape = rand() % SHAPES + 1;
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

                if(board[world_j][world_i])
                        game_over();

                board[world_j][world_i] = falling_shape;
        }

        check_lines();
        falling_shape = 0;
}

//check if there are any completed horizontal lines
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

//make a completed line "shine" and mark it to be removed
void shine_line(int y)
{
        shine_time = 50;
        killy_lines[y] = 1;
        for(int i = 0; i < BWIDTH; i++)
                board[y][i] = 8;
}

//remove lines that were marked to be removed by shine_line()
void kill_lines()
{
        int new_lines = 0;
        for(int y = 0; y < BHEIGHT; y++)
        {
                if(!killy_lines[y])
                        continue;

                new_lines++;

                killy_lines[y] = 0;
                memset(board[0], 0, sizeof *board);

                for(int j = y; j > 0; j--)
                {
                        for(int i = 0; i < BWIDTH; i++)
                                board[j][i] = board[j-1][i];
                }
        }

        lines += new_lines;

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
        while(!collide(falling_x, falling_y + 1, falling_rot))
        {
                falling_y++;
                idle_time = 0;
        }

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
        SDL_SetRenderDrawColor(renderer, 25, 40, 35, 255);
        SDL_RenderClear(renderer);

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);

        //draw board background
        SDL_RenderFillRect(renderer, &(SDL_Rect){10, 10, BS * BWIDTH, BS * BHEIGHT});

        //draw next box background
        SDL_RenderFillRect(renderer, &(SDL_Rect){10 + BS * BWIDTH + 10, 10, BS * 5, BS * 5});

        //draw falling piece
        for(int i = 0; i < 4; i++) for(int j = 0; j < 4; j++)
        {
                int world_i = i + falling_x;
                int world_j = j + falling_y;

                if(!is_solid_part(falling_shape, falling_rot, i, j))
                        continue;

                int shadow_j = world_j + 1;
                if(shadow_j < 0) shadow_j = 0;

                SDL_SetRenderDrawColor(renderer, 8, 13, 12, 255);
                SDL_RenderFillRect(renderer, &(SDL_Rect){
                        10 + BS * world_i,
                        10 + BS * shadow_j,
                        BS,
                        BS * (BHEIGHT - shadow_j)
                });

                if(world_j < 0)
                        continue;

                draw_board_square(world_i, world_j, falling_shape);
        }

        //draw next piece 1) compute center of piece
        int xmin = 3, xmax = 0, ymin = 3, ymax = 0;
        for(int i = 0; i < 4; i++) for(int j = 0; j < 4; j++)
        {
                if(is_solid_part(next_shape, 0, i, j))
                {
                        if(i < xmin) xmin = i;
                        if(i > xmax) xmax = i;
                        if(j < ymin) ymin = j;
                        if(j > ymax) ymax = j;
                }
        }
        int xdiff = (3 - xmax) - xmin;
        int ydiff = (3 - ymax) - ymin;

        //draw next piece 2) actually draw
        for(int i = 0; i < 4; i++) for(int j = 0; j < 4; j++)
        {
                if(!is_solid_part(next_shape, 0, i, j))
                        continue;

                draw_square(
                        10 + BS * BWIDTH + 10 + BS/2 + BS * i + (BS/2) * xdiff,
                        10 + BS/2 + BS * j + (BS/2) * ydiff,
                        next_shape
                );
        }

        //draw board pieces
        for(int i = 0; i < BWIDTH; i++) for(int j = 0; j < BHEIGHT; j++)
        {
                if(!board[j][i])
                        continue;

                draw_board_square(i, j, board[j][i]);
        }

        //draw text
        text("Lines:", lines, 10 + BS * BWIDTH + 10, 10 + BS * 5 + 10 +   0);
        text("%d"    , lines, 10 + BS * BWIDTH + 10, 10 + BS * 5 + 10 +  30);
        text("Score:", score, 10 + BS * BWIDTH + 10, 10 + BS * 5 + 10 +  70);
        text("%d"    , score, 10 + BS * BWIDTH + 10, 10 + BS * 5 + 10 + 100);
        text("Best:" , best , 10 + BS * BWIDTH + 10, 10 + BS * 5 + 10 + 140);
        text("%d"    , best , 10 + BS * BWIDTH + 10, 10 + BS * 5 + 10 + 170);

        SDL_RenderPresent(renderer);
}

//draw a single square/piece of a shape
void draw_square(int x, int y, int shape)
{
        set_shape_color(shape, -25);
        SDL_RenderDrawRect(renderer, &(SDL_Rect){x, y, BS, BS});

        set_shape_color(shape, 0);
        SDL_RenderFillRect(renderer, &(SDL_Rect){1 + x, 1 + y, BS - 2, BS - 2});
}

void draw_board_square(int bx, int by, int shape)
{
        draw_square(10 + BS * bx, 10 + BS * by, shape);
}

//set the current draw color to the color assoc. with a shape
void set_shape_color(int shape, int shade)
{
        int r = colors[palette][shape*3 + 0] + shade;
        int g = colors[palette][shape*3 + 1] + shade;
        int b = colors[palette][shape*3 + 2] + shade;

        if(r > 255) r = 255;
        if(r <   0) r =   0;
        if(g > 255) g = 255;
        if(g <   0) g =   0;
        if(b > 255) b = 255;
        if(b <   0) b =   0;

        SDL_SetRenderDrawColor(renderer, r, g, b, 255);
}

void text(char *fstr, int value, int x, int y)
{
        if(!font) return;
        int w, h;
        char msg[80];
        snprintf(msg, 80, fstr, value);
        TTF_SizeText(font, msg, &w, &h);
        SDL_Surface *msgsurf = TTF_RenderText_Blended(font, msg, (SDL_Color){0, 0, 0});
        SDL_Texture *msgtex = SDL_CreateTextureFromSurface(renderer, msgsurf);
        SDL_Rect fromrec = {0, 0, msgsurf->w, msgsurf->h};
        SDL_Rect torec = {x, y, msgsurf->w, msgsurf->h};
        SDL_RenderCopy(renderer, msgtex, &fromrec, &torec);
        SDL_DestroyTexture(msgtex);
        SDL_FreeSurface(msgsurf);
}

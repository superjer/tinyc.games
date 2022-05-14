// Tet -- http://tinyc.games -- (c) 2022 Jer Wilson
//
// Tet is an extremely small implementation of Tetris.

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#define SDL_DISABLE_IMMINTRIN_H
#include <SDL.h>
#include <SDL_ttf.h>

#define BWIDTH 10  // board width, height
#define BHEIGHT 25
#define VHEIGHT 20 // visible height
#define BAG_SZ 8   // bag size
#define BS 30      // size of one block
#define BS2 (BS/2) // size of half a block
#define WBOX (5*BS)// width of a preview/hold box
#define WBOARD (10*BS)// width of board
#define HELD_BOX_X (10 + BS2)
#define PREVIEW_BOX_X (10 + WBOX + 10 + BS * BWIDTH + 10 + BS2)
#define MAX(a,b) ((a)>(b)?(a):(b))

char shapes[] =
        ".... O... ..O. .OO. .... OO.. .OO. .O.. "
        ".... OOO. OOO. .OO. OOOO .OO. OO.. OOO. "
        ".... .... .... .... .... .... .... .... "
        ".... .... .... .... .... .... .... .... "

        ".... .OO. .O.. .OO. ..O. ..O. .O.. .O.. "
        ".... .O.. .O.. .OO. ..O. .OO. .OO. .OO. "
        ".... .O.. .OO. .... ..O. .O.. ..O. .O.. "
        ".... .... .... .... ..O. .... .... .... "

        ".... .... .... .OO. .... .... .... .... "
        ".... OOO. OOO. .OO. .... OO.. .OO. OOO. "
        ".... ..O. O... .... OOOO .OO. OO.. .O.. "
        ".... .... .... .... .... .... .... .... "

        ".... .O.. OO.. .OO. .O.. .O.. O... .O.. "
        ".... .O.. .O.. .OO. .O.. OO.. OO.. OO.. "
        ".... OO.. .O.. .... .O.. O... .O.. .O.. "
        ".... .... .... .... .O.. .... .... .... ";


int center[] = { // helps center shapes in preview box
         0,0, 1,1, 1,1, 0,1, 0,0, 1,1, 1,1, 1,1,
};

unsigned char colors[] = {
        0,     0,   0, // unused
        242, 245, 237, // J
        255,  91,   0, // L
        255, 194,   0, // square
        74,  192, 242, // line
        184,   0,  40, // Z
        15,  127, 127, // S
        132,   0,  46, // T
        255, 255, 255, // shine color
         59,  66, 159, // violet blue square
        122, 199,  79, // mantis square
};

int kicks[] = {   // clockwise                            counterclockwise
        0,0,  -1, 0,  -1, 1,   0,-2,  -1,-2,     0,0,   1, 0,   1, 1,   0,-2,   1,-2, // rotation 0
        0,0,  -1, 0,  -1,-1,   0, 2,  -1, 2,     0,0,  -1, 0,  -1,-1,   0, 2,  -1, 2, // rotation 1
        0,0,   1, 0,   1, 1,   0,-2,   1,-2,     0,0,  -1, 0,  -1, 1,   0,-2,  -1,-2, // rotation 2
        0,0,   1, 0,   1,-1,   0, 2,   1, 2,     0,0,   1, 0,   1,-1,   0, 2,   1, 2, // rotation 3
                // line-clockwise                       line-counterclockwise
        0,0,   2, 0,  -1, 0,   2,-1,  -1, 2,     0,0,   1, 0,  -2, 0,   1, 2,  -2,-1, // rotation 0
        0,0,  -2, 0,   1, 0,  -2, 1,   1,-2,     0,0,   1, 0,  -2, 0,   1, 2,  -2,-1, // rotation 1
        0,0,  -1, 0,   2, 0,  -1,-2,   2, 1,     0,0,  -2, 0,   1, 0,  -2, 1,   1,-2, // rotation 2
        0,0,   2, 0,  -1, 0,   2,-1,  -1, 2,     0,0,  -1, 0,   2, 0,  -1,-2,   2, 1, // rotation 3
};

unsigned char board[BHEIGHT][BWIDTH];
unsigned int idmap[BHEIGHT][BWIDTH];
int killy_lines[BHEIGHT];

int left, right, down; // true if holding a direction
int move_cooldown;
int falling_x;
int falling_y;
int falling_shape;
int falling_rot;
int grounded;
int grounded_moves;
int next[3];
int held_shape;
int hold_count;
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
void joy_setup();
void key_down();
void key_up();
void joy_down();
void joy_up();
void joy_hat();
void update_stuff();
void draw_stuff();
void draw_square(int x, int y, int shape, int shade);
void set_color_from_shape(int shape, int shade);
void text(char *fstr, int value, int x, int y);
void new_game();
void new_piece();
void move(int dx, int dy, int gravity);
int is_solid_part(int shape, int rot, int i, int j);
int collide(int x, int y, int rot);
void bake();
void check_lines();
void shine_line(int y);
void kill_lines();
void hard();
void spin(int dir);
void hold();

//the entry point and main game loop
int main()
{
        setup();
        new_game();

        for (;;)
        {
                while (SDL_PollEvent(&event)) switch (event.type)
                {
                        case SDL_QUIT:             exit(0);
                        case SDL_KEYDOWN:          key_down();  break;
                        case SDL_KEYUP:            key_up();    break;
                        case SDL_JOYBUTTONDOWN:    joy_down();  break;
                        case SDL_JOYBUTTONUP:      joy_up();    break;
                        case SDL_JOYHATMOTION:     joy_hat();   break;
                        case SDL_JOYDEVICEADDED:
                        case SDL_JOYDEVICEREMOVED: joy_setup(); break;
                }

                update_stuff();
                draw_stuff();
                SDL_Delay(10);
                idle_time++;
        }
}

void joy_setup()
{
        SDL_JoystickEventState(SDL_ENABLE);
        printf("%i joysticks were found.\n\n", SDL_NumJoysticks() );
        printf("The names of the joysticks are:\n");
        for (int i = 0; i < SDL_NumJoysticks(); i++)
        {
                SDL_Joystick *joy = SDL_JoystickOpen(i);
                int num_buttons = SDL_JoystickNumButtons(joy);
                int num_axes = SDL_JoystickNumAxes(joy);
                printf("    %s - %d buttons, %d axes\n", SDL_JoystickName(joy), num_buttons, num_axes);

                if (num_buttons == 0)
                        SDL_JoystickClose(joy);
        }
}

//initial setup to get the window and rendering going
void setup()
{
        srand(time(NULL));
        SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK);

        SDL_Window *win = SDL_CreateWindow("Tet",
                        SDL_WINDOWPOS_UNDEFINED,
                        SDL_WINDOWPOS_UNDEFINED,
                        10 + WBOX + 10 + WBOARD + 10 + WBOX + 10,
                        10 + VHEIGHT * BS + 10,
                        SDL_WINDOW_SHOWN);

        renderer = SDL_CreateRenderer(win, -1, 0);
        if (!renderer)
        {
                fprintf(stderr, "Could not create SDL renderer for some reason\n");
                exit(-1);
        }

        joy_setup();

        TTF_Init();
        font = TTF_OpenFont("res/LiberationSans-Regular.ttf", 28);
}

//handle a key press from the player
void key_down()
{
        if (event.key.repeat) return;

        if (falling_shape) switch (event.key.keysym.sym)
        {
                case SDLK_a:      case SDLK_LEFT:   left = 1;  move_cooldown = 0; break;
                case SDLK_d:      case SDLK_RIGHT:  right = 1; move_cooldown = 0; break;
                case SDLK_s:      case SDLK_DOWN:   down = 1;  move_cooldown = 0; break;
                case SDLK_w:      case SDLK_UP:     hard();    break;
                case SDLK_COMMA:  case SDLK_z:      spin(3);   break;
                case SDLK_PERIOD: case SDLK_x:      spin(1);   break;
                case SDLK_TAB:    case SDLK_LSHIFT: hold();    break;
        }

        if (event.key.keysym.sym == SDLK_j) // reset joystick subsystem
                joy_setup();
}

void key_up()
{
        switch (event.key.keysym.sym)
        {
                case SDLK_a:      case SDLK_LEFT:   left = 0;  break;
                case SDLK_d:      case SDLK_RIGHT:  right = 0; break;
                case SDLK_s:      case SDLK_DOWN:   down = 0;  break;
        }
}

void joy_down()
{
        printf("Joy %d down button=%d\n", event.jbutton.which, event.jbutton.button);
        if (!falling_shape)
                ;
        else if (event.jbutton.button <= 3)
                spin(event.jbutton.button % 2 ? 1 : 3);
        else if (event.jbutton.button <= 5)
                hold();
}

void joy_up()
{
}

void joy_hat()
{
        printf("Hat %d value=%d\n", event.jhat.which, event.jhat.value);
        down = left = right = 0;
        move_cooldown = 0;
        if (!falling_shape) ;
        else if (event.jhat.value & SDL_HAT_DOWN)  down = 1;
        else if (event.jhat.value & SDL_HAT_LEFT)  left = 1;
        else if (event.jhat.value & SDL_HAT_RIGHT) right = 1;
        else if (event.jhat.value & SDL_HAT_UP)    hard();
}

//update everything
void update_stuff()
{
        if (!falling_shape && !shine_time && !dead_time)
        {
                new_piece();
        }

        grounded = collide(falling_x, falling_y + 1, falling_rot);

        if (move_cooldown) move_cooldown--;

        if (move_cooldown < 2)
        {
                if (left)  move(-1, 0, 0);
                if (right) move( 1, 0, 0);
                if (down)  move( 0, 1, 0);
        }

        if (shine_time > 0)
        {
                shine_time--;
                if (shine_time == 0)
                        kill_lines();
        }

        if (dead_time > 0)
        {
                int x = dead_time % BWIDTH;
                int y = dead_time / BWIDTH;

                if (y >= 0 && y < VHEIGHT && x >= 0 && x < BWIDTH)
                        board[y + 5][x] = rand() % 7 + 1;

                if (--dead_time == 0)
                        new_game();
        }

        if (idle_time >= 50)
        {
                move(0, 1, 1);
                idle_time = 0;
        }
}

//reset score and pick one extra random piece
void new_game()
{
        memset(board, 0, sizeof board);
        memset(idmap, 0, sizeof idmap);
        do new_piece(); while (next[0] == 0 || next[0] > 4);
        if (best < score) best = score;
        score = 0;
        lines = 0;
        falling_shape = 0;
        held_shape = 0;
}

//create a new piece bag with 7 or 8 pieces
int new_bag(int *bag)
{
        int wildcard_idx = 0;

        for (int i = 0; i < BAG_SZ; i++)
        {
                nope:
                bag[i] = rand() % BAG_SZ;
                for (int j = 0; j < i; j++)
                        if (bag[j] == bag[i])
                                goto nope;

                if (bag[i] == 0) wildcard_idx = i;
        }

        // skip wildcard if in position zero
        if (wildcard_idx == 0)
                return 1;

        // set wildcard to some valid piece
        bag[wildcard_idx] = rand() % 7 + 1;
        return 0;
}

// set the current piece to the top, middle to start falling
void reset_fall()
{
        idle_time = 0;
        grounded_moves = 0;
        falling_x = 3;
        falling_y = 2;
        falling_rot = 0;
}

//pick a new next piece from the bag, and put the old one in play
void new_piece()
{
        static int bag[BAG_SZ] = {0};
        static int idx = BAG_SZ;

        if (idx >= BAG_SZ) idx = new_bag(bag);

        falling_shape = next[0];
        next[0] = next[1];
        next[1] = next[2];
        next[2] = bag[idx++];
        hold_count = 0;
        reset_fall();
}

//move the falling piece left, right, or down
void move(int dx, int dy, int gravity)
{
        if (!gravity)
                move_cooldown = move_cooldown ? 5 : 25;

        if (!collide(falling_x + dx, falling_y + dy, falling_rot))
        {
                falling_x += dx;
                falling_y += dy;

                // reset idle time if you voluntarily move DOWN
                if (dy) idle_time = 0;

                // reset idle time if piece is grounded, limit grounded moves though
                if (grounded && grounded_moves < 15)
                {
                        idle_time = 0;
                        grounded_moves++;
                }
        }
        else if (dy && gravity)
        {
                bake();
        }
}

//check if a sub-part of the falling shape is solid at a particular rotation
int is_solid_part(int shape, int rot, int i, int j)
{
        int base = shape*5 + rot*5*8*4;
        return shapes[base + j*5*8 + i] == 'O';
}

//check if the current piece would collide at a certain position and rotation
int collide(int x, int y, int rot)
{
        for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++)
        {
                int world_i = i + x;
                int world_j = j + y;

                if (!is_solid_part(falling_shape, rot, i, j))
                        continue;

                if (world_i < 0 || world_i >= BWIDTH || world_j >= BHEIGHT)
                        return 1;

                if (board[world_j][world_i])
                        return 1;
        }
        return 0;
}

void check_square_at(int x, int y)
{
        int found_ids[4] = {0};
        int first_found_shape = 0;
        int color = 10; // gold

        for (int i = x; i < x + 4; i++) for (int j = y; j < y + 4; j++)
        {
                if (idmap[j][i] == 0) return; // no square here

                if (first_found_shape && board[j][i] != first_found_shape)
                        color = 9; // silver

                first_found_shape = board[j][i];

                for (int k = 0; k < 5; k++)
                {
                        if (k == 4) return; // too many ids

                        if (found_ids[k] == 0)
                        {
                                found_ids[k] = idmap[j][i];
                                break;
                        }

                        if (found_ids[k] == idmap[j][i])
                                break;
                }
        }

        for (int i = x; i < x + 4; i++) for (int j = y; j < y + 4; j++)
        {
                board[j][i] = color;
                idmap[j][i] = 0;
        }
}

//bake the falling piece into the background/board
void bake()
{
        static int bake_id = 0;
        bake_id++;

        for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++)
        {
                int world_i = i + falling_x;
                int world_j = j + falling_y;

                if (!is_solid_part(falling_shape, falling_rot, i, j))
                        continue;

                if (world_i < 0 || world_i >= BWIDTH || world_j < 0 || world_j >= BHEIGHT)
                        continue;

                if (board[world_j][world_i]) // already a block here? game over
                        dead_time = BWIDTH * VHEIGHT;

                board[world_j][world_i] = falling_shape;
                idmap[world_j][world_i] = bake_id;
        }

        // check for squares
        for (int j = BHEIGHT - 1; j >= 3; j--)
                for (int i = 0; i < BWIDTH - 3; i++)
                        check_square_at(i, j);

        // check if there are any completed horizontal lines
        for (int j = BHEIGHT - 1; j >= 0; j--)
                for (int i = 0; i < BWIDTH && board[j][i]; i++)
                        if (i == BWIDTH - 1) shine_line(j);

        falling_shape = 0;
}

//make a completed line "shine" and mark it to be removed
void shine_line(int y)
{
        shine_time = 20;
        killy_lines[y] = 1;
        for (int i = 0; i < BWIDTH; i++)
                board[y][i] = 8; //shiny!
}

//remove lines that were marked to be removed by shine_line()
void kill_lines()
{
        int new_lines = 0;
        for (int y = 0; y < BHEIGHT; y++)
        {
                if (!killy_lines[y])
                        continue;

                lines++;
                new_lines++;
                killy_lines[y] = 0;

                for (int j = y; j > 0; j--) for (int i = 0; i < BWIDTH; i++)
                {
                        board[j][i] = board[j-1][i];
                        idmap[j][i] = idmap[j-1][i];
                }

                memset(board[0], 0, sizeof *board);
                memset(idmap[0], 0, sizeof *idmap);
        }

        switch (new_lines)
        {
                case 1: score += 100;  break;
                case 2: score += 250;  break;
                case 3: score += 500;  break;
                case 4: score += 1000; break;
        }
}

//move the falling piece as far down as it will go
void hard()
{
        while (!collide(falling_x, falling_y + 1, falling_rot))
                falling_y++;
        idle_time = 50;
}

//spin the falling piece left or right, if possible
void spin(int dir)
{
        int new_rot = (falling_rot + dir) % 4;
        int k = new_rot * 20 + (dir == 1 ? 0 : 10) + (falling_shape == 4 ? 80 : 0);

        for (int i = 0; i < 5; i++)
        {
                int kx = kicks[k++];
                int ky = kicks[k++];
                if (!collide(falling_x + kx, falling_y + ky, new_rot))
                {
                        falling_rot = new_rot;
                        falling_x += kx;
                        falling_y += ky;
                        if (grounded && grounded_moves < 15)
                        {
                                idle_time = 0;
                                grounded_moves++;
                        }
                        return;
                }
        }
}

void hold()
{
        if (hold_count++) return;
        held_shape ^= falling_shape;
        falling_shape ^= held_shape;
        held_shape ^= falling_shape;
        reset_fall();
}

//draw everything in the game on the screen
void draw_stuff()
{
        // draw background, black boxes
        SDL_SetRenderDrawColor(renderer, 25, 40, 35, 255);
        SDL_RenderClear(renderer);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderFillRect(renderer, &(SDL_Rect){10, 10, WBOX, WBOX});
        SDL_RenderFillRect(renderer, &(SDL_Rect){10 + WBOX + 10, 10, WBOARD, BS * VHEIGHT});
        //SDL_RenderFillRect(renderer, &(SDL_Rect){10 + WBOX + 10 + WBOARD + 10, 10, WBOX, WBOX});

        //find ghost piece position
        int ghost_y = falling_y;
        while (ghost_y < BHEIGHT && !collide(falling_x, ghost_y + 1, falling_rot))
                ghost_y++;

        //draw shadow
        for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++)
        {
                if (!is_solid_part(falling_shape, falling_rot, i, j))
                        continue;

                SDL_SetRenderDrawColor(renderer, 8, 13, 12, 255);
                SDL_RenderFillRect(renderer, &(SDL_Rect){
                        10 + WBOX + 10 + BS * (i + falling_x),
                        10 + BS * MAX(0, j + falling_y - 5),
                        BS,
                        BS * (ghost_y - falling_y)
                });
        }

        //draw falling piece & ghost
        for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++)
        {
                int world_i = i + falling_x;
                int world_j = j + falling_y - 5;
                int ghost_j = j + ghost_y - 5;

                if (!is_solid_part(falling_shape, falling_rot, i, j))
                        continue;

                if (ghost_j >= 0)
                        draw_square(10 + WBOX + 10 + BS * world_i, 10 + BS * ghost_j, falling_shape, 1);

                if (world_j >= 0)
                        draw_square(10 + WBOX + 10 + BS * world_i, 10 + BS * world_j, falling_shape, 0);
        }

        //draw next piece, centered in the preview box
        for (int n = 0; n < 3; n++) for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++)
        {
                if (!is_solid_part(next[n], 0, i, j)) continue;
                draw_square(
                        PREVIEW_BOX_X + BS * i + BS2 * center[2 * next[n]],
                        10 + BS + BS * 4 * n + BS * j + BS2 * center[2 * next[n] + 1],
                        next[n],
                        0
                );
        }

        //draw held piece, centered in the held box
        for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++)
        {
                if (!is_solid_part(held_shape, 0, i, j)) continue;
                draw_square(
                        HELD_BOX_X + BS * i + BS2 * center[2*held_shape],
                        10 + BS +  BS * j + BS2 * center[2*held_shape + 1],
                        held_shape,
                        0
                );
        }

        //draw board pieces
        for (int i = 0; i < BWIDTH; i++) for (int j = 0; j < VHEIGHT; j++)
        {
                if (board[j+5][i])
                        draw_square(10 + WBOX + 10 + BS * i, 10 + BS * j, board[j+5][i], 0);
        }

        //draw counters and instructions
        text("Lines:"   ,     0, 10, 10 + WBOX + 30 +   0);
        text("%d"       , lines, 10, 10 + WBOX + 30 +  30);
        text("Score:"   ,     0, 10, 10 + WBOX + 30 +  70);
        text("%d"       , score, 10, 10 + WBOX + 30 + 100);
        text("Best:"    ,     0, 10, 10 + WBOX + 30 + 140);
        text("%d"       ,  best, 10, 10 + WBOX + 30 + 170);
        text("Controls:",     0, 10, 10 + WBOX + 30 + 310);
        text("arrows,"  ,     0, 10, 10 + WBOX + 30 + 340);
        text("z, x, tab",     0, 10, 10 + WBOX + 30 + 370);

        SDL_RenderPresent(renderer);
}

//draw a single square/piece of a shape
void draw_square(int x, int y, int shape, int outline)
{
        set_color_from_shape(shape, -25);
        SDL_RenderDrawRect(renderer, &(SDL_Rect){x, y, BS, BS});
        set_color_from_shape(shape, outline ? -255 : 0);
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
        if (!font) return;
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

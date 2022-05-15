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
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define SWAP(a,b) {int c = (a); (a) = (b); (b) = c;}

/*
 * @ 1000000    - none
 * A 1000001    - up
 * B 1000010    - right
 * C 1000011    - right up
 * D 1000100    - down
 * E 1000101    - down up
 * F 1000110    - down right
 * G 1000111    - down right up
 * H 1001000    - left
 * I 1001001    - left up
 * J 1001010    - left right
 * K 1001011    - left right up
 * L 1001100    - left down
 * M 1001101    - left down up
 * N 1001110    - left down right
 * O 1001111    - left down right up
 *
 * S 1010011    - right up with vertical band stretch right
 * V 1010110    - down right with vertical band stretch right
 *
 * i 1101001    - left up with vertical band stretch left
 * l 1101100    - left down with vertical band stretch left
 */

char shapes[] =
        ".... D... ..D. .Vl. .... BL.. .FH. .D.. "
        ".... CJH. BJI. .Si. BJJH .CH. BI.. BKH. "
        ".... .... .... .... .... .... .... .... "
        ".... .... .... .... .... .... .... .... "

        ".... .FH. .D.. .Vl. ..D. ..D. .D.. .D.. "
        ".... .E.. .E.. .Si. ..E. .FI. .CL. .GH. "
        ".... .A.. .CH. .... ..E. .A.. ..A. .A.. "
        ".... .... .... .... ..A. .... .... .... "

        ".... .... .... .Vl. .... .... .... .... "
        ".... BJL. FJH. .Si. .... BL.. .FH. BNH. "
        ".... ..A. A... .... BJJH .CH. BI.. .A.. "
        ".... .... .... .... .... .... .... .... "

        ".... .D.. BL.. .Vl. .D.. .D.. D... .D.. "
        ".... .E.. .E.. .Si. .E.. FI.. CL.. BM.. "
        ".... BI.. .A.. .... .E.. A... .A.. .A.. "
        ".... .... .... .... .A.. .... .... .... ";


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

struct {
        unsigned char color;
        unsigned char part;
        unsigned int id;
} board[BHEIGHT][BWIDTH];

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
int tick;
int idle_time;
int shine_time;
int dead_time;
int square_time, square_x, square_y; // data for current big square

// window size and resulting sizes & positions of things
int win_x = 1000;
int win_y = 750;
int board_x, board_y, board_w;
int preview_x, preview_y;
int hold_x, hold_y;
int box_w;
int bs, bs2; // individual block size, and in half


SDL_Event event;
SDL_Renderer *renderer;
TTF_Font *font;

//prototypes
void setup();
void joy_setup();
void win_event();
void resize(int x, int y);
void key_down();
void key_up();
void joy_down();
void joy_up();
void update_stuff();
void draw_stuff();
void draw_square(int x, int y, int shape, int shade, int part);
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
                        case SDL_QUIT:                 exit(0);
                        case SDL_KEYDOWN:              key_down();  break;
                        case SDL_KEYUP:                key_up();    break;
                        case SDL_CONTROLLERBUTTONDOWN: joy_down();  break;
                        case SDL_CONTROLLERBUTTONUP:   joy_up();    break;
                        case SDL_JOYDEVICEADDED:
                        case SDL_JOYDEVICEREMOVED:     joy_setup(); break;
                        case SDL_WINDOWEVENT:          win_event(); break;
                }

                update_stuff();
                draw_stuff();
                SDL_Delay(10);
                tick++;
                idle_time++;
        }
}

void joy_setup()
{
        for (int i = 0; i < SDL_NumJoysticks(); i++)
        {
                if (!SDL_IsGameController(i))
                {
                        printf("Controller NOT supported: %s\n", SDL_JoystickNameForIndex(i));
                        printf("Google for SDL_GAMECONTROLLERCONFIG to fix this\n");
                        continue;
                }
                SDL_GameController *cont = SDL_GameControllerOpen(i);
                printf("Controller added: %s %p\n", SDL_GameControllerNameForIndex(i), cont);
        }
        SDL_GameControllerEventState(SDL_ENABLE);
}

void win_event()
{
        if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
                resize(event.window.data1, event.window.data2);
}

// recalculate sizes and positions on resize
void resize(int x, int y)
{
        fprintf(stderr, "Window resizing to %dx%d\n", x, y);
        win_x = x;
        win_y = y;
        int smaller = MIN(win_x, win_y * 100 / 160);
        printf("using base size of %d\n", smaller);
        bs = smaller / 30 * 2;
        bs2 = bs / 2;
        board_x = (x / 2) - bs2 * BWIDTH;
        board_y = (y / 2) - bs2 * VHEIGHT;
        board_w = bs * 10;
        box_w = bs * 5;
        hold_x = board_x - box_w - bs2;
        hold_y = board_y;
        preview_x = board_x + board_w + bs2;
        preview_y = board_y;
        TTF_CloseFont(font);
        font = TTF_OpenFont("res/LiberationSans-Regular.ttf", bs);
}

//initial setup to get the window and rendering going
void setup()
{
        srand(time(NULL));
        SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER);

        SDL_Window *win = SDL_CreateWindow("Tet",
                        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                        win_x, win_y,
                        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);

        renderer = SDL_CreateRenderer(win, -1, 0);
        if (!renderer)
        {
                fprintf(stderr, "Could not create SDL renderer for some reason\n");
                exit(-1);
        }

        resize(win_x, win_y);
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
        if (falling_shape) switch(event.cbutton.button)
        {
                case SDL_CONTROLLER_BUTTON_A:            spin(1); break;
                case SDL_CONTROLLER_BUTTON_B:            spin(3); break;
                case SDL_CONTROLLER_BUTTON_DPAD_UP:      hard();  break;
                case SDL_CONTROLLER_BUTTON_DPAD_DOWN:    down  = 1; move_cooldown = 0; break;
                case SDL_CONTROLLER_BUTTON_DPAD_LEFT:    left  = 1; move_cooldown = 0; break;
                case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:   right = 1; move_cooldown = 0; break;
                case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: hold();  break;
        }
}

void joy_up()
{
        switch(event.cbutton.button)
        {
                case SDL_CONTROLLER_BUTTON_DPAD_DOWN:  down  = 0; break;
                case SDL_CONTROLLER_BUTTON_DPAD_LEFT:  left  = 0; break;
                case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: right = 0; break;
        }
}

void fuse_square()
{
        for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++)
        {
                if (i != 0) board[square_y + j][square_x + i].part |= 'H' | 32 ; // fuse left
                if (j != 0) board[square_y + j][square_x + i].part |= 'A';       // fuse up
                if (i != 3) board[square_y + j][square_x + i].part |= 'B' | 16;  // fuse right
                if (j != 3) board[square_y + j][square_x + i].part |= 'D';       // fuse down
        }
}

//update everything
void update_stuff()
{
        if (square_time > 0)
        {
                if (--square_time == 0)
                        fuse_square();
                return;
        }

        if (!falling_shape && !shine_time && !dead_time)
                new_piece();

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

                if (y >= 0 && y < BHEIGHT && x >= 0 && x < BWIDTH)
                {
                        board[y + 0][x].color = rand() % 7 + 1;
                        board[y + 0][x].part = '@';
                }

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
        do new_piece(); while (next[0] == 0 || next[0] > 4); // get a nice starting piece
        if (best < score) best = score;
        score = 0;
        lines = 0;
        falling_shape = 0;
        held_shape = 0;
        hold_count = 0;
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
        reset_fall();
}

//move the falling piece left, right, or down
void move(int dx, int dy, int gravity)
{
        if (!gravity)
                move_cooldown = move_cooldown ? 5 : 15;

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
        int part = shapes[base + j*5*8 + i];
        return part == '.' ? 0 : part;
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

                if (board[world_j][world_i].color)
                        return 1;
        }
        return 0;
}

void check_square_at(int x, int y)
{
        int found_ids[4] = {0};
        int first_found_color = 0;
        int color = 10; // gold

        for (int i = x; i < x + 4; i++) for (int j = y; j < y + 4; j++)
        {
                if (board[j][i].id == 0) return; // no square forming here

                if (first_found_color && board[j][i].color != first_found_color)
                        color = 9; // silver

                first_found_color = board[j][i].color;

                for (int k = 0; k < 5; k++)
                {
                        if (k == 4) return; // too many ids

                        if (found_ids[k] == 0)
                        {
                                found_ids[k] = board[j][i].id;
                                break;
                        }

                        if (found_ids[k] == board[j][i].id)
                                break;
                }
        }

        for (int i = x; i < x + 4; i++) for (int j = y; j < y + 4; j++)
        {
                board[j][i].color = color;
                board[j][i].id = 0;
        }
        square_time = 40;
        square_x = x;
        square_y = y;
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
                int part = is_solid_part(falling_shape, falling_rot, i, j);

                if (!part || world_i < 0 || world_i >= BWIDTH || world_j < 0 || world_j >= BHEIGHT)
                        continue;

                if (board[world_j][world_i].color) // already a block here? game over
                        dead_time = BWIDTH * BHEIGHT - 1;

                board[world_j][world_i].color = falling_shape;
                board[world_j][world_i].part = part;
                board[world_j][world_i].id = bake_id;
        }

        // check for squares
        for (int j = BHEIGHT - 1; j >= 3; j--)
                for (int i = 0; i < BWIDTH - 3; i++)
                        check_square_at(i, j);

        // check if there are any completed horizontal lines
        for (int j = BHEIGHT - 1; j >= 0; j--)
                for (int i = 0; i < BWIDTH && board[j][i].color; i++)
                        if (i == BWIDTH - 1) shine_line(j);

        falling_shape = 0;
        hold_count = 0;
}

//make a completed line "shine" and mark it to be removed
void shine_line(int y)
{
        shine_time = 20;
        killy_lines[y] = 1;
        for (int i = 0; i < BWIDTH; i++)
                board[y][i].color = 8; //shiny!
}

//remove lines that were marked to be removed by shine_line()
void kill_lines()
{
        // clean up sliced pieces
        for (int y = 0; y < BHEIGHT; y++)
        {
                if (killy_lines[y]) continue;

                if (y > 0 && killy_lines[y - 1])
                        for (int x = 0; x < BWIDTH; x++)
                                board[y][x].part &= ~1;

                if (y < BHEIGHT - 1 && killy_lines[y + 1])
                        for (int x = 0; x < BWIDTH; x++)
                                board[y][x].part &= ~4;
        }

        int new_lines = 0;
        for (int y = 0; y < BHEIGHT; y++)
        {
                if (!killy_lines[y])
                        continue;

                lines++;
                new_lines++;
                killy_lines[y] = 0;

                for (int j = y; j > 0; j--) for (int i = 0; i < BWIDTH; i++)
                        board[j][i] = board[j-1][i];

                memset(board[0], 0, sizeof *board);
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
        SWAP(held_shape, falling_shape);
        reset_fall();
}

//draw everything in the game on the screen
void draw_stuff()
{
        // draw background, black boxes
        SDL_SetRenderDrawColor(renderer, 25, 40, 35, 255);
        SDL_RenderClear(renderer);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderFillRect(renderer, &(SDL_Rect){hold_x, hold_y, box_w, box_w});
        SDL_RenderFillRect(renderer, &(SDL_Rect){board_x, board_y, board_w, bs * VHEIGHT});

        //find ghost piece position
        int ghost_y = falling_y;
        while (ghost_y < BHEIGHT && !collide(falling_x, ghost_y + 1, falling_rot))
                ghost_y++;

        //draw shadow
        for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++)
        {
                int part = is_solid_part(falling_shape, falling_rot, i, j);
                if (!part || (part & 4)) // & 4 means connects down
                        continue;

                int top = MAX(0, j + falling_y - 5);
                SDL_SetRenderDrawColor(renderer, 8, 13, 12, 255);
                SDL_RenderFillRect(renderer, &(SDL_Rect){
                        board_x + bs * (i + falling_x),
                        board_y + bs * top,
                        bs,
                        bs * MAX(0, ghost_y + j - 4 - top)
                });
        }

        //draw falling piece & ghost
        for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++)
        {
                int world_i = i + falling_x;
                int world_j = j + falling_y - 5;
                int ghost_j = j + ghost_y - 5;
                int part = is_solid_part(falling_shape, falling_rot, i, j);

                if (ghost_j >= 0)
                        draw_square(board_x + bs * world_i, board_y + bs * ghost_j, falling_shape, 1, part);
                if (world_j >= 0)
                        draw_square(board_x + bs * world_i, board_y + bs * world_j, falling_shape, 0, part);
        }

        //draw next piece, centered in the preview box
        for (int n = 0; n < 3; n++) for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++)
                draw_square(
                        preview_x + bs * i + bs2 * center[2 * next[n]],
                        preview_y + bs + bs * 4 * n + bs * j + bs2 * center[2 * next[n] + 1],
                        next[n],
                        0,
                        is_solid_part(next[n], 0, i, j)
                );

        //draw held piece, centered in the held box
        for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++)
                draw_square(
                        hold_x + bs * i + bs2 * center[2*held_shape],
                        hold_y + bs + bs * j + bs2 * center[2*held_shape + 1],
                        held_shape,
                        0,
                        is_solid_part(held_shape, 0, i, j)
                );

        //draw board pieces
        for (int i = 0; i < BWIDTH; i++) for (int j = 0; j < VHEIGHT; j++)
                draw_square(board_x + bs * i, board_y + bs * j, board[j+5][i].color, 0, board[j+5][i].part);

        //draw counters and instructions
        int ln = bs * 110 / 100;
        text("Lines:"   ,     0, hold_x, hold_y + box_w + bs2 + ln *  0);
        text("%d"       , lines, hold_x, hold_y + box_w + bs2 + ln *  1);
        text("Score:"   ,     0, hold_x, hold_y + box_w + bs2 + ln *  3);
        text("%d"       , score, hold_x, hold_y + box_w + bs2 + ln *  4);
        text("Best:"    ,     0, hold_x, hold_y + box_w + bs2 + ln *  6);
        text("%d"       ,  best, hold_x, hold_y + box_w + bs2 + ln *  7);
        text("Controls:",     0, hold_x, hold_y + box_w + bs2 + ln * 10);
        text("arrows,"  ,     0, hold_x, hold_y + box_w + bs2 + ln * 11);
        text("z, x, tab",     0, hold_x, hold_y + box_w + bs2 + ln * 12);

        SDL_RenderPresent(renderer);
}

//draw a single square/piece of a shape
void draw_square(int x, int y, int shape, int outline, int part)
{
        if (!part) return;
        int bw = MAX(1, outline ? bs / 10 : bs / 6);
        set_color_from_shape(shape, -50);
        SDL_RenderFillRect(renderer, &(SDL_Rect){x, y, bs, bs});
        set_color_from_shape(shape, (outline ? -255 : 0) + (shape > 8 ? abs(tick % 100 - 50) : 0));
        SDL_RenderFillRect(renderer, &(SDL_Rect){ // horizontal band
                        x + (part & 8 ? 0 : bw),
                        y + bw,
                        bs - (part & 8 ? 0 : bw) - (part & 2 ? 0 : bw),
                        bs - bw - bw});
        SDL_RenderFillRect(renderer, &(SDL_Rect){ // vertical band
                        x + (part & 32 ? 0 : bw),
                        y + (part & 1 ? 0 : bw),
                        bs - (part & 32 ? 0 : bw) - (part & 16 ? 0 : bw),
                        bs - (part & 1 ? 0 : bw) - (part & 4 ? 0 : bw)});
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

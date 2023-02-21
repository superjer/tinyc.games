// Tet -- http://tinyc.games -- (c) 2022 Jer Wilson
//
// Tet is an extremely small implementation of Tetris.

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#define SDL_DISABLE_IMMINTRIN_H
#include <SDL.h>
#include <SDL_ttf.h>
#include "audio.c"

#define BWIDTH 10  // board width, height
#define BHEIGHT 25
#define VHEIGHT 20 // visible height
#define BAG_SZ 8   // bag size
#define NPLAY 4
#define CTDN_TICKS 80
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define SWAP(a,b) {int c = (a); (a) = (b); (b) = c;}

// collision test results
enum { NONE = 0, WALL, NORMAL };

/* Bits in each letter indicate which borders to draw in draw_square()
 * @ 1000000    - none
 * A 1000001    - up
 * B 1000010    - right
 * C 1000011    - right up
 * D 1000100    - down
 * E 1000101    - down up
 * F 1000110    - down right
 * G 1000111    - down right up
 * H 1001000    - left
 * ...
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

struct shadow { int x, w, y; } shadows[4][8] = { // pre-computed shadow positions for each piece
        {{0,0,0}, {0,3,1}, {0,3,1}, {1,2,0}, {0,4,1}, {0,3,1}, {0,3,1}, {0,3,1}},
        {{0,0,0}, {1,2,0}, {1,2,2}, {1,2,0}, {2,1,0}, {1,2,1}, {1,2,1}, {1,2,1}},
        {{0,0,0}, {0,3,1}, {0,3,1}, {1,2,0}, {0,4,2}, {0,3,2}, {0,3,2}, {0,3,1}},
        {{0,0,0}, {0,2,2}, {0,2,0}, {1,2,0}, {1,1,0}, {0,2,1}, {0,2,1}, {0,2,1}},
};

int center[] = { // helps center shapes in preview box
         0,0, 1,1, 1,1, 0,1, 0,0, 1,1, 1,1, 1,1,
};

unsigned char colors[] = {
          0,   0,   0, // unused
        242, 245, 237, // J
        255,  91,   0, // L
        255, 194,   0, // square
         74, 192, 242, // line
        184,   0,  40, // Z
         15, 127, 127, // S
        132,   0,  46, // T
        255, 255, 255, // shine color
};

int kicks[] = {   // clockwise                            counterclockwise
        0,0,  -1, 0,  -1, 1,   0,-2,  -1,-2,     0,0,   1, 0,   1, 1,   0,-2,   1,-2, // rot 0
        0,0,  -1, 0,  -1,-1,   0, 2,  -1, 2,     0,0,  -1, 0,  -1,-1,   0, 2,  -1, 2, // rot 1
        0,0,   1, 0,   1, 1,   0,-2,   1,-2,     0,0,  -1, 0,  -1, 1,   0,-2,  -1,-2, // rot 2
        0,0,   1, 0,   1,-1,   0, 2,   1, 2,     0,0,   1, 0,   1,-1,   0, 2,   1, 2, // rot 3
                // line-clockwise                       line-counterclockwise
        0,0,   2, 0,  -1, 0,   2,-1,  -1, 2,     0,0,   1, 0,  -2, 0,   1, 2,  -2,-1, // rot 0
        0,0,  -2, 0,   1, 0,  -2, 1,   1,-2,     0,0,   1, 0,  -2, 0,   1, 2,  -2,-1, // rot 1
        0,0,  -1, 0,   2, 0,  -1,-2,   2, 1,     0,0,  -2, 0,   1, 0,  -2, 1,   1,-2, // rot 2
        0,0,   2, 0,  -1, 0,   2,-1,  -1, 2,     0,0,  -1, 0,   2, 0,  -1,-2,   2, 1, // rot 3
};

float combo_bonus[] = {
        1.f, 1.5f, 2.f, 3.f, 4.f, 5.f, 6.f, 8.f, 10.f, 12.f, 15.f, 20.f,
        25.f, 30.f, 40.f, 50.f, 75.f, 100.f
};
#define MAX_COMBO ((sizeof combo_bonus / sizeof *combo_bonus) - 1)

int rewards[] = { 0, 100, 250, 500, 1000 };

int speeds[] = { 50, 40, 35, 30, 26, 23, 20, 18, 16, 14, 12, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1 };
#define MAX_SPEED ((sizeof speeds / sizeof *speeds) - 1)

char countdown_msg[][10] = { "    Go!", "    - 1 -", "    - 2 -", "    - 3 -", "    -   -" };

struct {
        struct {
                int color;
                int part;
        } board[BHEIGHT][BWIDTH];
        int killy_lines[BHEIGHT];
        int line_offset[BHEIGHT]; // amount lines have left to fall
        int left, right, down; // true if holding a direction
        int move_cooldown;
        int falling_x, falling_y, falling_shape, falling_rot; // current piece
        int beam_x, beam_y, beam_shape, beam_rot; // hard drop beam
        int beam_tick;
        int bag[BAG_SZ];
        int bag_idx;
        int grounded;
        int grounded_moves;
        int last_dx_tick;
        int next[5];
        int held_shape;
        int hold_count;
        int lines;
        int score, best;
        int b2b_combo, sq_combo;
        int reward, reward_x, reward_y;
        int level;
        int countdown_time;
        int idle_time;
        int shine_time;
        int dead_time;
        int board_x, board_y, board_w; // positions and sizes of things
        int preview_x, preview_y;
        int hold_x, hold_y;
        int box_w;
        int device;
        char dev_name[80];
} play[NPLAY], *p;

int win_x = 1000; // window size
int win_y = 750;
int bs, bs2; // individual block size, and in half
int line_height; // text line height
int tick;
int joy_tick; // most recent tick when a new joystick was detected
enum state { MAIN_MENU = 0, NUMBER_MENU, ASSIGN, PLAY} state;
int nplay = 1; // number of players
int assign_me;
int menu_pos; // current position in menu
int text_x, text_y; // position of text drawing

SDL_Event event;
SDL_Window *win;
SDL_Renderer *renderer;
TTF_Font *font;
SDL_AudioDeviceID audio;

//prototypes
void setup();
void audio_setup();
void win_event();
void resize(int x, int y);
void update_menu();
void update_stuff();
void draw_menu();
void draw_stuff();
void draw_square(int x, int y, int shape, int shade, int part);
void text(char *fstr, int value);
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

#include "input.c"

//the entry point and main game loop
int main()
{
        setup();

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
                        case SDL_JOYDEVICEREMOVED:     joy_tick = tick; break;
                        case SDL_WINDOWEVENT:          win_event(); break;
                }

                if (joy_tick == tick - 1) joy_setup();

                if (state == MAIN_MENU || state == NUMBER_MENU)
                        update_menu();
                else if (state == PLAY) for (p = play; p < play + nplay; p++)
                        update_stuff();
                SDL_SetRenderDrawColor(renderer, 25, 40, 35, 255);
                SDL_RenderClear(renderer);
                if (state == MAIN_MENU || state == NUMBER_MENU)
                        draw_menu();
                else for (p = play; p < play + nplay; p++)
                        draw_stuff();
                SDL_RenderPresent(renderer);
                SDL_Delay(10);
                tick++;
        }
}

void win_event()
{
        if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
                resize(event.window.data1, event.window.data2);
}

// recalculate sizes and positions on resize
void resize(int x, int y)
{
        win_x = x;
        win_y = y;
        bs = MIN(win_x / (nplay * 22), win_y / 24);
        bs2 = bs / 2;
        line_height = bs * 125 / 100;
        int n = 0;
        for (p = play; p < play + nplay; p++, n++)
        {
                p->board_x = (x / (nplay * 2)) * (2 * n + 1) - bs2 * BWIDTH;
                p->board_y = (y / 2) - bs2 * VHEIGHT;
                p->board_w = bs * 10;
                p->box_w = bs * 5;
                p->hold_x = p->board_x - p->box_w - bs2;
                p->hold_y = p->board_y;
                p->preview_x = p->board_x + p->board_w + bs2;
                p->preview_y = p->board_y;
        }
        if (font) TTF_CloseFont(font);
        font = TTF_OpenFont("../common/res/LiberationSans-Regular.ttf", bs);
}

//initial setup to get the window and rendering going
void setup()
{
        srand(time(NULL));
        SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_AUDIO);

        win = SDL_CreateWindow("Tet",
                        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                        win_x, win_y,
                        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);

        renderer = SDL_CreateRenderer(win, -1, 0);
        if (!renderer)
        {
                fprintf(stderr, "Could not create SDL renderer\n");
                exit(-1);
        }

        TTF_Init();
        resize(win_x, win_y);
        audioinit();
}

void update_menu()
{
        int menu_max = (state == NUMBER_MENU ? 3 : 2);
        if (menu_pos < 0) menu_pos = 0;
        if (menu_pos > menu_max) menu_pos = menu_max;
}

//update everything while the game is running normally
void update_stuff()
{
        if (p->countdown_time > 0)
        {
                int note = p->countdown_time > CTDN_TICKS ? A4 : A5;
                if (p->countdown_time % CTDN_TICKS == 0)
                        silly_noise(TRIANGLE, note, note, 80, 50, 5, 20);

                p->countdown_time--;
                if (p->countdown_time > CTDN_TICKS)
                        return;
        }

        if (!p->falling_shape && !p->shine_time && !p->dead_time)
                new_piece();

        p->grounded = collide(p->falling_x, p->falling_y + 1, p->falling_rot);

        if (p->move_cooldown) p->move_cooldown--;

        if (p->move_cooldown < 2)
        {
                if (p->left)  move(-1, 0, 0);
                if (p->right) move( 1, 0, 0);
                if (p->down)  move( 0, 1, 0);
        }

        if (p->shine_time > 0)
        {
                p->shine_time--;
                if (p->shine_time == 0)
                        kill_lines();
        }
        else for (int y = 0; y < BHEIGHT; y++)
        {
                if (p->line_offset[y] > 0)
                        p->line_offset[y] -= bs2;
                if (p->line_offset[y] < 0)
                        p->line_offset[y] = 0;
        }

        if (p->dead_time > 0)
        {
                int x = p->dead_time % BWIDTH;
                int y = p->dead_time / BWIDTH;

                if (y >= 0 && y < BHEIGHT && x >= 0 && x < BWIDTH && p->dead_time > 49)
                {
                        p->board[y + 0][x].color = rand() % 7 + 1;
                        p->board[y + 0][x].part = '@';
                        if (p->dead_time % 10 == 9)
                        {
                                int note = MAX(A0, p->dead_time * 200 / 1000);
                                silly_noise(SINE, note, note + 12, 10, 10, 10, 25);
                        }
                }

                if (--p->dead_time == 0)
                        new_game();
        }

        if (++p->idle_time >= (p->grounded ? 50 : speeds[MIN(MAX_SPEED, p->level)]))
        {
                move(0, 1, 1);
                p->idle_time = 0;
        }
}

//reset score and pick one extra random piece
void new_game()
{
        memset(p->board, 0, sizeof p->board);
        memset(p->killy_lines, 0, sizeof p->killy_lines);
        memset(p->line_offset, 0, sizeof p->line_offset);
        p->bag_idx = BAG_SZ;
        new_piece();
        if (p->best < p->score) p->best = p->score;
        p->score = 0;
        p->lines = 0;
        p->level = 0;
        p->held_shape = 0;
        p->hold_count = 0;
        p->countdown_time = 4 * CTDN_TICKS;
}

//create a new piece bag with 7 or 8 pieces
int new_bag()
{
        int wildcard_idx = 0;

        for (int i = 0; i < BAG_SZ; i++)
        {
                nope:
                p->bag[i] = rand() % BAG_SZ;
                for (int j = 0; j < i; j++)
                        if (p->bag[j] == p->bag[i])
                                goto nope;

                if (p->bag[i] == 0) wildcard_idx = i;
        }

        // skip wildcard if in position zero
        if (wildcard_idx == 0)
                return 1;

        // set wildcard to some valid piece
        p->bag[wildcard_idx] = rand() % 7 + 1;
        return 0;
}

// set the current piece to the top, middle to start falling
void reset_fall()
{
        p->idle_time = 0;
        p->grounded_moves = 0;
        p->falling_x = 3;
        p->falling_y = 3;
        p->falling_rot = 0;
}

//pick a new next piece from the bag, and put the old one in play
void new_piece()
{
        if (p->bag_idx >= BAG_SZ) p->bag_idx = new_bag();

        p->falling_shape = p->next[0];
        memmove(p->next, p->next + 1, sizeof *(p->next) * 4);
        /*
        p->next[0] = p->next[1];
        p->next[1] = p->next[2];
        p->next[2] = p->next[3];
        p->next[3] = p->next[4];
        */
        p->next[4] = p->bag[p->bag_idx++];
        reset_fall();
}

//move the falling piece left, right, or down
void move(int dx, int dy, int gravity)
{
        if (!gravity)
                p->move_cooldown = p->move_cooldown ? 5 : 15;

        int collision = collide(p->falling_x + dx, p->falling_y + dy, p->falling_rot);

        if (collision == NONE)
        {
                p->falling_x += dx;
                p->falling_y += dy;

                // reset idle time if you voluntarily move DOWN
                if (dy) p->idle_time = 0;

                // remember last successful x movement time
                if (dx) p->last_dx_tick = tick;

                // reset idle time if piece is grounded, limit grounded moves though
                if (p->grounded && p->grounded_moves < 15)
                {
                        p->idle_time = 0;
                        p->grounded_moves++;
                }

                if (!gravity)
                        silly_noise(TRIANGLE, C3, F3, 1, 1, 1, 1);
        }
        else if (dy && gravity)
        {
                bake();
                if (tick != p->beam_tick)
                        silly_noise(TRIANGLE, C4, F4, 10, 10, 10, 10);
        }
        else if (collision == WALL)
        {
                if (dx == -1 && tick - p->last_dx_tick < 8)
                        silly_noise(TRIANGLE, C2, C2, 25, 5, 5, 25);
                if (dx ==  1 && tick - p->last_dx_tick < 8)
                        silly_noise(TRIANGLE, E2, C2, 15, 5, 5, 15);
        }
}

//check if a sub-part of the falling shape is solid at a particular rotation
int is_solid_part(int shape, int rot, int i, int j)
{
        int base = shape * 5 + rot * 5 * 8 * 4;
        int part = shapes[base + j * 5 * 8 + i];
        return part == '.' ? 0 : part;
}

//check if the current piece would collide at a certain position and rotation
int collide(int x, int y, int rot)
{
        int ret = NONE;
        for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++)
        {
                int world_i = i + x;
                int world_j = j + y;

                if (!is_solid_part(p->falling_shape, rot, i, j))
                        continue;

                if (world_i < 0 || world_i >= BWIDTH || world_j >= BHEIGHT)
                        return WALL;

                if (p->board[world_j][world_i].color)
                        ret = NORMAL;
        }
        return ret;
}

//bake the falling piece into the background/board
void bake()
{
        for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++)
        {
                int world_i = i + p->falling_x;
                int world_j = j + p->falling_y;
                int part = is_solid_part(p->falling_shape, p->falling_rot, i, j);

                if (!part || world_i < 0 || world_i >= BWIDTH
                          || world_j < 0 || world_j >= BHEIGHT)
                        continue;

                if (p->board[world_j][world_i].color) // already a block here? game over
                        p->dead_time = BWIDTH * BHEIGHT - 1;

                p->board[world_j][world_i].color = p->falling_shape;
                p->board[world_j][world_i].part = part;
        }

        // check if there are any completed horizontal lines
        int shines = 0;
        for (int j = BHEIGHT - 1; j >= 0; j--)
                for (int i = 0; i < BWIDTH && p->board[j][i].color; i++)
                        if (i == BWIDTH - 1)
                        {
                                shine_line(j);
                                shines++;
                        }

        if (shines)
        {
                p->reward_x = p->board_x + bs * (p->falling_x + 2);
                p->reward_y = p->board_y + bs * (p->falling_y - 4);
                p->reward = 0;
        }
        else
        {
                p->b2b_combo = 0;
        }

        p->falling_shape = 0;
        p->hold_count = 0;
}

//make a completed line "shine" and mark it to be removed
void shine_line(int y)
{
        p->shine_time = 20;
        p->killy_lines[y] = 1;
        for (int i = 0; i < BWIDTH; i++)
                p->board[y][i].color = 8; // the extra color
        silly_noise(SINE, G3, G5, 20, 50, 50, 200);
}

//remove lines that were marked to be removed by shine_line()
void kill_lines()
{
        // clean up sliced pieces
        for (int y = 0; y < BHEIGHT; y++)
        {
                p->line_offset[y] = 0;
                if (p->killy_lines[y]) continue;

                if (y > 0 && p->killy_lines[y - 1])
                        for (int x = 0; x < BWIDTH; x++)
                                p->board[y][x].part &= ~1;

                if (y < BHEIGHT - 1 && p->killy_lines[y + 1])
                        for (int x = 0; x < BWIDTH; x++)
                                p->board[y][x].part &= ~4;
        }

        int new_lines = 0;
        for (int y = 0; y < BHEIGHT; y++)
        {
                if (!p->killy_lines[y]) continue;

                p->lines++;
                new_lines++;
                p->killy_lines[y] = 0;

                for (int j = y; j > 0; j--)
		{
                        p->line_offset[j] = p->line_offset[j-1] + bs;
			for (int i = 0; i < BWIDTH; i++)
                       	        p->board[j][i] = p->board[j-1][i];
                }

                memset(p->board[0], 0, sizeof *p->board);
        }

        p->level = p->lines / 10;
        float b2b_bonus = combo_bonus[MIN(MAX_COMBO, p->b2b_combo)];
        float sq_bonus = combo_bonus[MIN(MAX_COMBO, p->sq_combo / 4)];
        p->b2b_combo += new_lines;
        p->sq_combo = 0;
        p->reward = b2b_bonus * sq_bonus * rewards[new_lines];
        p->score += p->reward;
}

//move the falling piece as far down as it will go
void hard()
{
        while (!collide(p->falling_x, p->falling_y + 1, p->falling_rot))
                p->falling_y++;
        p->idle_time = 50;
        p->beam_shape = p->falling_shape;
        p->beam_rot = p->falling_rot;
        p->beam_x = p->falling_x;
        p->beam_y = p->falling_y;
        p->beam_tick = tick;
        silly_noise(TRIANGLE, A1, E3, 5, 5, 5, 90);
}

//spin the falling piece left or right, if possible
void spin(int dir)
{
        int new_rot = (p->falling_rot + dir) % 4;
        int k = new_rot * 20 + (dir == 1 ? 0 : 10) + (p->falling_shape == 4 ? 80 : 0);

        for (int i = 0; i < 5; i++)
        {
                int kx = kicks[k++];
                int ky = kicks[k++];
                if (!collide(p->falling_x + kx, p->falling_y + ky, new_rot))
                {
                        p->falling_rot = new_rot;
                        p->falling_x += kx;
                        p->falling_y += ky;
                        if (p->grounded && p->grounded_moves < 15)
                        {
                                p->idle_time = 0;
                                p->grounded_moves++;
                        }
                        silly_noise(TRIANGLE, C5, B5, 1, 1, 1, 1);
                        return;
                }
        }
        silly_noise(SQUARE, C5, B5, 1, 1, 1, 1);
}

void hold()
{
        if (p->hold_count++) return;
        SWAP(p->held_shape, p->falling_shape);
        reset_fall();
}

void draw_menu()
{
        p = play;
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderFillRect(renderer, &(SDL_Rect){
                        p->hold_x,
                        p->hold_y + p->box_w + bs2 + line_height * (menu_pos + 1),
                        p->board_w,
                        line_height });
        text_x = p->hold_x;
        text_y = p->hold_y + p->box_w + bs2;
        if (state == MAIN_MENU)
        {
                text("Main Menu"        , 0);
                text("Play"             , 0);
                text("Options"          , 0);
                text("Quit"             , 0);
        }
        else if (state == NUMBER_MENU)
        {
                text("How many players?", 0);
                text("1"                , 0);
                text("2"                , 0);
                text("3"                , 0);
                text("4"                , 0);
        }
}

//draw everything in the game on the screen
void draw_stuff()
{
        // draw background, black boxes
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderFillRect(renderer, &(SDL_Rect){p->hold_x, p->hold_y, p->box_w, p->box_w});
        int x = p->board_x;
        int y = p->board_y;
        SDL_RenderFillRect(renderer, &(SDL_Rect){x, y, p->board_w, bs * VHEIGHT});

        //find ghost piece position
        int ghost_y = p->falling_y;
        while (ghost_y < BHEIGHT && !collide(p->falling_x, ghost_y + 1, p->falling_rot))
                ghost_y++;

        //draw shadow
        if (p->falling_shape)
        {
                struct shadow shadow = shadows[p->falling_rot][p->falling_shape];
                int top = MAX(0, p->falling_y + shadow.y - 5);
                SDL_SetRenderDrawColor(renderer, 8, 13, 12, 255);
                SDL_RenderFillRect(renderer, &(SDL_Rect){
                        x + bs * (p->falling_x + shadow.x),
                        y + bs * top,
                        bs * shadow.w,
                        MAX(0, bs * (ghost_y - top + shadow.y - 5)) });
        }

        //draw hard drop beam
        float loss = .05f * (tick - p->beam_tick);
        if (loss < 1.f && p->beam_shape)
        {
                struct shadow shadow = shadows[p->beam_rot][p->beam_shape];
                int rx = x + bs * (p->beam_x + shadow.x);
                int rw = bs * shadow.w;
                int rh = bs * (p->beam_y + shadow.y - 5);
                int lossw = (1.f - ((1.f - loss) * (1.f - loss))) * rw;
                int lossh = loss < .5f ? 0.f : (1.f - ((1.f - loss) * (1.f - loss))) * rh;
                SDL_SetRenderDrawColor(renderer, 33, 37, 43, 255);
                SDL_RenderFillRect(renderer, &(SDL_Rect){
                                rx + lossw / 2, y, rw - lossw, rh - lossh});
        }

        //draw falling piece & ghost
        for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++)
        {
                int world_i = i + p->falling_x;
                int world_j = j + p->falling_y - 5;
                int ghost_j = j + ghost_y - 5;
                int part = is_solid_part(p->falling_shape, p->falling_rot, i, j);

                if (ghost_j >= 0)
                        draw_square(x + bs * world_i, y + bs * ghost_j, p->falling_shape, 1, part);
                if (world_j >= 0)
                        draw_square(x + bs * world_i, y + bs * world_j, p->falling_shape, 0, part);
        }

        //draw next piece, centered in the preview box
        for (int n = 0; n < 5; n++) for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++)
                draw_square(
                        p->preview_x + bs * i + bs2 + bs2 * center[2 * p->next[n]],
                        p->preview_y + bs + bs * 4 * n + bs * j + bs2 * center[2 * p->next[n] + 1],
                        p->next[n],
                        0,
                        is_solid_part(p->next[n], 0, i, j)
                );

        //draw held piece, centered in the held box
        for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++)
                draw_square(
                        p->hold_x + bs * i + bs2 + bs2 * center[2 * p->held_shape],
                        p->hold_y + bs + bs * j + bs2 * center[2 * p->held_shape + 1],
                        p->held_shape,
                        0,
                        is_solid_part(p->held_shape, 0, i, j)
                );

        //draw board pieces
        for (int i = 0; i < BWIDTH; i++) for (int j = 0; j < VHEIGHT; j++)
                draw_square(x + bs * i,
                                y + bs * j - p->line_offset[j+5],
                                p->board[j+5][i].color,
                                0,
                                p->board[j+5][i].part);

        //draw counters and instructions
        int ln = bs * 110 / 100;
        char combo_amt[80];
        sprintf(combo_amt, "%0.1f", combo_bonus[MIN(MAX_COMBO, p->b2b_combo)]);
        text_x = p->hold_x;
        text_y = p->hold_y + p->box_w + bs2;
        text("Lines:"   , 0           );
        text("%d "      , p->lines    );
        text("Score:"   , 0           );
        text("%d "      , p->score    );
        text("Best:"    , 0           );
        text("%d "      , p->best     );
        text("Level %d ", p->level    );
        text(p->dev_name, 0           );

        if (p->reward)
        {
                text_x = p->reward_x - 100;
                text_y = p->reward_y--;
                text(" %d", p->reward);
        }

        text_x = p->board_x + bs2;
        text_y = p->board_y + bs2 * 19;
        if (p->countdown_time > 0)
                text(countdown_msg[p->countdown_time / CTDN_TICKS], 0);

        if (state == ASSIGN)
                text(p >= play + assign_me ? "Press button to join" : p->dev_name, 0 );
}

//set the current draw color to the color assoc. with a shape
void set_color_from_shape(int shape, int shade)
{
        int r = MAX(colors[shape*3 + 0] + shade, 0);
        int g = MAX(colors[shape*3 + 1] + shade, 0);
        int b = MAX(colors[shape*3 + 2] + shade, 0);
        SDL_SetRenderDrawColor(renderer, r, g, b, 255);
}

//draw a single square of a shape
void draw_square(int x, int y, int shape, int outline, int part)
{
        if (!part) return;
        int bw = MAX(1, outline ? bs / 10 : bs / 6);
        set_color_from_shape(shape, -50);
        SDL_RenderFillRect(renderer, &(SDL_Rect){x, y, bs, bs});
        set_color_from_shape(shape, outline ? -255 : 0);
        SDL_RenderFillRect(renderer, &(SDL_Rect){ // horizontal band
                        x + (part & 8 ? 0 : bw),
                        y + bw,
                        bs - (part & 8 ? 0 : bw) - (part & 2 ? 0 : bw),
                        bs - bw - bw });
        SDL_RenderFillRect(renderer, &(SDL_Rect){ // vertical band
                        x + (part & 32 ? 0 : bw),
                        y + (part & 1 ? 0 : bw),
                        bs - (part & 32 ? 0 : bw) - (part & 16 ? 0 : bw),
                        bs - (part & 1 ? 0 : bw) - (part & 4 ? 0 : bw) });
}

//render a line of text optionally with a %d value in it
void text(char *fstr, int value)
{
        if (!font || !fstr || !fstr[0]) return;
        char msg[80];
        snprintf(msg, 80, fstr, value);
        SDL_Color color = (SDL_Color){180, 190, 185};
        if (fstr[0] == ' ' && tick / 3 % 2) color = (SDL_Color){255, 255, 255};
        SDL_Surface *msgsurf = TTF_RenderText_Blended(font, msg, color);
        SDL_Texture *msgtex = SDL_CreateTextureFromSurface(renderer, msgsurf);
        SDL_Rect fromrec = {0, 0, msgsurf->w, msgsurf->h};
        SDL_Rect torec = {text_x, text_y, msgsurf->w, msgsurf->h};
        SDL_RenderCopy(renderer, msgtex, &fromrec, &torec);
        SDL_DestroyTexture(msgtex);
        SDL_FreeSurface(msgsurf);
        text_y += bs * 125 / 100 + (fstr[strlen(fstr) - 1] == ' ' ? bs : 0);
}

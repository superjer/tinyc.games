// Tet -- http://tinyc.games -- (c) 2023 Jer Wilson
//
// Tet is tiny implementation of a fully-featured Tetris clone.

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#define SDL_DISABLE_IMMINTRIN_H // why do I need this again? For mac? For win??
#include "tet.h"

// allows us to compile as single file:
#include "../common/tinyc.games/audio.c"
#include "input.c"
#include "graphics.c"

// the entry point and main game loop
int main()
{
        setup();
        for (;;)
        {
                int joy_tick = 0;
                while (SDL_PollEvent(&event)) switch (event.type)
                {
                        case SDL_QUIT:                 exit(0);
                        case SDL_KEYDOWN:              key_down();      break;
                        case SDL_KEYUP:                key_up();        break;
                        case SDL_CONTROLLERBUTTONDOWN: joy_down();      break;
                        case SDL_CONTROLLERBUTTONUP:   joy_up();        break;
                        case SDL_JOYDEVICEADDED:
                        case SDL_JOYDEVICEREMOVED:     joy_tick = tick; break;
                        case SDL_WINDOWEVENT:
                                if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
                                        resize(event.window.data1, event.window.data2);
                                break;
                }
                if (joy_tick == tick - 1) joy_setup();

                SDL_SetRenderDrawColor(renderer, 0x2f, 0x2f, 0x2f, 255);
                SDL_RenderClear(renderer);
                draw_menu();

                for (p = play; p < play + nplay; p++)
                {
                        update_player();
                        draw_player();
                }

                SDL_RenderPresent(renderer);
                SDL_Delay(10);
                tick++;
        }
}

// initial setup to get the window and rendering going
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
                fprintf(stderr, "Unable to create renderer: %s\n", SDL_GetError());
                exit(1);
        }

        TTF_Init();
        resize(win_x, win_y);
        audio_init();
}

// remove lines that are full
void kill_lines()
{
        // clean up sliced pieces
        for (int y = 0; y < BHEIGHT; y++)
        {
                p->line_offset[y] = 0;
                if (p->line_fullness[y] == 10) continue;

                if (y > 0 && p->line_fullness[y - 1] == 10)
                        for (int x = 0; x < BWIDTH; x++)
                                p->board[y][x].part &= ~1;

                if (y < BHEIGHT - 1 && p->line_fullness[y + 1] == 10)
                        for (int x = 0; x < BWIDTH; x++)
                                p->board[y][x].part &= ~4;
        }

        int new_lines = 0;
        for (int y = 0; y < BHEIGHT; y++)
        {
                if (p->line_fullness[y] != 10) continue;

                p->lines++;
                new_lines++;
                p->line_fullness[y] = 0;

                for (int j = y; j > 0; j--)
                {
                        p->line_offset[j] = p->line_offset[j-1] + bs;
                        p->line_fullness[j] = p->line_fullness[j-1];
                        for (int i = 0; i < BWIDTH; i++)
                                p->board[j][i] = p->board[j-1][i];
                }

                memset(p->board[0], 0, sizeof *p->board);
                p->line_fullness[0] = 0;
        }

        p->level = p->lines / 10;
        p->combo++;
        p->reward = combo_bonus[MIN(MAX_COMBO, p->combo)] * rewards[new_lines];
        p->score += p->reward;

        if (nplay > 1)
        {
                p->reward /= 400; // in multiplayer, send garbage
                int opponent;
                do opponent = rand() % nplay; while (play + opponent == p);
                play[opponent].garbage += p->reward;
        }
}

void add_garbage()
{
        for (int y = BHEIGHT - 10; y < BHEIGHT; y++)
        {
                int skip = rand() % 10;
                for (int x = 0; x < 10; x++)
                        if (x != skip && rand() % 20)
                        {
                                p->board[y][x].color = 9;
                                p->board[y][x].part = '@';
                                p->line_fullness[y]++;
                        }
        }
}

// reset score and pick one extra random piece
void new_game()
{
        memset(p->board, 0, sizeof p->board);
        memset(p->line_fullness, 0, sizeof p->line_fullness);
        memset(p->line_offset, 0, sizeof p->line_offset);
        p->bag_idx = BAG_SZ;
        if (p->best < p->score) p->best = p->score;
        p->score = 0;
        p->lines = 0;
        p->level = 0;
        p->held.color = 0;
        p->hold_uses = 0;
        p->countdown_time = 4 * CTDN_TICKS;

        if (garbage_race) add_garbage();
}

// set the current piece to the top, middle to start falling
void reset_fall()
{
        p->idle_time = 0;
        p->grounded_moves = 0;
        p->it.x = 3;
        p->it.y = 3;
        p->it.rot = 0;
}

// pick a new next piece from the bag, and put the old one in play
void new_piece()
{
        if (p->bag_idx >= BAG_SZ) // need to make a new bag?
        {
                p->bag_idx = 0;

                for (int i = 0; i < BAG_SZ; i++)
                        p->bag[i] = i % 7 + 1;

                for (int i = 0; i < BAG_SZ; i++)
                        SWAP(p->bag[i], p->bag[rand() % BAG_SZ]);
        }

        p->it.color = p->next[0];
        memmove(p->next, p->next + 1, sizeof *(p->next) * 4);
        p->next[4] = p->bag[p->bag_idx++];
        reset_fall();
}

void receive_garbage()
{
        int gap = rand() % 10;
        for (; p->garbage; p->garbage--)
        {
                memmove(p->board, p->board + 1, (BHEIGHT - 1) * sizeof *p->board);
                memmove(p->line_fullness, p->line_fullness + 1, (BHEIGHT - 1) * sizeof *p->line_fullness);
                for (int i = 0; i < 10; i++)
                        p->board[BHEIGHT - 1][i] = (gap == i) ?
                                (struct spot){0, 0} : (struct spot){9, '@'};
                p->line_fullness[BHEIGHT - 1] = 9;
        }
}

//move the falling piece left, right, or down
void move(int dx, int dy, int gravity)
{
        if (!gravity)
                p->move_cooldown = p->move_cooldown ? 5 : 15;

        int collision = collide(p->it.x + dx, p->it.y + dy, p->it.rot);

        if (collision == NONE)
        {
                p->it.x += dx;
                p->it.y += dy;

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
                        audio_tone(TRIANGLE, C3, F3, 1, 1, 1, 1);
        }
        else if (dy && gravity)
        {
                bake();
                if (p->garbage) receive_garbage();
                if (tick != p->beam_tick)
                        audio_tone(TRIANGLE, C4, F4, 10, 10, 10, 10);
        }
        else if (collision == WALL)
        {
                if (dx == -1 && tick - p->last_dx_tick < 8)
                        audio_tone(TRIANGLE, C2, C2, 25, 5, 5, 25);
                if (dx ==  1 && tick - p->last_dx_tick < 8)
                        audio_tone(TRIANGLE, E2, C2, 15, 5, 5, 15);
        }
}

// update everything related to current player, while the game is running normally
void update_player()
{
        if (state != PLAY) return;

        if (p->countdown_time > 0)
        {
                int note = p->countdown_time > CTDN_TICKS ? A4 : A5;
                if (p->countdown_time % CTDN_TICKS == 0)
                        audio_tone(TRIANGLE, note, note, 80, 50, 5, 20);

                if (--p->countdown_time > CTDN_TICKS)
                        return;
        }

        while (!p->it.color && !p->shine_time && !p->dead_time)
                new_piece();

        p->grounded = collide(p->it.x, p->it.y + 1, p->it.rot);

        if (p->move_cooldown) p->move_cooldown--;

        if (p->move_cooldown < 2)
        {
                if (p->left)  move(-1, 0, 0);
                if (p->right) move( 1, 0, 0);
                if (p->down)  move( 0, 1, 0);
        }

        for (int y = 0; y < BHEIGHT; y++)
                p->line_offset[y] = MAX(0, p->line_offset[y] - bs2);

        if (p->shine_time > 0 && --p->shine_time == 0)
                kill_lines();

        if (p->dead_time > 0 && --p->dead_time == 0)
                new_game();

        if (++p->idle_time >= (p->grounded ? 50 : speeds[MIN(MAX_SPEED, p->level)]))
        {
                move(0, 1, 1);
                p->idle_time = 0;
        }
}

void game_over()
{
        memset(p->bag, 0, sizeof p->bag);
        memset(p->next, 0, sizeof p->next);
        p->dead_time = 100;
}

// check if a line has been completed and act accordingly
int increment_and_check_line(int y)
{
        if (++p->line_fullness[y] != 10)
                return 0;

        p->reward = 0; // set up hovering reward number
        p->reward_x = p->board_x + bs * (p->it.x + 2);
        p->reward_y = p->board_y + bs * (p->it.y - 4);
        p->shine_time = 20;

        for (int i = 0; i < BWIDTH; i++)
                p->board[y][i].color = 8;

        audio_tone(SINE, G3, G5, 20, 50, 50, 200);
        return 1;
}

// bake the falling piece into the background/board
void bake()
{
        int completed_lines = 0;

        for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++)
        {
                int world_i = i + p->it.x;
                int world_j = j + p->it.y;
                int part = is_solid_part(p->it.color, p->it.rot, i, j);

                if (!part) continue;

                if (p->board[world_j][world_i].color)
                        game_over();

                p->board[world_j][world_i].color = p->it.color;
                p->board[world_j][world_i].part = part;
                completed_lines += increment_and_check_line(world_j);
        }

        p->it.color = 0;
        p->hold_uses = 0;
        if (!completed_lines) p->combo = 0; // break combo
}

// check if a sub-part of the falling shape is solid at a particular rotation
int is_solid_part(int shape, int rot, int i, int j)
{
        int base = shape * 5 + rot * 5 * 8 * 4;
        int part = shapes[base + j * 5 * 8 + i];
        return part == '.' ? 0 : part;
}

// check if the current piece would collide at a certain position and rotation
int collide(int x, int y, int rot)
{
        int ret = NONE;
        for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++)
                if (is_solid_part(p->it.color, rot, i, j))
                {
                        if (i + x < 0 || i + x >= BWIDTH || j + y >= BHEIGHT)
                                return WALL;
                        if (p->board[j + y][i + x].color)
                                ret = NORMAL;
                }

        return ret;
}

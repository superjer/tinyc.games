#include "tet.h"
#include "../common/tinyc.games/audio.h"

#define WASD -1
#define ARROW_KEYS -2

void resize(int, int);

void down()
{
        p->down = 1;
        p->move_cooldown = 0;
}

void left()
{
        p->left = 1;
        p->move_cooldown = 0;
}

void right()
{
        p->right = 1;
        p->move_cooldown = 0;
}

// spin the falling piece left or right, if possible
void spin(int dir)
{
        int new_rot = (p->it.rot + dir) % 4;
        int k = new_rot * 20 + (dir == 1 ? 0 : 10) + (p->it.color == 4 ? 80 : 0);

        for (int i = 0; i < 5; i++)
        {
                int kx = kicks[k++];
                int ky = kicks[k++];
                if (!collide(p->it.x + kx, p->it.y + ky, new_rot))
                {
                        p->it.rot = new_rot;
                        p->it.x += kx;
                        p->it.y += ky;
                        if (p->grounded && p->grounded_moves < 15)
                        {
                                p->idle_time = 0;
                                p->grounded_moves++;
                        }
                        audio_tone(TRIANGLE, C5, B5, 1, 1, 1, 1);
                        return;
                }
        }
        audio_tone(SQUARE, C5, B5, 1, 1, 1, 1);
}

// move the falling piece as far down as it will go
void hard()
{
        for (; !collide(p->it.x, p->it.y + 1, p->it.rot); p->it.y++)
                p->score++;
        p->idle_time = 100;
        p->beam = p->it;
        p->beam_tick = tick;
        p->shake_y += .25f;
        audio_tone(TRIANGLE, A1, E3, 5, 5, 5, 90);
}

// hold a piece for later
void hold()
{
        if (p->hold_uses++) return;
        SWAP(p->held.color, p->it.color);
        reset_fall();
}

void joy_setup()
{
        int n;
        SDL_JoystickID *jsids = SDL_GetJoysticks(&n);
        for (int i = 0; jsids[i]; i++)
        {
                if (!SDL_IsGamepad(jsids[i]))
                {
                        printf("Controller not supported: %s", SDL_GetJoystickNameForID(jsids[i]));
                        continue;
                }
                SDL_Gamepad *cont = SDL_OpenGamepad(jsids[i]);
                printf("Controller added: %s %p\n", SDL_GetGamepadNameForID(jsids[i]), (void*)cont);
        }
        SDL_SetGamepadEventsEnabled(true);
}

// set current player to match an input device
void set_player_from_device(int device)
{
        for (int i = 0; i < NPLAY; i++)
        {
                if (play[i].device < 0)
                        p = play + i; // default to any keyboard

                if (play[i].device == device)
                {
                        p = play + i;
                        return;
                }
        }
}

// figure out which "device" from key pressed, i.e. WASD or Arrow keys
int device_from_key()
{
        switch(event.key.key) {
                case SDLK_W: case SDLK_A: case SDLK_S: case SDLK_D: case SDLK_Z: case SDLK_X:
                case SDLK_TAB: case SDLK_CAPSLOCK: case SDLK_LSHIFT:
                        return WASD;
                default:
                        return ARROW_KEYS;
        }
}

int menu_input(int key_or_button)
{
        switch (key_or_button)
        {
                case SDLK_S:  case SDLK_DOWN:
                case SDL_GAMEPAD_BUTTON_DPAD_DOWN:   menu_pos++; break;

                case SDLK_W:  case SDLK_UP:
                case SDL_GAMEPAD_BUTTON_DPAD_UP:     menu_pos--; break;

                case SDLK_RETURN:  case SDLK_Z:
                case SDL_GAMEPAD_BUTTON_SOUTH:
                        if (state == MAIN_MENU)
                        {
                                if (menu_pos == 0) garbage_race = 0;
                                if (menu_pos == 1) garbage_race = 1;
                                if (menu_pos == 2) exit(0);
                                state = NUMBER_MENU;
                                menu_pos = 0;
                        }
                        else
                        {
                                nplay = menu_pos + 1;
                                resize(win_x, win_y);
                                state = ASSIGN;
                                assign_me = 0;
                        }
                        break;
        }
        return 0;
}

int assign(int device)
{
        for (int i = 0; i < assign_me; i++)
                if (play[i].device == device)
                        return 0;

        play[assign_me].device = device;
        sprintf(play[assign_me].dev_name, "%.10s",
                        device == WASD ? "WASD keys" :
                        device == ARROW_KEYS ? "Arrow keys" :
                        SDL_GetGamepadName(SDL_GetGamepadFromID(device)));

        if (++assign_me == nplay)
        {
                state = PLAY;
                assign_me = 0;
                seed = rand();
                for (p = play; p < play + nplay; p++)
                        new_game();
        }
        return 0;
}

// handle a key press from a player
int key_down()
{
        if (event.key.repeat)                           return 0;
        if (state == MAIN_MENU || state == NUMBER_MENU) return menu_input(event.key.key);
        if (state == GAMEOVER)                          return (state = MAIN_MENU);
        if (state == ASSIGN)                            return assign(device_from_key());

        set_player_from_device(device_from_key());

        if (!p->it.color || p->countdown_time >= CTDN_TICKS) return 0;

        switch (event.key.key)
        {
                case SDLK_A:   case SDLK_LEFT:                        left();  break;
                case SDLK_S:   case SDLK_DOWN:                        down();  break;
                case SDLK_D:   case SDLK_RIGHT:                       right(); break;

                case SDLK_W:   case SDLK_UP:                          hard();  break;
                case SDLK_Z:   case SDLK_CAPSLOCK:  case SDLK_COMMA:  spin(3); break;
                case SDLK_X:   case SDLK_LSHIFT:    case SDLK_PERIOD: spin(1); break;
                case SDLK_TAB: case SDLK_SLASH:                       hold();  break;

                case SDLK_L:
                        p->level++;
                        p->lines += 10;
                        break;
        }

        return 0;
}

void key_up()
{
        if (state == ASSIGN) return;
        set_player_from_device(device_from_key());

        switch (event.key.key)
        {
                case SDLK_A:      case SDLK_LEFT:   p->left = 0;  break;
                case SDLK_D:      case SDLK_RIGHT:  p->right = 0; break;
                case SDLK_S:      case SDLK_DOWN:   p->down = 0;  break;
        }
}

int joy_down()
{
        if (state == ASSIGN) return assign(event.gbutton.which);
        if (state == GAMEOVER) return (state = MAIN_MENU);
        if (state == MAIN_MENU || state == NUMBER_MENU) return menu_input(event.gbutton.button);
        set_player_from_device(event.gbutton.which);

        if (p->it.color) switch(event.gbutton.button)
        {
                case SDL_GAMEPAD_BUTTON_SOUTH:            spin(3); break;
                case SDL_GAMEPAD_BUTTON_EAST:            spin(1); break;
                case SDL_GAMEPAD_BUTTON_DPAD_UP:      hard();  break;
                case SDL_GAMEPAD_BUTTON_DPAD_DOWN:    down();  break;
                case SDL_GAMEPAD_BUTTON_DPAD_LEFT:    left();  break;
                case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:   right(); break;
                case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER: hold();  break;
        }
        return 0;
}

void joy_up()
{
        if (state == ASSIGN) return;
        set_player_from_device(event.gbutton.which);

        switch(event.gbutton.button)
        {
                case SDL_GAMEPAD_BUTTON_DPAD_DOWN:  p->down  = 0; break;
                case SDL_GAMEPAD_BUTTON_DPAD_LEFT:  p->left  = 0; break;
                case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: p->right = 0; break;
        }
}

#include <SDL.h>

int assign(int device);
int menu_input();
void set_p_from_device(int device);

void joy_setup()
{
        for (int i = 0; i < SDL_NumJoysticks(); i++)
        {
                if (!SDL_IsGameController(i))
                {
                        printf("Controller NOT supported: %s", SDL_JoystickNameForIndex(i));
                        printf(" - Google for SDL_GAMECONTROLLERCONFIG to fix this\n");
                        continue;
                }
                SDL_GameController *cont = SDL_GameControllerOpen(i);
                printf("Controller added: %s %p\n", SDL_GameControllerNameForIndex(i), cont);
        }
        SDL_GameControllerEventState(SDL_ENABLE);
}

//handle a key press from a player
int key_down()
{
        if (event.key.repeat) return 0;

        if (event.key.keysym.sym >= '1' && event.key.keysym.sym <= '4')
        {
                nplay = event.key.keysym.sym - '0';
                resize(win_x, win_y);
                state = ASSIGN;
                assign_me = 0;
                return 0;
        }

        if (state == MAIN_MENU || state == NUMBER_MENU) return menu_input();
        if (state == ASSIGN) return assign(-1);

        set_p_from_device(-1);

        if (p->falling_shape && p->countdown_time < CTDN_TICKS) switch (event.key.keysym.sym)
        {
                case SDLK_a:      case SDLK_LEFT:   p->left = 1;  p->move_cooldown = 0; break;
                case SDLK_d:      case SDLK_RIGHT:  p->right = 1; p->move_cooldown = 0; break;
                case SDLK_s:      case SDLK_DOWN:   p->down = 1;  p->move_cooldown = 0; break;
                case SDLK_w:      case SDLK_UP:     hard();    break;
                case SDLK_COMMA:  case SDLK_z:      spin(3);   break;
                case SDLK_PERIOD: case SDLK_x:      spin(1);   break;
                case SDLK_TAB:    case SDLK_LSHIFT: hold();    break;
        }

        return 0;
}

void key_up()
{
        if (state == ASSIGN) return;
        set_p_from_device(-1);

        switch (event.key.keysym.sym)
        {
                case SDLK_a:      case SDLK_LEFT:   p->left = 0;  break;
                case SDLK_d:      case SDLK_RIGHT:  p->right = 0; break;
                case SDLK_s:      case SDLK_DOWN:   p->down = 0;  break;
        }
}

int joy_down()
{
        if (state == ASSIGN) return assign(event.cbutton.which);
        set_p_from_device(event.cbutton.which);

        if (p->falling_shape) switch(event.cbutton.button)
        {
                case SDL_CONTROLLER_BUTTON_A:            spin(3); break;
                case SDL_CONTROLLER_BUTTON_B:            spin(1); break;
                case SDL_CONTROLLER_BUTTON_DPAD_UP:      hard();  break;
                case SDL_CONTROLLER_BUTTON_DPAD_DOWN:    p->down  = 1; p->move_cooldown = 0; break;
                case SDL_CONTROLLER_BUTTON_DPAD_LEFT:    p->left  = 1; p->move_cooldown = 0; break;
                case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:   p->right = 1; p->move_cooldown = 0; break;
                case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: hold();  break;
        }
        return 0;
}

void joy_up()
{
        if (state == ASSIGN) return;
        set_p_from_device(event.cbutton.which);

        switch(event.cbutton.button)
        {
                case SDL_CONTROLLER_BUTTON_DPAD_DOWN:  p->down  = 0; break;
                case SDL_CONTROLLER_BUTTON_DPAD_LEFT:  p->left  = 0; break;
                case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: p->right = 0; break;
        }
}

int menu_input()
{
        switch (event.key.keysym.sym)
        {
                case SDLK_s:      case SDLK_DOWN:   menu_pos++; break;
                case SDLK_w:      case SDLK_UP:     menu_pos--; break;
                case SDLK_RETURN:
                        if (state == MAIN_MENU)
                        {
                                if (menu_pos == 0) state = NUMBER_MENU;
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
}

int assign(int device)
{
        for (int i = 0; i < assign_me; i++)
                if (play[i].device == device)
                        return 0;

        play[assign_me].device = device;
        sprintf(play[assign_me].dev_name, "%.10s", (device == -1 ?
                        "Keyboard" :
                        SDL_GameControllerName(SDL_GameControllerFromInstanceID(device))));

        if (++assign_me == nplay)
        {
                state = PLAY;
                assign_me = 0;
                for (p = play; p < play + nplay; p++)
                        new_game();
        }
        return 0;
}

void set_p_from_device(int device)
{
        for (p = play; p < play + nplay; p++)
                if (p->device == device)
                        return;
        p = play; // default to first player
}

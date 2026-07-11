#include "tet.c"
#ifndef TET_INPUT_C_INCLUDED
#define TET_INPUT_C_INCLUDED

void resize(int, int);
void hard();

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

// handle a key press from a player
int key_down()
{
        if (event.key.repeat)    return 0;

        if (event.key.key == SDLK_ESCAPE) exit(0);

        if (!p->it.color || p->countdown_time >= CTDN_TICKS) return 0;

        switch (event.key.key)
        {
                case SDLK_A:   case SDLK_LEFT:   p->left  = 1; p->move_cooldown = 0; break;
                case SDLK_S:   case SDLK_DOWN:   p->down  = 1; p->move_cooldown = 0; break;
                case SDLK_D:   case SDLK_RIGHT:  p->right = 1; p->move_cooldown = 0; break;

                case SDLK_W:   case SDLK_UP:                          hard();  break;
                case SDLK_Z:   case SDLK_CAPSLOCK:  case SDLK_COMMA:  spin(3); break;
                case SDLK_X:   case SDLK_LSHIFT:    case SDLK_PERIOD: spin(1); break;
                case SDLK_TAB: case SDLK_SLASH:                       hold();  break;
        }

        return 0;
}

void key_up()
{
        switch (event.key.key)
        {
                case SDLK_A:      case SDLK_LEFT:   p->left = 0;  break;
                case SDLK_D:      case SDLK_RIGHT:  p->right = 0; break;
                case SDLK_S:      case SDLK_DOWN:   p->down = 0;  break;
        }
}

int btn_down()
{
        if (event.gbutton.button == SDL_GAMEPAD_BUTTON_START) exit(0);

        if (p->it.color) switch(event.gbutton.button)
        {
                case SDL_GAMEPAD_BUTTON_SOUTH:            spin(3); break;
                case SDL_GAMEPAD_BUTTON_EAST:            spin(1); break;
                case SDL_GAMEPAD_BUTTON_DPAD_UP:       hard(); break;
                case SDL_GAMEPAD_BUTTON_DPAD_DOWN:  p->down  = 1; p->move_cooldown = 0; break;
                case SDL_GAMEPAD_BUTTON_DPAD_LEFT:  p->left  = 1; p->move_cooldown = 0; break;
                case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: p->right = 1; p->move_cooldown = 0; break;
                case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER: hold(); break;
        }
        return 0;
}

void btn_up()
{
        switch(event.gbutton.button)
        {
                case SDL_GAMEPAD_BUTTON_DPAD_DOWN:  p->down  = 0; break;
                case SDL_GAMEPAD_BUTTON_DPAD_LEFT:  p->left  = 0; break;
                case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: p->right = 0; break;
        }
}

#endif // TET_INPUT_C_INCLUDED

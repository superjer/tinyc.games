#include "blocko.c"
#ifndef BLOCKO_INTERFACE_C_INCLUDED
#define BLOCKO_INTERFACE_C_INCLUDED

void resize()
{
        screenw = event.window.data1;
        screenh = event.window.data2;
        vulkan_recreate_swapchain();
}

void jump(int down)
{
        player[my_player].jump_held = down;
        if (down)
                player[my_player].jumping = JUMP_BUFFER_FRAMES;
}

void key_move(int down)
{
        if (console_key(down)) return;
        if (event.key.repeat) return;

        switch (event.key.key)
        {
                // continuous movement stuff
                case SDLK_W:      player[my_player].goingf = down;
                        if (down) player[my_player].cooldownf += 10; // detect double tap
                        break;
                case SDLK_S:      player[my_player].goingb   = down; break;
                case SDLK_A:      player[my_player].goingl   = down; break;
                case SDLK_D:      player[my_player].goingr   = down; break;
                case SDLK_LSHIFT: player[my_player].sneaking = down; break;
                case SDLK_LCTRL:  player[my_player].running  = down; break;

                // instantaneous movement
                case SDLK_SPACE:
                        jump(down);
                        break;

                // switch held block (the mouse wheel does this too)
                case SDLK_Q:
                        if (down) held_cycle(-1);
                        break;
                case SDLK_E:
                        if (down) held_cycle(+1);
                        break;

                // menu stuff
                case SDLK_ESCAPE:
                        SDL_SetWindowRelativeMouseMode(vk.window, false);
                        mouselook = false;
                        break;

                // debug stuff
                case SDLK_H: // show help
                        if (down)
                                help_layer = !help_layer;
                        break;
                case SDLK_N: // noclip: fly through solids, no gravity
                        if (down) player[my_player].noclip = !player[my_player].noclip;
                        break;
        }
}

void mouse_move()
{
        if (!mouselook) return;

        player[my_player].yaw += event.motion.xrel * 0.001;
        player[my_player].pitch += event.motion.yrel * 0.001;

        if (player[my_player].yaw >= TAU) player[my_player].yaw -= TAU;
        if (player[my_player].yaw < 0.f) player[my_player].yaw += TAU;

        float limit = 3.1415926535 * 0.5 - 0.001;
        if (player[my_player].pitch > limit) player[my_player].pitch = limit;
        if (player[my_player].pitch < -limit) player[my_player].pitch = -limit;
}

void mouse_wheel()
{
        if (!mouselook) return;
        // scroll up = next block, scroll down = previous
        if (event.wheel.y > 0) held_cycle(+1);
        else if (event.wheel.y < 0) held_cycle(-1);
}

void mouse_button(int down)
{
        if (!mouselook)
        {
                if (down)
                {
                        SDL_SetWindowRelativeMouseMode(vk.window, true);
                        mouselook = true;
                }
        }
        else if (event.button.button == SDL_BUTTON_LEFT)
        {
                player[my_player].breaking = down;
        }
        else if (event.button.button == SDL_BUTTON_RIGHT)
        {
                player[my_player].building = down;
        }
        else if (event.button.button == SDL_BUTTON_X1)
        {
                jump(down);
        }
}

#endif // BLOCKO_INTERFACE_C_INCLUDED

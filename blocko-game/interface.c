#include "blocko.c"
#ifndef BLOCKO_INTERFACE_C_INCLUDED
#define BLOCKO_INTERFACE_C_INCLUDED

void resize()
{
        screenw = event.window.data1;
        screenh = event.window.data2;
}

void jump(int down)
{
        if (player[0].wet)
                player[0].jumping = down;
        else if (down)
                player[0].jumping = JUMP_BUFFER_FRAMES;
}

void key_move(int down)
{
        if (event.key.repeat) return;

        switch (event.key.key)
        {
                // continuous movement stuff
                case SDLK_W:      player[0].goingf = down;
                        if (down) player[0].cooldownf += 10; // detect double tap
                        break;
                case SDLK_S:      player[0].goingb   = down; break;
                case SDLK_A:      player[0].goingl   = down; break;
                case SDLK_D:      player[0].goingr   = down; break;
                case SDLK_LSHIFT: player[0].sneaking = down; break;
                case SDLK_LCTRL:  player[0].running  = down; break;
                case SDLK_Z:      zooming            = down; break;

                // instantaneous movement
                case SDLK_SPACE:
                        jump(down);
                        break;

                // place a light
                case SDLK_E:
                        player[0].lighting = down;
                        break;

                // menu stuff
                case SDLK_ESCAPE:
                        SDL_SetWindowRelativeMouseMode(win, false);
                        mouselook = false;
                        break;

                // debug stuff
                case SDLK_Q: // go up alot
                        if (!down)
                        {
                                player[0].pos.y -= 1000;
                                player[0].grav = GRAV_ZERO;
                        }
                        break;
                case SDLK_F: // go fast
                        if (down)
                                fast = (fast == 1.f) ? 8.f : 1.f;
                        break;
                case SDLK_H: // show help
                        if (down)
                                help_layer = (help_layer == 1) ? 0 : 1;
                        break;
                case SDLK_G: // show help
                        if (down)
                                help_layer = (help_layer == 2) ? 0 : 2;
                        break;
                case SDLK_R: // toggle phys step regulation
                        if (down)
                        {
                                regulated = !regulated;
                                fprintf(stderr, "%s\n", regulated ? "regulated" : "unregulated");
                        }
                        break;
                case SDLK_V: // toggle vsync
                        if (down)
                        {
                                vsync = !vsync;
                                SDL_GL_SetSwapInterval(vsync);
                                fprintf(stderr, "%s\n", vsync ? "vsync" : "no vsync");
                        }
                        break;
                case SDLK_SLASH: // toggle antialiasing
                        if (down)
                        {
                                antialiasing = !antialiasing;
                                fprintf(stderr, "%s\n", antialiasing ? "antialiasing" : "no antialiasing");
                        }
                        break;
                case SDLK_T: // build lighting testing area
                        if (down) build_test_area();
                        break;
                case SDLK_N: // night mode on/off
                        if (down) reverse_sun = !reverse_sun;
                        break;
                case SDLK_L: // toggle light values
                        if (down) show_light_values = !show_light_values;
                        break;
                case SDLK_P: // speed of the sun
                        if (down) speedy_sun = !speedy_sun;
                        break;
                case SDLK_M: // do shadow mapping
                        if (!down) shadow_mapping = !shadow_mapping;
                        break;
                case SDLK_C: // change view distance
                        if (down) {
                                if (draw_dist < 320.f)
                                        draw_dist = 320.f;
                                else if (draw_dist < 640.f)
                                        draw_dist = 640.f;
                                else if (draw_dist < 1024.f)
                                        draw_dist = 1024.f;
                                else
                                        draw_dist = 160.f;
                                fprintf(stderr, "draw_dist: %f\n", draw_dist);
                        }
                        break;
                case SDLK_F1: // do frustum culling
                        if (down) frustum_culling = !frustum_culling;
                        break;
                case SDLK_F2: // stop updating frustum culling
                        if (down) lock_culling = !lock_culling;
                        break;
                case SDLK_F3: // show FPS and timings etc.
                        if (!down) noisy = !noisy;
                        break;
                case SDLK_F4: // show which chunks are being sent to gl
                        if (!down) show_fresh_updates = !show_fresh_updates;
                        break;
                case SDLK_F5: // delete test chunk
                        if (!down) for(int x=0;x<CHUNKW;x++) for(int y=0;y<TILESH;y++) for(int z=0;z<CHUNKD;z++)
                        {
                                T_(C2B(32) + x, y, C2B(32) + z) = OPEN;
                        }
                        break;
                case SDLK_F12: // draw shadow map on the sun
                        if (!down) show_shadow_map = !show_shadow_map;
                        break;

                /*
                case SDLK_LEFT:  if (down) scoot(-1,  0); break;
                case SDLK_RIGHT: if (down) scoot( 1,  0); break;
                case SDLK_DOWN:  if (down) scoot( 0, -1); break;
                case SDLK_UP:    if (down) scoot( 0,  1); break;
                */

                case SDLK_LEFT:     if (down) sun_yaw   -= .1f; break;
                case SDLK_RIGHT:    if (down) sun_yaw   += .1f; break;
                case SDLK_DOWN:     if (down) sun_pitch -= .1f; break;
                case SDLK_UP:       if (down) sun_pitch += .1f; break;
                case SDLK_PAGEDOWN: if (down) sun_roll  -= .1f; break;
                case SDLK_PAGEUP:   if (down) sun_roll  += .1f; break;
        }
}

void mouse_move()
{
        if (!mouselook) return;

        player[0].yaw += event.motion.xrel * 0.001;
        player[0].pitch += event.motion.yrel * 0.001;

        if (player[0].yaw >= TAU) player[0].yaw -= TAU;
        if (player[0].yaw < 0.f) player[0].yaw += TAU;

        float limit = 3.1415926535 * 0.5 - 0.001;
        if (player[0].pitch > limit) player[0].pitch = limit;
        if (player[0].pitch < -limit) player[0].pitch = -limit;
}

void mouse_button(int down)
{
        if (!mouselook)
        {
                if (down)
                {
                        SDL_SetWindowRelativeMouseMode(win, true);
                        mouselook = true;
                }
        }
        else if (event.button.button == SDL_BUTTON_LEFT)
        {
                player[0].breaking = down;
        }
        else if (event.button.button == SDL_BUTTON_RIGHT)
        {
                player[0].building = down;
        }
        else if (event.button.button == SDL_BUTTON_X1)
        {
                jump(down);
        }
}

#endif // BLOCKO_INTERFACE_C_INCLUDED

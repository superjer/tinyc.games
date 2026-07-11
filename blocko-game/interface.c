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
        if (test_lock) return; // test running: only the console works
        if (tweak_key(down)) return; // terrain tweaker panel (before the
                                     // repeat gate: held arrows keep stepping)
        if (pmedit_key(down)) return; // model editor (U toggles it); sits
                                      // before the repeat gate so held keys
                                      // keep nudging the joint origin
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
                case SDLK_Z:      zooming            = down; break;

                // cycle camera: first person / third person / second person
                case SDLK_V:
                        if (down) cam_view = (cam_view + 1) % CAM_NR_VIEWS;
                        break;

                // instantaneous movement
                case SDLK_SPACE:
                        jump(down);
                        break;

                // place a light
                case SDLK_E:
                        player[my_player].lighting = down;
                        break;

                // spawn a slime where the crosshair points
                case SDLK_B:
                        if (down && place_x >= 0)
                                mob_spawn_at(place_x, place_y, place_z);
                        break;

                // menu stuff
                case SDLK_ESCAPE:
                        SDL_SetWindowRelativeMouseMode(vk.window, false);
                        mouselook = false;
                        break;

                // debug stuff
                case SDLK_Q: // go up alot
                        if (!down)
                        {
                                player[my_player].pos.y -= 16000;
                                player[my_player].grav = GRAV_ZERO - 5;
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
                case SDLK_N: // night mode on/off
                        if (down) reverse_sun = !reverse_sun;
                        break;
                case SDLK_P: // speed of the sun
                        if (down) speedy_sun = !speedy_sun;
                        break;
                case SDLK_M: // do shadow mapping
                        if (!down) shadow_mapping = !shadow_mapping;
                        break;
                case SDLK_F7: // toggle tall grass shadows (T belongs to chat now)
                        if (!down) grass_shadows = !grass_shadows;
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
                case SDLK_F2: // freeze culling to see what gets culled
                        if (down) lock_culling = !lock_culling;
                        break;
                case SDLK_F3: // show FPS and timings etc.
                        if (!down) noisy = !noisy;
                        break;
                case SDLK_F5: // hot-reload shaders
                        if (!down) {
                                int failed = vulkan_reload_all_pipelines();
                                if (failed > 0) {
                                        snprintf(reload_msg, sizeof reload_msg,
                                                "SHADER RELOAD FAILED (%d) - see console", failed);
                                        reload_msg_r = 1; reload_msg_g = 0.2f; reload_msg_b = 0.2f;
                                } else {
                                        snprintf(reload_msg, sizeof reload_msg, "shaders reloaded");
                                        reload_msg_r = 0.3f; reload_msg_g = 1; reload_msg_b = 0.3f;
                                }
                                reload_msg_expire = SDL_GetTicks() +
                                        (failed > 0 ? 6000 : 1500);
                        }
                        break;
                case SDLK_F6: // freeze shadows + sun to inspect cascade edges
                        if (down) freeze_shadows = !freeze_shadows;
                        break;
                case SDLK_F12: // draw shadow map on the sun
                        if (!down) show_shadow_map = !show_shadow_map;
                        break;

                // teleport a whole chunk - auto_scoot recenters the window after
                case SDLK_LEFT:  if (down) player[my_player].pos.x -= CHUNKW * BS; break;
                case SDLK_RIGHT: if (down) player[my_player].pos.x += CHUNKW * BS; break;
                case SDLK_DOWN:  if (down) player[my_player].pos.z -= CHUNKD * BS; break;
                case SDLK_UP:    if (down) player[my_player].pos.z += CHUNKD * BS; break;

                /*
                case SDLK_LEFT:     if (down) sun_yaw   -= .1f; break;
                case SDLK_RIGHT:    if (down) sun_yaw   += .1f; break;
                case SDLK_DOWN:     if (down) sun_pitch -= .1f; break;
                case SDLK_UP:       if (down) sun_pitch += .1f; break;
                */
                case SDLK_PAGEDOWN: if (down) sun_roll  -= .1f; break;
                case SDLK_PAGEUP:   if (down) sun_roll  += .1f; break;
        }
}

void mouse_move()
{
        if (pmedit_on) { pmedit_motion(); return; }
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
        if (pmedit_on) return;
        if (!mouselook) return;
        // scroll up = next block, scroll down = previous
        if (event.wheel.y > 0) held_cycle(+1);
        else if (event.wheel.y < 0) held_cycle(-1);
}

void mouse_button(int down)
{
        if (pmedit_on) { pmedit_click(down); return; }
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

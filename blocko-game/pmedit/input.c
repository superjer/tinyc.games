#include "../blocko.c"
#ifndef BLOCKO_PMEDIT_INPUT_C_INCLUDED
#define BLOCKO_PMEDIT_INPUT_C_INCLUDED

// pmedit/input.c - keyboard, mouse and the 60Hz update: every mode's control flow

void pmedit_toggle()
{
        pmedit_on = !pmedit_on;
        struct player *pl = &player[my_player];
        if (pmedit_on)
        {
                pmedit_sel = -1;
                pmedit_joint = pmedit_socket = pmedit_parent = pmedit_animate = 0;
                pmedit_resize = pmedit_face = pmedit_newpart = pmedit_restang = 0;
                pmedit_hide = 0;
                pmedit_hidden = 0;
                pmedit_picker = 0;
                pmedit_myaw = pmedit_mpitch = 0;
                pmedit_yramp = pmedit_pramp = 0;
                pmedit_kw = pmedit_ka = pmedit_ks = pmedit_kd = 0;
                pmedit_paint_btn = 0;
                pmedit_pal_drag = pmedit_flood = pmedit_resize_drag = 0;
                pmedit_snap = 1;
                pmedit_hist_sum = pm_checksum(&pm_models[my_player]);
                pmedit_hist_n = 0;
                pmedit_hist_cool = 0;
                pmedit_undo_reset();
                // free the cursor; stop any in-flight movement and mining
                pl->goingf = pl->goingb = pl->goingl = pl->goingr = 0;
                pl->breaking = pl->building = pl->running = pl->sneaking = 0;
                zooming = 0;
                SDL_SetWindowRelativeMouseMode(vk.window, false);
                mouselook = false;
                // the editor draws its own tool cursor: hide the OS one, and
                // seed our position from where the pointer actually is so it
                // doesn't jump on the first frame
                SDL_HideCursor();
                SDL_GetMouseState(&pmedit_mx, &pmedit_my);
                // the world camera swings out front (the in-world model
                // hides too): like zooming out and grabbing the player.
                // Already in 2nd person: keep the pitch they chose
                pmedit_prev_view = cam_view;
                pmedit_level_cam = cam_view != CAM_SECOND;
                cam_view = CAM_SECOND;
        }
        else
        {
                pmedit_picker = 0;
                pmedit_newpart_cancel(); // an unplaced NEW PART doesn't survive
                pmedit_hist_save(1);
                pmodel_send_mine();
                SDL_ShowCursor(); // hand the OS cursor back
                SDL_SetWindowRelativeMouseMode(vk.window, true);
                mouselook = true;
                cam_view = pmedit_prev_view;
        }
}

// JOINT/SOCKET mode: nudge the active point one px along the gizmo-frame
// axis that best matches a camera-relative direction - Up/Down arrows
// vertically, Left/Right arrows sideways; rotate the turntable (WASD still
// works) to reach the other horizontal axis. Directions are what the arrows
// do to the POINT being placed: the attachment point walks the piece's
// space, the attach point walks the parent's (carrying the piece with it) -
// both step straight with the arrows.
static void pmedit_joint_move(int k)
{
        float f[3], vm[16];
        lookit(vm, f, 0, 0, 0, 0, camplayer.yaw); // the editor's leveled camera
        float d[3];
        switch (k)
        {
        case SDLK_UP:    d[0] =  vm[1]; d[1] =  vm[5]; d[2] =  vm[9]; break;
        case SDLK_DOWN:  d[0] = -vm[1]; d[1] = -vm[5]; d[2] = -vm[9]; break;
        case SDLK_LEFT:  d[0] = -vm[0]; d[1] = -vm[4]; d[2] = -vm[8]; break;
        default:         d[0] =  vm[0]; d[1] =  vm[4]; d[2] =  vm[8]; break; // RIGHT
        }

        // into the gizmo's local frame: transpose of the rotation columns
        // (uniform scale doesn't matter for picking the dominant axis).
        // RESIZE has no gizmo; the piece's own frame serves.
        float *M = pmedit_resize ? pmedit_mats[pmedit_sel] : pmedit_gizmo_mat;
        float dl[3];
        for (int a = 0; a < 3; a++)
                dl[a] = M[4*a] * d[0] + M[4*a+1] * d[1] + M[4*a+2] * d[2];

        struct pm_piece *pc = &pm_models[my_player].piece[pmedit_sel];
        if (pmedit_resize)
        {
                // any arrow pushes/pulls the picked face along its own axis,
                // by relative angle: dl[fax] is the arrow's world direction
                // dotted with the face's axis, so the face grows when the
                // arrow leans toward its outward normal on screen, shrinks
                // when it leans away, and near-perpendicular arrows do
                // nothing (rotate a bit). No face picked: arrows are idle
                if (!pmedit_face) return;
                int fax = pmedit_axis_of[pmedit_face];
                if (fabsf(dl[fax]) < 0.3f) return;
                int out = dl[fax] * pmedit_side_of[pmedit_face] > 0;
                if (pm_piece_resize(&pm_models[my_player], pmedit_sel,
                                pmedit_face, out))
                        pmodel_upload(my_player);
                return;
        }

        int ax = 0;
        if (fabsf(dl[1]) > fabsf(dl[ax])) ax = 1;
        if (fabsf(dl[2]) > fabsf(dl[ax])) ax = 2;
        int step = dl[ax] > 0 ? 1 : -1;
        unsigned char *pt = pmedit_joint ? pc->origin : pc->attach;
        // a flush attach can sit past the parent's tile - let nudges roam there
        int hi = pmedit_joint ? PM_TILE : 2 * PM_TILE;
        int was = pt[ax];
        pt[ax] = ICLAMP(pt[ax] + step, 0, hi);
        if (pmedit_joint)
        {
                // carry the attach point along so the piece doesn't budge
                int at = pc->attach[ax] + pt[ax] - was;
                pc->attach[ax] = ICLAMP(at, 0, 2 * PM_TILE);
        }
}

// RESTING ANGLE mode: arrows tilt the selected piece a degree at a time
// (up/down pitch, left/right yaw), Q/E roll it; the pose clamps at +-25 deg
static void pmedit_rest_adjust(int k)
{
        signed char *r = pm_models[my_player].piece[pmedit_sel].rest;
        int a, d;
        switch (k)
        {
        case SDLK_UP:    a = 0; d = -1; break;
        case SDLK_DOWN:  a = 0; d =  1; break;
        case SDLK_LEFT:  a = 1; d = -1; break;
        case SDLK_RIGHT: a = 1; d =  1; break;
        case SDLK_Q:     a = 2; d = -1; break;
        case SDLK_E:     a = 2; d =  1; break;
        default: return;
        }
        int v = r[a] + d;
        r[a] = ICLAMP(v, -25, 25);
}

// keyboard while the editor owns it. Returns 1 when the key was consumed.
// (Called before key_move's repeat gate: repeats drive the joint nudge.)
int pmedit_key(int down)
{
        int k = event.key.key;
        int joint_move_key = k == SDLK_UP || k == SDLK_DOWN ||
                k == SDLK_LEFT || k == SDLK_RIGHT;
        int rest_key = joint_move_key || k == SDLK_Q || k == SDLK_E;
        int rest_ok = pmedit_restang && pmedit_sel >= 0 && !pmedit_animate;

        // arrows nudge in the gizmo modes AND in the plain selected view,
        // where they move the piece around just like MOVE PART (mouse-less)
        int nudge_ok = pmedit_sel >= 0 && !pmedit_parent && !pmedit_animate
                && !pmedit_restang;
        // the picker owns the whole keyboard while it's up: ESC or U closes it
        if (pmedit_on && pmedit_picker)
        {
                if (down && !event.key.repeat && (k == SDLK_ESCAPE || k == SDLK_U))
                        pmedit_picker = 0;
                else if (down && !event.key.repeat && k == SDLK_LEFT)  pmedit_pick_flip(-1);
                else if (down && !event.key.repeat && k == SDLK_RIGHT) pmedit_pick_flip(1);
                return 1;
        }
        int ctrl_z = down && (event.key.mod & SDL_KMOD_CTRL) && k == SDLK_Z;
        int ctrl_y = down && (event.key.mod & SDL_KMOD_CTRL) && k == SDLK_Y;
        int is_redo = ctrl_y || (ctrl_z && (event.key.mod & SDL_KMOD_SHIFT));
        int is_undo = ctrl_z && !(event.key.mod & SDL_KMOD_SHIFT);
        if (event.key.repeat)
        {
                if (pmedit_on && rest_ok && down && rest_key)
                        pmedit_rest_adjust(k);
                else if (pmedit_on && nudge_ok && down && joint_move_key)
                        pmedit_joint_move(k);
                else if (pmedit_on && is_redo) pmedit_redo(); // hold to repeat
                else if (pmedit_on && is_undo) pmedit_undo();
                return pmedit_on; // repeats do nothing else while editing
        }
        if (k == SDLK_U) { if (down) pmedit_toggle(); return 1; }
        if (!pmedit_on) return 0;
        if (k >= SDLK_F1 && k <= SDLK_F12) return 0; // debug keys still work

        if (rest_ok && rest_key)
        {
                if (down) pmedit_rest_adjust(k);
                return 1;
        }
        if (nudge_ok && joint_move_key)
        {
                if (down) pmedit_joint_move(k);
                return 1;
        }

        switch (k)
        {
        case SDLK_W: pmedit_kw = down; break;
        case SDLK_A: pmedit_ka = down; break;
        case SDLK_S: pmedit_ks = down; break;
        case SDLK_D: pmedit_kd = down; break;
        case SDLK_C: // ctrl-C copies, same as the MAKE COPY button
                if (down && event.key.mod & SDL_KMOD_CTRL && !pmedit_animate)
                        pmedit_copy();
                break;
        case SDLK_Z: // ctrl-Z undo, ctrl-shift-Z redo (local only, no net)
                if (is_redo) pmedit_redo();
                else if (is_undo) pmedit_undo();
                break;
        case SDLK_Y: // ctrl-Y redo, the other convention
                if (is_redo) pmedit_redo();
                break;
        case SDLK_DELETE:
        case SDLK_BACKSPACE: // both delete, same as the button
                if (down && !pmedit_animate) pmedit_delete();
                break;
        case SDLK_ESCAPE:
                if (down)
                {
                        if (pmedit_animate) pmedit_animate = 0;
                        else if (pmedit_hide) pmedit_hide = 0;
                        else if (pmedit_sel >= 0)
                        {
                                pmedit_newpart_cancel();
                                pmedit_sel = -1;
                                pmedit_joint = pmedit_socket = pmedit_parent = 0;
                                pmedit_resize = pmedit_face = pmedit_restang = 0;
                        }
                        else pmedit_toggle();
                }
                break;
        }
        return 1; // the editor owns the rest of the keyboard
}

// turntable easing, run at the fixed 60Hz physics rate: velocity ramps
// linearly to 360 deg/s over half a second of hold, stops dead on release
void pmedit_update()
{
        if (!pmedit_on) return;
        // the flood modes belong to the paint view; leaving it (any other
        // mode, or deselecting) hands the plain brush back
        if (!pmedit_panel_on()) pmedit_flood = 0;
        float dt = 1 / 60.f;
        if (pmedit_picker) // the thumbnails share one slow turntable
        {
                pmedit_pick_spin += dt * 0.6f;
                if (pmedit_pick_spin >= TAU) pmedit_pick_spin -= TAU;
        }
        pmedit_bob_t += dt; // the parent-mode hover bob's 1Hz clock
        if (pmedit_bob_t >= 1) pmedit_bob_t -= 1;
        if (pmedit_hist_cool > 0) pmedit_hist_cool -= dt;
        pmedit_hist_save(0);
        pmedit_undo_track(dt);
        int ydir = pmedit_ka - pmedit_kd;
        int pdir = pmedit_kw - pmedit_ks;
        pmedit_yramp = ydir ? MIN(pmedit_yramp + dt, 0.5f) : 0.f;
        pmedit_pramp = pdir ? MIN(pmedit_pramp + dt, 0.5f) : 0.f;
        pmedit_myaw += ydir * (pmedit_yramp / 0.5f) * TAU * dt;
        pmedit_mpitch += pdir * (pmedit_pramp / 0.5f) * TAU * dt;
        if (pmedit_myaw >= TAU) pmedit_myaw -= TAU;
        if (pmedit_myaw < 0.f) pmedit_myaw += TAU;
        float limit = PI / 2 - PI / 180.f; // +-89 degrees
        pmedit_mpitch = CLAMP(pmedit_mpitch, -limit, limit);
}

void pmedit_click(int down)
{
        int btn = event.button.button;
        pmedit_mx = event.button.x;
        pmedit_my = event.button.y;
        if (!down)
        {
                if (btn == pmedit_paint_btn) pmedit_paint_btn = 0;
                if (btn == SDL_BUTTON_LEFT) pmedit_pal_drag = pmedit_resize_drag = 0;
                return;
        }
        if (btn != SDL_BUTTON_LEFT && btn != SDL_BUTTON_RIGHT) return;

        // the picker overlay eats every click while it's up
        if (pmedit_picker) { pmedit_pick_click(btn); return; }

        int st = pmedit_ui_state();

        // BACK / X, top-left. X (nothing selected) closes the editor; BACK
        // steps out one level - a running tool or the anim back to the piece
        // view, the plain piece view back to the whole model
        if (pmedit_in_btn(PB_BACK, pmedit_mx, pmedit_my))
        {
                if (btn != SDL_BUTTON_LEFT) return;
                if (st == PMEDIT_S_NONE) { pmedit_toggle(); return; }
                pmedit_newpart_cancel(); // an unplaced NEW PART/copy doesn't survive
                pmedit_animate = 0;
                if (st == PMEDIT_S_MODAL)
                {
                        pmedit_joint = pmedit_socket = pmedit_parent = 0;
                        pmedit_resize = pmedit_face = pmedit_restang = 0;
                        pmedit_hide = 0;
                        return;
                }
                pmedit_sel = -1; // piece view -> whole model
                return;
        }

        // LOAD opens the picker (whole-model view only)
        if (pmedit_in_btn(PB_LOAD, pmedit_mx, pmedit_my))
        {
                if (btn == SDL_BUTTON_LEFT) pmedit_picker_open();
                return;
        }

        // STYLE toggles the model's animation style; it sits above the "any
        // click stops ANIMATE" rule so you can A/B styles while it plays
        if (pmedit_in_btn(PB_STYLE, pmedit_mx, pmedit_my))
        {
                struct pmodel *mo = &pm_models[my_player];
                mo->style = mo->style == PM_STYLE_WALK ? PM_STYLE_FLAIL
                                                       : PM_STYLE_WALK;
                return;
        }

        if (pmedit_animate) { pmedit_animate = 0; return; } // any click stops it

        // HIDE (THIS PART group, a piece must be selected): with pieces
        // already hidden it's the one-click bring-them-all-back; otherwise it
        // drops into hide mode, keeping the selection, and every piece clicked
        // ghosts away until the click lands somewhere else
        if (pmedit_in_btn(PB_HIDE, pmedit_mx, pmedit_my))
        {
                if (btn != SDL_BUTTON_LEFT) return;
                if (pmedit_hidden) { pmedit_hidden = 0; return; } // UNHIDE (n)
                pmedit_hide = 1;
                return;
        }

        if (pmedit_in_btn(PB_JOINT, pmedit_mx, pmedit_my))
        {
                if (btn == SDL_BUTTON_LEFT)
                {
                        pmedit_joint = !pmedit_joint;
                        pmedit_socket = pmedit_parent = pmedit_newpart = 0;
                        pmedit_resize = pmedit_face = pmedit_restang = 0;
                }
                return;
        }
        if (pmedit_in_btn(PB_SOCKET, pmedit_mx, pmedit_my))
        {
                if (btn == SDL_BUTTON_LEFT)
                {
                        pmedit_socket = !pmedit_socket;
                        pmedit_joint = pmedit_parent = pmedit_newpart = 0;
                        pmedit_resize = pmedit_face = pmedit_restang = 0;
                }
                return;
        }
        if (pmedit_in_btn(PB_PARENT, pmedit_mx, pmedit_my))
        {
                if (btn != SDL_BUTTON_LEFT) return;
                struct pmodel *mo = &pm_models[my_player];
                struct pm_piece *pc = &mo->piece[pmedit_sel];
                if (pc->parent >= 0)
                {
                        // DETACH: re-hang the piece on the invisible player
                        // box. In the standing pose every hop is the pure
                        // translation (attach - origin), so folding the old
                        // parent chain's offsets into the attach keeps the
                        // piece visually put - unless the sum clamps at the
                        // attach range walls, then it moves as little as
                        // the range allows
                        int off[3] = { 0, 0, 0 };
                        int hops = 0;
                        for (int j = pc->parent;
                                        j >= 0 && hops++ <= PM_MAX_PIECES;
                                        j = mo->piece[j].parent)
                                for (int a = 0; a < 3; a++)
                                        off[a] += mo->piece[j].attach[a]
                                                - mo->piece[j].origin[a];
                        for (int a = 0; a < 3; a++)
                        {
                                int at = pc->attach[a] + off[a];
                                pc->attach[a] = ICLAMP(at, 0, 2 * PM_TILE);
                        }
                        pc->parent = -1;
                        pmedit_joint = pmedit_socket = 0;
                        pmedit_resize = pmedit_face = pmedit_restang = 0;
                        return;
                }
                pmedit_parent = !pmedit_parent;
                pmedit_newpart = 0;
                pmedit_joint = pmedit_socket = 0;
                pmedit_resize = pmedit_face = pmedit_restang = 0;
                return;
        }
        // PART TYPE list: click a row to set the piece's type outright
        {
                int trow = pmedit_type_row(pmedit_mx, pmedit_my);
                if (trow >= 0)
                {
                        if (btn == SDL_BUTTON_LEFT)
                                pm_models[my_player].piece[pmedit_sel].type = trow;
                        return;
                }
        }
        if (pmedit_in_btn(PB_RESIZE, pmedit_mx, pmedit_my))
        {
                if (btn == SDL_BUTTON_LEFT)
                {
                        pmedit_resize = !pmedit_resize;
                        pmedit_face = pmedit_restang = 0;
                        pmedit_joint = pmedit_socket = pmedit_parent = 0;
                        pmedit_newpart = 0;
                }
                return;
        }
        if (pmedit_in_btn(PB_RESTANG, pmedit_mx, pmedit_my))
        {
                if (btn == SDL_BUTTON_LEFT)
                {
                        pmedit_restang = !pmedit_restang;
                        pmedit_joint = pmedit_socket = pmedit_parent = 0;
                        pmedit_resize = pmedit_face = 0;
                        pmedit_newpart = 0;
                }
                return;
        }
        if (pmedit_in_btn(PB_COPY, pmedit_mx, pmedit_my))
        {
                if (btn == SDL_BUTTON_LEFT) pmedit_copy();
                return;
        }
        if (pmedit_in_btn(PB_DELETE, pmedit_mx, pmedit_my))
        {
                if (btn == SDL_BUTTON_LEFT) pmedit_delete();
                return;
        }
        if (pmedit_in_btn(PB_NEWPART, pmedit_mx, pmedit_my))
        {
                struct pmodel *mo = &pm_models[my_player];
                if (btn != SDL_BUTTON_LEFT || mo->nr_pieces >= PM_MAX_PIECES)
                        return;
                pmedit_newpart_cancel(); // starting over abandons an unplaced one
                // a fresh 4x4x4 in the middle of its space, gray
                // checkerboard coat, joint dead center - then straight into
                // the parent-picking flow to hang it somewhere
                int i = mo->nr_pieces++;
                mo->piece[i] = (struct pm_piece){
                        .dims = {4, 4, 4}, .corner = {6, 6, 6},
                        .origin = {8, 8, 8}, .attach = {8, 8, 8},
                        .parent = -1, .type = PM_T_FIXED };
                for (int f = 0; f < PM_FACES; f++)
                        for (int v = 0; v < PM_TILE; v++)
                        for (int u = 0; u < PM_TILE; u++)
                                PM_TEXSET(mo->texel[i][f], v * PM_TILE + u,
                                        (u + v) & 1 ? PMEDIT_GRAY_A
                                                    : PMEDIT_GRAY_B);
                pmodel_upload(my_player);
                pmedit_sel = i;
                pmedit_parent = 1;
                pmedit_newpart = 1;
                pmedit_joint = pmedit_socket = 0;
                pmedit_resize = pmedit_face = pmedit_restang = 0;
                pmedit_hide = 0;
                return;
        }
        if (pmedit_in_btn(PB_ANIMATE, pmedit_mx, pmedit_my))
        {
                if (btn == SDL_BUTTON_LEFT)
                {
                        pmedit_animate = 1;
                        pmedit_hide = 0;
                }
                return;
        }

        if (pmedit_hide)
        {
                // every piece clicked goes see-through and dead to clicks;
                // a click on anything else is done hiding
                int face, tu, tv;
                int hit = pmedit_pick(pmedit_mx, pmedit_my, -1, 0.f,
                                &face, &tu, &tv, NULL);
                if (hit >= 0) pmedit_hidden |= 1u << hit;
                else pmedit_hide = 0;
                return;
        }

        if (pmedit_sel < 0) // whole-model view: click selects a piece
        {
                int face, tu, tv;
                int hit = pmedit_pick(pmedit_mx, pmedit_my, -1, 0.f, &face, &tu, &tv, NULL);
                if (hit >= 0)
                {
                        pmedit_sel = hit;
                        pmedit_joint = pmedit_socket = pmedit_parent = 0;
                        pmedit_resize = pmedit_face = pmedit_restang = 0;
                }
                return;
        }

        int face, tu, tv;
        if (pmedit_parent)
        {
                // one click: pick the new parent (only the eligible pieces
                // show - loops-in-waiting are hidden) and the piece re-wires
                // AND lays itself flush against the clicked spot, exactly
                // like MOVE PART - then out to the whole model, nothing
                // selected. A far miss also backs all the way out.
                struct pm_piece *pc = &pm_models[my_player].piece[pmedit_sel];
                float h[3];
                int hit = pmedit_pick(pmedit_mx, pmedit_my,
                                PMEDIT_PICK_ELIG, 0.f,
                                &face, &tu, &tv, h);
                if (hit >= 0)
                {
                        if (btn == SDL_BUTTON_LEFT)
                        {
                                int att[3];
                                // a copy keeps its own joint (== 2)
                                if (pmedit_newpart == 1)
                                        pmedit_newpart_snap(pc, face);
                                pc->parent = hit;
                                pmedit_flush_att(pc, hit, face, h, att);
                                for (int a = 0; a < 3; a++)
                                        pc->attach[a] = att[a];
                                pmedit_parent = pmedit_newpart = 0;
                                pmedit_sel = -1;
                        }
                        return;
                }
                if (pmedit_pick(pmedit_mx, pmedit_my, PMEDIT_PICK_ELIG,
                                1.f, &face, &tu, &tv, NULL) >= 0)
                        return;
                if (pmedit_pick(pmedit_mx, pmedit_my, pmedit_sel, 1.f,
                                &face, &tu, &tv, NULL) >= 0)
                        return;
                pmedit_newpart_cancel();
                pmedit_sel = -1;
                pmedit_joint = pmedit_socket = pmedit_parent = 0;
                pmedit_resize = pmedit_face = pmedit_restang = 0;
                return;
        }
        if (pmedit_joint || pmedit_socket)
        {
                // left commits what the preview shows and drops back to the
                // plain piece view: PLACE ATTACHMENT POINT plants the origin
                // at the clicked texel's spot, MOVE PART lays the piece flush
                // against the parent right there. In PLACE ATTACHMENT POINT
                // any click off the piece also just leaves the mode; in MOVE
                // PART a real miss (off piece AND parent) backs all the way
                // out - the rest of the model is hidden in both modes, so no
                // click-to-switch here
                struct pm_piece *pc = &pm_models[my_player].piece[pmedit_sel];
                float h[3];
                int att[3];
                if (pmedit_socket && btn == SDL_BUTTON_LEFT
                                && pmedit_sock_aim(pc, att))
                {
                        for (int a = 0; a < 3; a++)
                                pc->attach[a] = att[a];
                        pmedit_socket = 0;
                        return;
                }
                if (pmedit_joint && btn == SDL_BUTTON_LEFT && pmedit_pick(pmedit_mx,
                                pmedit_my, pmedit_sel, 0.f, &face, &tu, &tv, h) >= 0)
                {
                        unsigned char *corner = pc->corner;
                        for (int a = 0; a < 3; a++)
                        {
                                // shift the attach point by the same delta so
                                // the piece itself doesn't budge
                                int o = ICLAMP((int)roundf(corner[a] + h[a]),
                                                0, PM_TILE);
                                int at = pc->attach[a] + o - pc->origin[a];
                                pc->origin[a] = o;
                                pc->attach[a] = ICLAMP(at, 0, 2 * PM_TILE);
                        }
                        pmedit_joint = 0;
                        return;
                }
                if (pmedit_joint)
                {
                        if (pmedit_pick(pmedit_mx, pmedit_my, pmedit_sel, 0.f,
                                        &face, &tu, &tv, NULL) < 0)
                                pmedit_joint = 0;
                        return;
                }
                if (pmedit_pick(pmedit_mx, pmedit_my, pmedit_sel, 1.f,
                                &face, &tu, &tv, NULL) >= 0) return;
                if (pc->parent >= 0 && pmedit_pick(pmedit_mx,
                                pmedit_my, pc->parent, 1.f, &face, &tu, &tv,
                                NULL) >= 0) return;
                pmedit_sel = -1;
                pmedit_socket = 0;
                return;
        }
        if (pmedit_resize)
        {
                // left-click a face of the selection to pick it - the arrow
                // keys then push it out / pull it in 1px, paint following. A
                // click on another piece switches to it (same mode, no face
                // picked); a near-miss does nothing; farther backs out to
                // the whole model.
                if (btn == SDL_BUTTON_LEFT && pmedit_pick(pmedit_mx, pmedit_my,
                                pmedit_sel, 0.f, &face, &tu, &tv, NULL) >= 0)
                {
                        // pick the face AND arm a drag from here: dragging
                        // pushes/pulls it, the arrow keys still work too
                        pmedit_face = face;
                        pmedit_resize_drag = 1;
                        pmedit_resize_ax = pmedit_mx;
                        pmedit_resize_ay = pmedit_my;
                        pmedit_resize_applied = 0;
                        return;
                }
                int hit = pmedit_pick(pmedit_mx, pmedit_my, -1, 0.f,
                                &face, &tu, &tv, NULL);
                if (hit >= 0 && hit != pmedit_sel)
                {
                        pmedit_sel = hit;
                        pmedit_face = 0;
                        return;
                }
                if (pmedit_pick(pmedit_mx, pmedit_my, pmedit_sel, 1.f,
                                &face, &tu, &tv, NULL) >= 0) return;
                pmedit_sel = -1;
                pmedit_resize = pmedit_face = pmedit_restang = 0;
                return;
        }
        if (pmedit_restang)
        {
                // click another piece to pose it too (same mode); a
                // near-miss does nothing; farther backs out to the whole
                // model (the angles keep - they're per piece)
                int hit = pmedit_pick(pmedit_mx, pmedit_my, -1, 0.f,
                                &face, &tu, &tv, NULL);
                if (hit >= 0) { pmedit_sel = hit; return; }
                if (pmedit_pick(pmedit_mx, pmedit_my, pmedit_sel, 1.f,
                                &face, &tu, &tv, NULL) >= 0) return;
                pmedit_sel = -1;
                pmedit_restang = 0;
                return;
        }

        // the palette panel eats every click on it: a swatch becomes the
        // paint color, the picker below recolors that swatch's slot (and
        // keeps tracking the drag), and the backdrop between them is dead -
        // no backing out of the piece view by a missed swatch
        if (pmedit_in_panel(pmedit_mx, pmedit_my))
        {
                int sw = pmedit_in_swatch(pmedit_mx, pmedit_my);
                if (sw >= 0) { pmedit_color = sw; return; }
                if (pmedit_in_flood_btn(pmedit_mx, pmedit_my))
                {
                        if (btn == SDL_BUTTON_LEFT)
                                pmedit_flood = pmedit_flood == 1 ? 0 : 1;
                        return;
                }
                if (pmedit_in_sflood_btn(pmedit_mx, pmedit_my))
                {
                        if (btn == SDL_BUTTON_LEFT)
                                pmedit_flood = pmedit_flood == 2 ? 0 : 2;
                        return;
                }
                if (btn == SDL_BUTTON_LEFT
                                && pmedit_mx >= PMEDIT_HSL_X0
                                && pmedit_mx <= PMEDIT_HSL_X1
                                && pmedit_my >= PMEDIT_HSL_Y0
                                && pmedit_my <= PMEDIT_HSL_Y1)
                {
                        pmedit_pal_drag = 1;
                        pmedit_pal_pick(pmedit_mx, pmedit_my);
                }
                return;
        }

        // right-click - or shift, ctrl, alt or cmd with any click: whatever
        // a pixel-editor reflex expects - copies the pointed-at texel's color
        // to the paint swatch instead of painting; a missed eyedrop does
        // nothing at all, so a fumbled copy can't back out of the view
        if (btn == SDL_BUTTON_RIGHT || SDL_GetModState()
                        & (SDL_KMOD_SHIFT | SDL_KMOD_CTRL | SDL_KMOD_ALT
                                | SDL_KMOD_GUI))
        {
                // eyedrop what the eye sees: this view draws the selection
                // over everything (on cleared depth), so it wins wherever
                // the ray touches it, even with another piece physically in
                // front - the honest-depth rest only count where it misses
                int hit = pmedit_pick(pmedit_mx, pmedit_my, pmedit_sel, 0.f,
                                &face, &tu, &tv, NULL);
                if (hit < 0)
                        hit = pmedit_pick(pmedit_mx, pmedit_my, -1, 0.f,
                                        &face, &tu, &tv, NULL);
                if (hit >= 0)
                        pmedit_color = PM_TEXGET(
                                pm_models[my_player].texel[hit][face - 1],
                                tv * PM_TILE + tu);
                return;
        }

        // piece view: paint on a true hit; a click that lands on ANOTHER
        // piece switches to it; a near-miss (within a texel) does nothing,
        // and anything farther backs out to the whole model. The flood
        // modes swap in for the brush - one shot per click, no drag
        if (pmedit_flood)
        {
                if (pmedit_flood_do())
                {
                        // SUPER FLOOD is a one-shot: hand the brush back after
                        // the single coat (FLOOD FILL stays on for more fills)
                        if (pmedit_flood == 2) pmedit_flood = 0;
                        return;
                }
        }
        else if (pmedit_paint()) { pmedit_paint_btn = btn; return; }
        int hit = pmedit_pick(pmedit_mx, pmedit_my, -1, 0.f, &face, &tu, &tv, NULL);
        if (hit >= 0) { pmedit_sel = hit; return; }
        if (pmedit_pick(pmedit_mx, pmedit_my, pmedit_sel, 1.f, &face, &tu, &tv, NULL) < 0)
                pmedit_sel = -1;
}

// RESIZE drag: push/pull the picked face to follow the cursor. One model px
// of face travel projects to a known screen vector (the face center vs a
// point one px out through it); the drag from the anchor, divided by that
// vector, is exactly how many px to grow (positive) or shrink. Applied
// relative to the anchor each move, so it self-corrects and never runs away.
static void pmedit_resize_drag_to(void)
{
        struct pmodel *mo = &pm_models[my_player];
        struct pm_piece *pc = &mo->piece[pmedit_sel];
        int a = pmedit_axis_of[pmedit_face], side = pmedit_side_of[pmedit_face];
        float ctr[3] = { pc->corner[0] + pc->dims[0] / 2.f,
                         pc->corner[1] + pc->dims[1] / 2.f,
                         pc->corner[2] + pc->dims[2] / 2.f };
        ctr[a] = side > 0 ? pc->corner[a] + pc->dims[a] : pc->corner[a];
        float out[3] = { ctr[0], ctr[1], ctr[2] };
        out[a] += side; // one px in the growing direction

        float w0[4], w1[4], s0x, s0y, s1x, s1y;
        mat4_f3_multiply(w0, pmedit_mats[pmedit_sel], ctr[0], ctr[1], ctr[2]);
        mat4_f3_multiply(w1, pmedit_mats[pmedit_sel], out[0], out[1], out[2]);
        if (!pmedit_to_screen(w0, &s0x, &s0y)
                        || !pmedit_to_screen(w1, &s1x, &s1y))
                return;
        float sn[2] = { s1x - s0x, s1y - s0y };
        float len2 = sn[0] * sn[0] + sn[1] * sn[1];
        if (len2 < 1e-3f) return; // face edge-on: nothing to grab

        float drag[2] = { pmedit_mx - pmedit_resize_ax,
                          pmedit_my - pmedit_resize_ay };
        int target = (int)roundf((drag[0] * sn[0] + drag[1] * sn[1]) / len2);
        int changed = 0;
        while (pmedit_resize_applied < target)
        {
                if (!pm_piece_resize(mo, pmedit_sel, pmedit_face, 1)) break;
                pmedit_resize_applied++;
                changed = 1;
        }
        while (pmedit_resize_applied > target)
        {
                if (!pm_piece_resize(mo, pmedit_sel, pmedit_face, 0)) break;
                pmedit_resize_applied--;
                changed = 1;
        }
        if (changed) pmodel_upload(my_player);
}

void pmedit_motion()
{
        pmedit_mx = event.motion.x;
        pmedit_my = event.motion.y;
        if (pmedit_pal_drag) { pmedit_pal_pick(pmedit_mx, pmedit_my); return; }
        if (pmedit_resize_drag && pmedit_resize && pmedit_face && pmedit_sel >= 0)
        {
                pmedit_resize_drag_to();
                return;
        }
        if (pmedit_paint_btn) pmedit_paint(); // drag to paint
}

#endif // BLOCKO_PMEDIT_INPUT_C_INCLUDED

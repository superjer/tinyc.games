#ifndef BLOCKO_PMEDIT_C_INCLUDED
#define BLOCKO_PMEDIT_C_INCLUDED
#include "blocko.c"

// In-game player model editor (U key). The model floats in front of the
// frozen camera in a plain standing pose; WASD spins it like a turntable
// (easing to 360 deg/s over half a second). Clicking a prism selects it: the
// view eases to center on that prism (zoomed to fit), it gets a white outline
// while the rest of the model draws full-color behind it (Z-tested, so colors
// stay matchable), and clicks paint the targeted pixel - left red, right
// blue, from two reserved palette slots. The JOINT button switches the piece
// view to editing the rotation origin instead, and SOCKET to editing the
// attach point in the parent's space (the parent joins the top depth layer
// there): the point shows as a color-cycling cube with axis lines through it
// (depth-tested, so the surface intersections read exactly), left-click
// plants it on the clicked texel (JOINT: on the piece; SOCKET: on the
// parent), and the arrow keys/Space/LShift nudge it a px at a time
// (camera-relative; repeats work; WASD still rotates the turntable). JOINT
// never touches the parent or its attach point, SOCKET never touches the
// origin. PARENT mode re-wires the hierarchy: the selection drops BEHIND
// everything, and clicking any other piece makes it the parent with the
// click as the new attach point (clicks that would loop the chain are
// ignored). The TYPE button shows the selected piece's animation type (HEAD,
// LEFT ARM, ...) - left-click cycles forward, right-click back. The ANIMATE
// button plays the model's animation (walking in place)
// on the whole model - WASD still spins it, the piece buttons grey out, any
// click (or ESC) stops it right back where you were, and the zoom stays fit
// to the standing pose so it doesn't pump. The STYLE button below it flips
// the model between WALK and FLAIL animation styles (a model property that
// travels with it over the net) and is click-exempt so you can A/B styles
// while ANIMATE plays. Clicking off
// the piece or ESC goes back to the whole model; U closes the editor, saves
// model.dat and announces the new look over the net.

#define PMEDIT_RED  254 // reserved palette slots for the two paint colors
#define PMEDIT_BLUE 255

static int pmedit_sel = -1;              // selected piece, -1 = whole model
static int pmedit_joint;                 // JOINT mode: edit the rotation origin
static int pmedit_socket;                // SOCKET mode: edit the parent attach point
static int pmedit_parent;                // PARENT mode: click a piece to re-parent to
static int pmedit_animate;               // ANIMATE mode: play the placeholder anim
static float pmedit_gizmo_mat[16];       // the active gizmo's px -> world frame
static float pmedit_myaw, pmedit_mpitch; // turntable angles
static float pmedit_yramp, pmedit_pramp; // seconds each axis key is held, <= 1
static int pmedit_kw, pmedit_ka, pmedit_ks, pmedit_kd;
static float pmedit_cen[3];              // eased center of interest (model frame)
static float pmedit_dist;                // eased eye-to-center distance
static int pmedit_snap;                  // skip easing on the first frame
static float pmedit_mats[PM_MAX_PIECES][16]; // px->world per piece, for picking
static int pmedit_nr;                    // pieces in pmedit_mats
static int pmedit_paint_btn;             // mouse button held down painting, or 0
static float pmedit_mx, pmedit_my;       // latest cursor position
static int pm_edit_rest_start, pm_edit_rest_count;   // instance ranges in
static int pm_edit_hull_start, pm_edit_hull_count;   // pmbuf: unselected
static int pm_edit_sel_start, pm_edit_sel_count;     // pieces, outline hull,
static int pm_edit_joint_start, pm_edit_joint_count; // selection, joint gizmo

static int pmedit_pick(float mx, float my, int only, float pad,
                int *face, int *tu, int *tv, float *hitp);

// rotation about an arbitrary unit axis (Rodrigues), column-major like vector.c
static void pm_mat_axis(float *m, float x, float y, float z, float a)
{
        float c = cosf(a), s = sinf(a), ic = 1 - c;
        pm_mat_ident(m);
        m[0] = c + x*x*ic;   m[4] = x*y*ic - z*s; m[8]  = x*z*ic + y*s;
        m[1] = y*x*ic + z*s; m[5] = c + y*y*ic;   m[9]  = y*z*ic - x*s;
        m[2] = z*x*ic - y*s; m[6] = z*y*ic + x*s; m[10] = c + z*z*ic;
}

// the button column: labels left-align at PMEDIT_BTN_X, hit boxes span it
#define PMEDIT_BTN_X (screenw - 330)

static int pmedit_in_joint_btn(float x, float y)
{
        return pmedit_sel >= 0 &&
                x >= PMEDIT_BTN_X - 10 && x <= screenw - 8 && y >= 16 && y <= 76;
}

static int pmedit_in_socket_btn(float x, float y)
{
        return pmedit_sel >= 0 &&
                x >= PMEDIT_BTN_X - 10 && x <= screenw - 8 && y >= 80 && y <= 140;
}

static int pmedit_in_parent_btn(float x, float y)
{
        return pmedit_sel >= 0 &&
                x >= PMEDIT_BTN_X - 10 && x <= screenw - 8 && y >= 144 && y <= 204;
}

static int pmedit_in_type_btn(float x, float y)
{
        return pmedit_sel >= 0 &&
                x >= PMEDIT_BTN_X - 10 && x <= screenw - 8 && y >= 208 && y <= 268;
}

static int pmedit_in_animate_btn(float x, float y)
{
        return x >= PMEDIT_BTN_X - 10 && x <= screenw - 8 && y >= 272 && y <= 332;
}

static int pmedit_in_style_btn(float x, float y)
{
        return x >= PMEDIT_BTN_X - 10 && x <= screenw - 8 && y >= 336 && y <= 396;
}

static const char *pmedit_type_name[PM_T_COUNT] = {
        [PM_T_FIXED] = "FIXED",   [PM_T_TORSO] = "TORSO",
        [PM_T_HEAD]  = "HEAD",    [PM_T_ARM_L] = "LEFT ARM",
        [PM_T_ARM_R] = "RIGHT ARM", [PM_T_LEG1] = "LEG 1",
        [PM_T_LEG2]  = "LEG 2",   [PM_T_TAIL]  = "TAIL",
        [PM_T_JIGGLE] = "JIGGLE", [PM_T_EYES]  = "EYES",
};

// would re-parenting the selection to piece j loop the chain?
static int pmedit_cycle(int j)
{
        int hops = 0;
        while (j >= 0 && hops++ <= PM_MAX_PIECES)
        {
                if (j == pmedit_sel) return 1;
                j = pm_models[my_player].piece[j].parent;
        }
        return 0;
}

void pmedit_toggle()
{
        pmedit_on = !pmedit_on;
        struct player *pl = &player[my_player];
        if (pmedit_on)
        {
                pmedit_sel = -1;
                pmedit_joint = pmedit_socket = pmedit_parent = pmedit_animate = 0;
                pmedit_myaw = pmedit_mpitch = 0;
                pmedit_yramp = pmedit_pramp = 0;
                pmedit_kw = pmedit_ka = pmedit_ks = pmedit_kd = 0;
                pmedit_paint_btn = 0;
                pmedit_snap = 1;
                // the paint colors live in reserved palette slots
                pm_models[my_player].palette[PMEDIT_RED]  = PM_RGB(220, 40, 40);
                pm_models[my_player].palette[PMEDIT_BLUE] = PM_RGB(45, 80, 230);
                // free the cursor; stop any in-flight movement and mining
                pl->goingf = pl->goingb = pl->goingl = pl->goingr = 0;
                pl->breaking = pl->building = pl->running = pl->sneaking = 0;
                zooming = 0;
                SDL_SetWindowRelativeMouseMode(vk.window, false);
                mouselook = false;
        }
        else
        {
                pmodel_save();
                pmodel_send_mine();
                SDL_SetWindowRelativeMouseMode(vk.window, true);
                mouselook = true;
        }
}

// JOINT/SOCKET mode: nudge the active point one px along the gizmo-frame
// axis that best matches a camera-relative direction - Up/Down arrows
// vertically, Left/Right arrows sideways; rotate the turntable (WASD still
// works) to reach the other horizontal axis. Directions are what the arrows
// do to the PIECE on screen: the origin is a pivot pinned to the parent, so
// JOINT steps it the opposite way; the attach point carries the piece with
// it, so SOCKET steps it straight.
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
        // (uniform scale doesn't matter for picking the dominant axis)
        float *M = pmedit_gizmo_mat;
        float dl[3];
        for (int a = 0; a < 3; a++)
                dl[a] = M[4*a] * d[0] + M[4*a+1] * d[1] + M[4*a+2] * d[2];
        int ax = 0;
        if (fabsf(dl[1]) > fabsf(dl[ax])) ax = 1;
        if (fabsf(dl[2]) > fabsf(dl[ax])) ax = 2;

        struct pm_piece *pc = &pm_models[my_player].piece[pmedit_sel];
        int step = dl[ax] > 0 ? 1 : -1;
        if (pmedit_joint) step = -step; // the piece follows the arrows
        unsigned char *pt = pmedit_joint ? pc->origin : pc->attach;
        pt[ax] = ICLAMP(pt[ax] + step, 0, PM_TILE);
}

// keyboard while the editor owns it. Returns 1 when the key was consumed.
// (Called before key_move's repeat gate: repeats drive the joint nudge.)
int pmedit_key(int down)
{
        int k = event.key.key;
        int joint_move_key = k == SDLK_UP || k == SDLK_DOWN ||
                k == SDLK_LEFT || k == SDLK_RIGHT;

        int gizmo_mode = pmedit_joint || pmedit_socket;
        if (event.key.repeat)
        {
                if (pmedit_on && gizmo_mode && pmedit_sel >= 0 && down && joint_move_key)
                        pmedit_joint_move(k);
                return pmedit_on; // repeats do nothing else while editing
        }
        if (k == SDLK_U) { if (down) pmedit_toggle(); return 1; }
        if (!pmedit_on) return 0;
        if (k >= SDLK_F1 && k <= SDLK_F12) return 0; // debug keys still work

        if (gizmo_mode && pmedit_sel >= 0 && joint_move_key)
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
        case SDLK_ESCAPE:
                if (down)
                {
                        if (pmedit_animate) pmedit_animate = 0;
                        else if (pmedit_sel >= 0)
                        {
                                pmedit_sel = -1;
                                pmedit_joint = pmedit_socket = pmedit_parent = 0;
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
        float dt = 1 / 60.f;
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

// build the preview's instances into pmbuf: unselected pieces, then the white
// outline hull, then the selected piece (each group drawn onto freshly
// cleared depth, so the selection always reads in full, in front). Also
// refreshes the pick matrices and eases the center/zoom toward the selection.
struct pmvert *pmedit_emit(struct pmvert *b)
{
        struct pmodel *mo = &pm_models[my_player];
        float space[PM_MAX_PIECES][16], geom[PM_MAX_PIECES][16], root[16];

        // standing pose in the model frame: hitbox center at the origin,
        // facing the camera; NULL context = no animation. ANIMATE mode feeds
        // a synthetic walking-in-place context (full speed, phase advancing)
        // so the WALK style previews too, not just the FLAIL clock.
        static float anim_t;
        struct pm_anim an;
        if (pmedit_animate)
        {
                anim_t += 0.05f;
                an = (struct pm_anim){
                        .walk_phase = anim_t * 4.f,
                        .speed = 1,
                        // gentle look sweep so HEAD tracking shows off
                        .look_yaw = sinf(anim_t * 0.5f) * 0.8f,
                        .look_pitch = sinf(anim_t * 0.3f) * 0.35f,
                        // canned wobble so JIGGLE pieces preview too
                        .bounce = sinf(anim_t * 2.5f) * 0.3f,
                        .t = anim_t,
                        .style = mo->style,
                };
        }
        pm_resolve(mo, -PLYR_W / 2.f, -PLYR_H / 2.f, -PLYR_W / 2.f,
                        -camplayer.yaw, pmedit_animate ? &an : NULL,
                        space, geom, root);
        pmedit_nr = mo->nr_pieces;
        if (pmedit_sel >= pmedit_nr) pmedit_sel = -1;

        // frame the STANDING pose even while animating, so the zoom-to-fit
        // doesn't pump with the swinging limbs
        float sspace[PM_MAX_PIECES][16], sgeom[PM_MAX_PIECES][16];
        float (*bgeom)[16] = geom;
        if (pmedit_animate)
        {
                pm_resolve(mo, -PLYR_W / 2.f, -PLYR_H / 2.f, -PLYR_W / 2.f,
                                -camplayer.yaw, NULL, sspace, sgeom, NULL);
                bgeom = sgeom;
        }

        // center of interest + radius: the selected prism's bounding box
        // (center = its corner average), plus its parent's in SOCKET mode,
        // or the whole model's
        float lo[3] = {1e9f, 1e9f, 1e9f}, hi[3] = {-1e9f, -1e9f, -1e9f};
        int spar = pmedit_socket && pmedit_sel >= 0 ? mo->piece[pmedit_sel].parent : -1;
        for (int i = 0; i < pmedit_nr; i++)
        {
                // PARENT mode frames the whole model (any piece is
                // clickable); so does ANIMATE (the whole model moves)
                if (pmedit_sel >= 0 && !pmedit_parent && !pmedit_animate
                                && i != pmedit_sel && i != spar)
                        continue;
                for (int k = 0; k < 8; k++)
                {
                        float p[4];
                        mat4_f3_multiply(p, bgeom[i],
                                (k      & 1) * mo->piece[i].dims[0],
                                (k >> 1 & 1) * mo->piece[i].dims[1],
                                (k >> 2 & 1) * mo->piece[i].dims[2]);
                        for (int a = 0; a < 3; a++)
                        {
                                if (p[a] < lo[a]) lo[a] = p[a];
                                if (p[a] > hi[a]) hi[a] = p[a];
                        }
                }
        }
        float cen[3] = { (lo[0]+hi[0])/2, (lo[1]+hi[1])/2, (lo[2]+hi[2])/2 };
        float radius = 0.5f * sqrtf((hi[0]-lo[0]) * (hi[0]-lo[0])
                                  + (hi[1]-lo[1]) * (hi[1]-lo[1])
                                  + (hi[2]-lo[2]) * (hi[2]-lo[2]));

        // zoom to fit: recover tan(half-fov) from the projection last drawn
        float tanw = main_ubo.proj[0] ? 1.f / main_ubo.proj[0] : 1.f;
        float tanh_ = main_ubo.proj[5] ? -1.f / main_ubo.proj[5] : 1.f;
        float fill = pmedit_sel < 0 || pmedit_parent || pmedit_animate
                        ? 0.65f : 0.6f; // radius / half-extent
        float dist = radius / (fill * MIN(tanw, tanh_));
        if (dist < radius + 0.3f * BS) dist = radius + 0.3f * BS; // keep off the near plane

        if (pmedit_snap)
        {
                memcpy(pmedit_cen, cen, sizeof cen);
                pmedit_dist = dist;
                pmedit_snap = 0;
        }
        else
        {
                // fly to a selected piece fast; drift back out gently
                float ease = pmedit_sel < 0 ? 0.2f : 0.6f;
                for (int a = 0; a < 3; a++)
                        pmedit_cen[a] += (cen[a] - pmedit_cen[a]) * ease;
                pmedit_dist += (dist - pmedit_dist) * ease;
        }

        // the editor's own camera: the eye, leveled (pitch 0), so the model
        // always opens dead ahead at eye level and the turntable's +-89 deg
        // means what it says - no matter where the player was looking
        float f[3], vm[16];
        lookit(vm, f, 0, 0, 0, 0, camplayer.yaw);

        // whole-preview transform: put the eased center pmedit_dist in front
        // of the eye, turntable-rotated about it (pitch spins on screen-right)
        float R[16], t1[16], t2[16], E[16];
        pm_mat_axis(t1, vm[0], vm[4], vm[8], pmedit_mpitch);
        pm_mat_yaw(t2, pmedit_myaw);
        mat4_multiply(R, t1, t2);
        pm_mat_translate(t1, -pmedit_cen[0], -pmedit_cen[1], -pmedit_cen[2]);
        mat4_multiply(t2, R, t1);
        pm_mat_translate(t1, peye0 + f[0] * pmedit_dist,
                             peye1 + f[1] * pmedit_dist,
                             peye2 + f[2] * pmedit_dist);
        mat4_multiply(E, t1, t2);

        for (int i = 0; i < pmedit_nr; i++)
                mat4_multiply(pmedit_mats[i], E, geom[i]);

        // one prism face instance; every piece draws at full, glow-heavy
        // brightness so paint colors read the same day or night, and
        // unselected pieces stay color-matchable against the selection
        #define PM_EDIT_FACE(M, dx, dy, dz, layer, il, gl) (struct pmvert){ \
                .r0 = { (M)[0], (M)[4], (M)[8],  (M)[12] },                 \
                .r1 = { (M)[1], (M)[5], (M)[9],  (M)[13] },                 \
                .r2 = { (M)[2], (M)[6], (M)[10], (M)[14] },                 \
                .dims = { dx, dy, dz }, .orient = fc + 1,                   \
                .tex = (layer), .illum = (il), .glow = (gl) }

        // buffer order = draw order: unselected pieces, outline hull, selection
        pm_edit_rest_start = b - pmbuf;
        for (int i = 0; i < pmedit_nr; i++)
        {
                if (i == pmedit_sel) continue;
                // the ANIMATE preview blinks its EYES pieces like the game
                // does (the selected piece stays visible - you're editing it)
                if (pmedit_animate && mo->piece[i].type == PM_T_EYES
                                && pm_blinking(my_player, anim_t))
                        continue;
                for (int fc = 0; fc < PM_FACES; fc++)
                        *b++ = PM_EDIT_FACE(pmedit_mats[i],
                                mo->piece[i].dims[0], mo->piece[i].dims[1], mo->piece[i].dims[2],
                                pmodel_tex_base + my_player * pm_slot_layers() + i * PM_FACES + fc,
                                0.9f, 0.8f);
        }
        pm_edit_rest_count = (int)(b - pmbuf) - pm_edit_rest_start;

        // selection outline: the selected prism again, inflated by a pixel
        // membrane, in solid white (the extra layer past every slot's tiles).
        // It draws onto freshly cleared depth and the piece draws over it, so
        // the fatter silhouette shows as a rim around the piece.
        pm_edit_hull_start = pm_edit_rest_start + pm_edit_rest_count;
        pm_edit_hull_count = 0;
        pm_edit_sel_count = 0;
        if (pmedit_sel >= 0)
        {
                struct pm_piece *pc = &mo->piece[pmedit_sel];
                float e = 0.15f; // px of rim on every side - thin, so the shell
                                 // doesn't cover neighbors where pieces touch
                float t1[16], H[16];
                pm_mat_translate(t1, -e, -e, -e);
                mat4_multiply(H, pmedit_mats[pmedit_sel], t1);
                for (int fc = 0; fc < PM_FACES; fc++)
                        *b++ = PM_EDIT_FACE(H,
                                pc->dims[0] + 2*e, pc->dims[1] + 2*e, pc->dims[2] + 2*e,
                                PM_LAYER_WHITE, 0.f, 3.f); // saturates: reads white
                pm_edit_hull_count = PM_FACES;

                for (int fc = 0; fc < PM_FACES; fc++)
                        *b++ = PM_EDIT_FACE(pmedit_mats[pmedit_sel],
                                pc->dims[0], pc->dims[1], pc->dims[2],
                                pmodel_tex_base + my_player * pm_slot_layers()
                                        + pmedit_sel * PM_FACES + fc,
                                0.9f, 0.8f);
                pm_edit_sel_count = PM_FACES;

                // SOCKET mode: the parent joins the top depth layer, so the
                // gizmo below intersects ITS surface honestly too
                int par = pc->parent;
                if (pmedit_socket && par >= 0)
                {
                        struct pm_piece *pp = &mo->piece[par];
                        for (int fc = 0; fc < PM_FACES; fc++)
                                *b++ = PM_EDIT_FACE(pmedit_mats[par],
                                        pp->dims[0], pp->dims[1], pp->dims[2],
                                        pmodel_tex_base + my_player * pm_slot_layers()
                                                + par * PM_FACES + fc,
                                        0.9f, 0.8f);
                        pm_edit_sel_count += PM_FACES;
                }
        }
        pm_edit_sel_start = pm_edit_hull_start + pm_edit_hull_count;

        // JOINT/SOCKET gizmo: the rotation origin (in the piece's 16^3 SPACE
        // - it isn't corner-relative like the prism geometry) or the attach
        // point (in the PARENT's space; parent -1 = the center box) as a
        // texel-and-a-bit cube in a cycling hue, with counter-cycling axis
        // lines running out of its center in all 6 directions
        pm_edit_joint_start = pm_edit_sel_start + pm_edit_sel_count;
        pm_edit_joint_count = 0;
        if (pmedit_sel >= 0 && (pmedit_joint || pmedit_socket))
        {
                struct pm_piece *pc = &mo->piece[pmedit_sel];
                float t1[16], G[16];
                if (pmedit_joint)
                        mat4_multiply(pmedit_gizmo_mat, E, space[pmedit_sel]);
                else if (pc->parent >= 0)
                        mat4_multiply(pmedit_gizmo_mat, E, space[(int)pc->parent]);
                else
                        mat4_multiply(pmedit_gizmo_mat, E, root);
                unsigned char *pt = pmedit_joint ? pc->origin : pc->attach;
                int hue = pframe / 10 % PM_NR_HUES;

                float c = 1.3f;
                pm_mat_translate(t1, pt[0] - c/2, pt[1] - c/2, pt[2] - c/2);
                mat4_multiply(G, pmedit_gizmo_mat, t1);
                for (int fc = 0; fc < PM_FACES; fc++)
                        *b++ = PM_EDIT_FACE(G, c, c, c, PM_LAYER_HUES + hue, 0.f, 3.f);

                for (int a = 0; a < 3; a++)
                {
                        float dims[3] = { 0.2f, 0.2f, 0.2f };
                        dims[a] = 40.f;
                        pm_mat_translate(t1, pt[0] - dims[0]/2,
                                        pt[1] - dims[1]/2,
                                        pt[2] - dims[2]/2);
                        mat4_multiply(G, pmedit_gizmo_mat, t1);
                        for (int fc = 0; fc < PM_FACES; fc++)
                                *b++ = PM_EDIT_FACE(G, dims[0], dims[1], dims[2],
                                        PM_LAYER_HUES + (hue + 3) % PM_NR_HUES, 0.f, 3.f);
                }
                pm_edit_joint_count = 4 * PM_FACES;

                // ghost cube (no axis lines, dimmer) where pointing at the
                // relevant piece would plant the point on click
                int gface, gtu, gtv, tgt = pmedit_joint ? pmedit_sel : pc->parent;
                float gh[3];
                if (tgt >= 0 && pmedit_pick(pmedit_mx, pmedit_my, tgt, 0.f,
                                &gface, &gtu, &gtv, gh) >= 0)
                {
                        unsigned char *corner = mo->piece[tgt].corner;
                        int gp[3], same = 1;
                        for (int a = 0; a < 3; a++)
                        {
                                gp[a] = ICLAMP((int)roundf(corner[a] + gh[a]), 0, PM_TILE);
                                same &= gp[a] == pt[a];
                        }
                        if (!same) // identical cubes would just z-fight
                        {
                                pm_mat_translate(t1, gp[0] - c/2, gp[1] - c/2, gp[2] - c/2);
                                mat4_multiply(G, pmedit_gizmo_mat, t1);
                                for (int fc = 0; fc < PM_FACES; fc++)
                                        *b++ = PM_EDIT_FACE(G, c, c, c,
                                                PM_LAYER_HUES + hue, 0.f, 1.1f);
                                pm_edit_joint_count += PM_FACES;
                        }
                }
        }

        // PARENT mode: ghost gizmo where the attach point would land on the
        // hovered candidate (rays pass through the selection - it draws
        // behind everything here; loops-in-waiting get no ghost)
        if (pmedit_sel >= 0 && pmedit_parent)
        {
                int gface, gtu, gtv;
                float gh[3];
                int hit = pmedit_pick(pmedit_mx, pmedit_my, -2 - pmedit_sel,
                                0.f, &gface, &gtu, &gtv, gh);
                if (hit >= 0 && !pmedit_cycle(hit))
                {
                        unsigned char *corner = mo->piece[hit].corner;
                        float t1[16], F[16], G[16], c = 1.3f;
                        int gp[3];
                        for (int a = 0; a < 3; a++)
                                gp[a] = ICLAMP((int)roundf(corner[a] + gh[a]), 0, PM_TILE);
                        mat4_multiply(F, E, space[hit]);
                        pm_mat_translate(t1, gp[0] - c/2, gp[1] - c/2, gp[2] - c/2);
                        mat4_multiply(G, F, t1);
                        for (int fc = 0; fc < PM_FACES; fc++)
                                *b++ = PM_EDIT_FACE(G, c, c, c,
                                        PM_LAYER_HUES + pframe / 10 % PM_NR_HUES,
                                        0.f, 1.1f);
                        pm_edit_joint_count += PM_FACES;
                }
        }
        #undef PM_EDIT_FACE

        polys += b - pmbuf - pm_edit_rest_start;
        return b;
}

// draw the preview over the finished world: clear depth so it always wins,
// the unselected pieces first, then (on cleared depth each) the white outline
// hull and the selected piece over it, so the selection always reads in full
void pmedit_render(VkCommandBuffer cmdbuf)
{
        if (!pmedit_on || !(pm_edit_rest_count + pm_edit_sel_count)) return;

        // the preview renders through the editor's leveled camera, not the
        // world's (which keeps whatever pitch the player froze it at)
        float f[3], pv[16], view[16];
        lookit(view, f, peye0, peye1, peye2, 0, camplayer.yaw);
        translate(view, -peye0, -peye1, -peye2);
        mat4_multiply(pv, main_ubo.proj, view);

        VkClearAttachment ca = { .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                .clearValue.depthStencil = {1.f, 0} };
        VkClearRect cr = { .rect = {{0, 0},
                {vk.bestSwapchainExtent.width, vk.bestSwapchainExtent.height}},
                .baseArrayLayer = 0, .layerCount = 1 };

        vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS,
                        vk.pipelines[pmodel_pipe].pipeline);
        struct { float pv[16]; } push;
        memcpy(push.pv, pv, sizeof push.pv);
        vkCmdPushConstants(cmdbuf, vk.pipelines[pmodel_pipe].layout,
                        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                        0, sizeof push, &push);
        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(cmdbuf, 0, 1, &pm_alloc[vk.currentFrame].buf, &offset);

        // PARENT mode draws the selection FIRST so every other piece (the
        // parent candidates) reads in front of it; normally the selection
        // draws last, on top
        int layers[3][2] = {
                { pm_edit_rest_start, pm_edit_rest_count },
                { pm_edit_hull_start, pm_edit_hull_count },
                { pm_edit_sel_start,  pm_edit_sel_count  },
        };
        int order[3];
        if (pmedit_parent) { order[0] = 1; order[1] = 2; order[2] = 0; }
        else               { order[0] = 0; order[1] = 1; order[2] = 2; }

        for (int i = 0; i < 3; i++)
        {
                if (!layers[order[i]][1]) continue;
                vkCmdClearAttachments(cmdbuf, 1, &ca, 1, &cr);
                vkCmdDraw(cmdbuf, 4, layers[order[i]][1], 0, layers[order[i]][0]);
        }
        // the gizmo shares the last layer's depth: where the cube and axis
        // lines pierce the piece's surface shows exactly, no x-ray
        if (pm_edit_joint_count)
                vkCmdDraw(cmdbuf, 4, pm_edit_joint_count, 0, pm_edit_joint_start);
}

// cast the cursor's ray against the preview prisms. Returns the piece hit
// (or -1), plus which face (orient code) and the texel under the cursor.
// `only` restricts the test to one piece (painting), -1 tests all (selecting),
// -2 - i tests all EXCEPT piece i (PARENT mode: the selection draws behind
// everything, so rays pass through it to the candidates you actually see).
// `pad` fattens the prisms by that many px - it's the miss tolerance that
// keeps a near-miss click from backing out of the piece view. Paint rays use
// pad 0: a padded hit lands on the inflated surface, whose texel can be a
// pixel off from the real face under the cursor. `hitp`, when non-NULL, gets
// the hit point in the prism's local px space (0..dims from its min corner).
static int pmedit_pick(float mx, float my, int only, float pad,
                int *face, int *tu, int *tv, float *hitp)
{
        if (!main_ubo.proj[0] || !main_ubo.proj[5]) return -1;
        struct pmodel *mo = &pm_models[my_player];

        // cursor -> world ray, through the editor's leveled frustum
        float f[3], vm[16];
        lookit(vm, f, 0, 0, 0, 0, camplayer.yaw);
        float kx = (2.f * mx / screenw - 1.f) / main_ubo.proj[0];
        float ky = (2.f * my / screenh - 1.f) / main_ubo.proj[5];
        float D[3] = { f[0] + vm[0] * kx + vm[1] * ky,
                       f[1] + vm[4] * kx + vm[5] * ky,
                       f[2] + vm[8] * kx + vm[9] * ky };
        float O[3] = { peye0, peye1, peye2 };

        int best = -1;
        float best_t = 1e30f;
        int i0 = only < 0 ? 0 : only;
        int i1 = only < 0 ? pmedit_nr - 1 : only;
        int skip = only < -1 ? -2 - only : -1; // only = -2-i: all EXCEPT i
        for (int i = i0; i <= i1; i++)
        {
                if (i == skip) continue;
                float *M = pmedit_mats[i];
                // upper 3x3 is rotation * uniform scale: inverse = transpose/s^2
                float s2 = M[0]*M[0] + M[1]*M[1] + M[2]*M[2];
                if (!s2) continue;
                float w[3] = { O[0] - M[12], O[1] - M[13], O[2] - M[14] };
                float Ol[3], Dl[3];
                for (int a = 0; a < 3; a++)
                {
                        Ol[a] = (M[4*a]*w[0] + M[4*a+1]*w[1] + M[4*a+2]*w[2]) / s2;
                        Dl[a] = (M[4*a]*D[0] + M[4*a+1]*D[1] + M[4*a+2]*D[2]) / s2;
                }

                // slab test against the prism [0, dims], fattened by pad
                float tmin = -1e30f, tmax = 1e30f;
                int ax = -1, sgn = 0, miss = 0;
                for (int a = 0; a < 3; a++)
                {
                        float size = mo->piece[i].dims[a];
                        if (fabsf(Dl[a]) < 1e-9f)
                        {
                                if (Ol[a] < -pad || Ol[a] > size + pad) { miss = 1; break; }
                                continue;
                        }
                        float t0 = (-pad - Ol[a]) / Dl[a];
                        float t1 = (size + pad - Ol[a]) / Dl[a];
                        int s = Dl[a] > 0 ? 1 : -1;
                        if (t0 > t1) { float sw = t0; t0 = t1; t1 = sw; }
                        if (t0 > tmin) { tmin = t0; ax = a; sgn = s; }
                        if (t1 < tmax) tmax = t1;
                }
                if (miss || tmin > tmax || tmin < 0 || ax < 0 || tmin >= best_t)
                        continue;

                // entry axis + ray direction give the face (orient code)
                static const int face_of[3][2] = {  // [axis][dir > 0]
                        { EAST, WEST }, { DOWN, UP }, { NORTH, SOUTH } };
                int fc = face_of[ax][sgn > 0];

                // hit point -> texel, inverting pmodel.vert's uv mapping
                float h[3] = { Ol[0] + tmin * Dl[0], Ol[1] + tmin * Dl[1],
                               Ol[2] + tmin * Dl[2] };
                float sx = mo->piece[i].dims[0], sz = mo->piece[i].dims[2];
                float u, v;
                switch (fc) {
                case UP:    u = sx - h[0]; v = h[2]; break;
                case DOWN:  u = h[0];      v = h[2]; break;
                case EAST:  u = h[2];      v = h[1]; break;
                case WEST:  u = sz - h[2]; v = h[1]; break;
                case NORTH: u = sx - h[0]; v = h[1]; break;
                default:    u = h[0];      v = h[1]; break; // SOUTH
                }
                int eu, ev;
                pm_face_extent(&mo->piece[i], fc, &eu, &ev);
                best = i;
                best_t = tmin;
                *face = fc;
                *tu = ICLAMP((int)u, 0, eu - 1);
                *tv = ICLAMP((int)v, 0, ev - 1);
                if (hitp) { hitp[0] = h[0]; hitp[1] = h[1]; hitp[2] = h[2]; }
        }
        return best;
}

// paint the texel under the cursor; only exact hits on the piece's real
// surface paint. Returns whether the ray hit.
static int pmedit_paint(int btn)
{
        int face, tu, tv;
        if (pmedit_sel < 0) return 0;
        if (pmedit_pick(pmedit_mx, pmedit_my, pmedit_sel, 0.f, &face, &tu, &tv, NULL) < 0)
                return 0;
        unsigned char c = btn == SDL_BUTTON_LEFT ? PMEDIT_RED : PMEDIT_BLUE;
        unsigned char *t = &pm_models[my_player].texel[pmedit_sel][face - 1][tv * PM_TILE + tu];
        if (*t != c)
        {
                *t = c;
                pmodel_upload(my_player);
        }
        return 1;
}

void pmedit_click(int down)
{
        int btn = event.button.button;
        pmedit_mx = event.button.x;
        pmedit_my = event.button.y;
        if (!down)
        {
                if (btn == pmedit_paint_btn) pmedit_paint_btn = 0;
                return;
        }
        if (btn != SDL_BUTTON_LEFT && btn != SDL_BUTTON_RIGHT) return;

        // STYLE toggles the model's animation style; it sits above the "any
        // click stops ANIMATE" rule so you can A/B styles while it plays
        if (pmedit_in_style_btn(pmedit_mx, pmedit_my))
        {
                struct pmodel *mo = &pm_models[my_player];
                mo->style = mo->style == PM_STYLE_WALK ? PM_STYLE_FLAIL
                                                       : PM_STYLE_WALK;
                return;
        }

        if (pmedit_animate) { pmedit_animate = 0; return; } // any click stops it

        if (pmedit_in_joint_btn(pmedit_mx, pmedit_my))
        {
                if (btn == SDL_BUTTON_LEFT)
                {
                        pmedit_joint = !pmedit_joint;
                        pmedit_socket = pmedit_parent = 0;
                }
                return;
        }
        if (pmedit_in_socket_btn(pmedit_mx, pmedit_my))
        {
                if (btn == SDL_BUTTON_LEFT)
                {
                        pmedit_socket = !pmedit_socket;
                        pmedit_joint = pmedit_parent = 0;
                }
                return;
        }
        if (pmedit_in_parent_btn(pmedit_mx, pmedit_my))
        {
                if (btn == SDL_BUTTON_LEFT)
                {
                        pmedit_parent = !pmedit_parent;
                        pmedit_joint = pmedit_socket = 0;
                }
                return;
        }
        if (pmedit_in_type_btn(pmedit_mx, pmedit_my))
        {
                // left cycles forward, right cycles back
                unsigned char *ty = &pm_models[my_player].piece[pmedit_sel].type;
                *ty = (*ty + (btn == SDL_BUTTON_LEFT ? 1 : PM_T_COUNT - 1))
                                % PM_T_COUNT;
                return;
        }
        if (pmedit_in_animate_btn(pmedit_mx, pmedit_my))
        {
                // keeps the selection and modes: stopping puts you right
                // back where you were
                if (btn == SDL_BUTTON_LEFT) pmedit_animate = 1;
                return;
        }

        if (pmedit_sel < 0) // whole-model view: click selects a piece
        {
                int face, tu, tv;
                int hit = pmedit_pick(pmedit_mx, pmedit_my, -1, 0.f, &face, &tu, &tv, NULL);
                if (hit >= 0) { pmedit_sel = hit; pmedit_joint = pmedit_socket = pmedit_parent = 0; }
                return;
        }

        int face, tu, tv;
        if (pmedit_parent)
        {
                // click any OTHER piece: it becomes the parent and the click
                // lands as the attach point in its space, then SOCKET mode
                // takes over for fine-tuning it. Re-parenting to a piece that
                // hangs (transitively) off the selected one would loop the
                // chain, so those clicks do nothing.
                struct pm_piece *pc = &pm_models[my_player].piece[pmedit_sel];
                float h[3];
                int hit = pmedit_pick(pmedit_mx, pmedit_my, -2 - pmedit_sel,
                                0.f, &face, &tu, &tv, h);
                if (hit >= 0 && btn == SDL_BUTTON_LEFT)
                {
                        if (!pmedit_cycle(hit))
                        {
                                unsigned char *corner = pm_models[my_player].piece[hit].corner;
                                pc->parent = hit;
                                for (int a = 0; a < 3; a++)
                                        pc->attach[a] = ICLAMP((int)roundf(corner[a] + h[a]),
                                                        0, PM_TILE);
                                pmedit_parent = 0;
                                pmedit_socket = 1;
                        }
                        return;
                }
                if (hit < 0 && pmedit_pick(pmedit_mx, pmedit_my, -1, 1.f,
                                &face, &tu, &tv, NULL) < 0)
                {
                        pmedit_sel = -1;
                        pmedit_joint = pmedit_socket = pmedit_parent = 0;
                }
                return;
        }
        if (pmedit_joint || pmedit_socket)
        {
                // left plants the point at the clicked texel's spot - JOINT on
                // the piece itself, SOCKET on the parent piece; right does
                // nothing; a real miss (off piece AND parent) backs all the
                // way out
                struct pm_piece *pc = &pm_models[my_player].piece[pmedit_sel];
                int tgt = pmedit_joint ? pmedit_sel : pc->parent;
                float h[3];
                if (btn == SDL_BUTTON_LEFT && tgt >= 0 && pmedit_pick(pmedit_mx,
                                pmedit_my, tgt, 0.f, &face, &tu, &tv, h) >= 0)
                {
                        unsigned char *corner = pm_models[my_player].piece[tgt].corner;
                        unsigned char *pt = pmedit_joint ? pc->origin : pc->attach;
                        for (int a = 0; a < 3; a++)
                                pt[a] = ICLAMP((int)roundf(corner[a] + h[a]), 0, PM_TILE);
                        return;
                }
                if (pmedit_pick(pmedit_mx, pmedit_my, pmedit_sel, 1.f,
                                &face, &tu, &tv, NULL) >= 0) return;
                if (pc->parent >= 0 && pmedit_pick(pmedit_mx, pmedit_my,
                                pc->parent, 1.f, &face, &tu, &tv, NULL) >= 0) return;
                pmedit_sel = -1;
                pmedit_joint = pmedit_socket = pmedit_parent = 0;
                return;
        }

        // piece view: paint on a true hit; a near-miss (within a texel) does
        // nothing, and anything farther backs out to the whole model
        if (pmedit_paint(btn)) { pmedit_paint_btn = btn; return; }
        if (pmedit_pick(pmedit_mx, pmedit_my, pmedit_sel, 1.f, &face, &tu, &tv, NULL) < 0)
                pmedit_sel = -1;
}

void pmedit_motion()
{
        pmedit_mx = event.motion.x;
        pmedit_my = event.motion.y;
        if (pmedit_paint_btn) pmedit_paint(pmedit_paint_btn); // drag to paint
}

// grey backdrops behind the button labels so they read as buttons - solid 2D
// rects on the cursor pipeline, recorded before the text batches so the
// labels land on top. The boxes ARE the hit rects.
static void pmedit_boxes()
{
        float r[6][4];
        int nr = 0;
        for (int i = 0; i < 4; i++)
        {
                r[nr][0] = PMEDIT_BTN_X - 10; r[nr][1] = 16 + 64 * i;
                r[nr][2] = screenw - 8;       r[nr][3] = 76 + 64 * i;
                nr++;
        }
        r[nr][0] = PMEDIT_BTN_X - 10; r[nr][1] = 272;
        r[nr][2] = screenw - 8;       r[nr][3] = 332;
        nr++;
        r[nr][0] = PMEDIT_BTN_X - 10; r[nr][1] = 336;
        r[nr][2] = screenw - 8;       r[nr][3] = 396;
        nr++;

        float buf[6 * 12], *p = buf;
        for (int i = 0; i < nr; i++)
        {
                float x0 = r[i][0], y0 = r[i][1], x1 = r[i][2], y1 = r[i][3];
                *p++ = x0; *p++ = y0; *p++ = x1; *p++ = y1; *p++ = x1; *p++ = y0;
                *p++ = x1; *p++ = y1; *p++ = x0; *p++ = y0; *p++ = x0; *p++ = y1;
        }

        static struct allocation alloc[MAX_FRAMES_IN_FLIGHT];
        if (!alloc[vk.currentFrame].buf)
                vulkan_allocate_vertex_buffer(sizeof buf, &alloc[vk.currentFrame]);
        vulkan_populate_vertex_buffer(buf, (p - buf) * sizeof *buf,
                        &alloc[vk.currentFrame]);

        VkCommandBuffer cmdbuf = vk.commandBuffers[vk.imageIndex];
        vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS,
                        vk.pipelines[cursor_pipe].pipeline);
        VkViewport viewport = { 0, 0, screenw, screenh, 0, 1 };
        VkRect2D scissor = { {0, 0}, {screenw, screenh} };
        vkCmdSetViewport(cmdbuf, 0, 1, &viewport);
        vkCmdSetScissor(cmdbuf, 0, 1, &scissor);

        // the piece buttons grey down further when they don't apply
        int enabled = pmedit_sel >= 0 && !pmedit_animate;
        struct { float proj[16]; float color[3]; float pad; } push = {
                .proj = { 2.f / screenw, 0, 0, 0,
                          0, 2.f / screenh, 0, 0,
                          0, 0, 1, 0,
                         -1, -1, 0, 1 }, // pixel space, y 0 at the top
        };
        memcpy(push.color, enabled ? (float[3]){ 0.13f, 0.13f, 0.16f }
                                   : (float[3]){ 0.08f, 0.08f, 0.10f },
                        sizeof push.color);
        vkCmdPushConstants(cmdbuf, vk.pipelines[cursor_pipe].layout,
                        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                        0, sizeof push, &push);
        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(cmdbuf, 0, 1, &alloc[vk.currentFrame].buf, &offset);
        vkCmdDraw(cmdbuf, (nr - 2) * 6, 1, 0, 0); // piece buttons

        memcpy(push.color, (float[3]){ 0.13f, 0.13f, 0.16f }, sizeof push.color);
        vkCmdPushConstants(cmdbuf, vk.pipelines[cursor_pipe].layout,
                        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                        0, sizeof push, &push);
        vkCmdDraw(cmdbuf, 12, 1, (nr - 2) * 6, 0); // ANIMATE + STYLE, always live
}

void pmedit_draw_ui()
{
        if (!pmedit_on) return;

        pmedit_boxes();

        // piece buttons always show; they grey down to "disabled" when no
        // piece is selected or ANIMATE is playing
        int enabled = pmedit_sel >= 0 && !pmedit_animate;
        float dim = enabled ? 0.55f : 0.28f;

        font_begin(screenw, screenh);
        font_add_text("MOVE JOINT", PMEDIT_BTN_X, 28.f, 3);
        if (enabled && pmedit_joint) font_end(1, 1, 0.25f);
        else font_end(dim, dim, dim);

        font_begin(screenw, screenh);
        font_add_text("MOVE SOCKET", PMEDIT_BTN_X, 92.f, 3);
        if (enabled && pmedit_socket) font_end(1, 1, 0.25f);
        else font_end(dim, dim, dim);

        font_begin(screenw, screenh);
        font_add_text("SELECT PARENT", PMEDIT_BTN_X, 156.f, 3);
        if (enabled && pmedit_parent) font_end(1, 1, 0.25f);
        else font_end(dim, dim, dim);

        font_begin(screenw, screenh);
        font_add_text(pmedit_sel >= 0 ? (char *)pmedit_type_name[
                        pm_models[my_player].piece[pmedit_sel].type] : "TYPE",
                        PMEDIT_BTN_X, 220.f, 3);
        if (enabled) font_end(0.55f, 0.7f, 0.55f);
        else font_end(dim, dim, dim);

        font_begin(screenw, screenh);
        font_add_text("ANIMATE", PMEDIT_BTN_X, 284.f, 3);
        if (pmedit_animate) font_end(1, 1, 0.25f);
        else font_end(0.55f, 0.55f, 0.55f);

        font_begin(screenw, screenh);
        font_add_text(pm_models[my_player].style == PM_STYLE_FLAIL ?
                        "FLAIL" : "WALK", PMEDIT_BTN_X, 348.f, 3);
        font_end(0.55f, 0.7f, 0.55f);

        char *hint = pmedit_animate ?
                "WASD rotate   click anywhere to stop" :
                pmedit_sel < 0 ?
                "click a piece to paint it   WASD rotate   U done" :
                pmedit_joint ?
                "LMB place joint   arrows nudge   WASD rotate   click away to go back" :
                pmedit_socket ?
                "LMB place socket on parent   arrows nudge   WASD rotate" :
                pmedit_parent ?
                "LMB click a piece to make it the parent   WASD rotate" :
                "LMB paint red   RMB paint blue   WASD rotate   click away to go back";
        float scale = MIN(roundf(screenw / 600.f), roundf(screenh / 400.f));
        if (scale < 1) scale = 1;
        font_begin(screenw, screenh);
        font_add_text(hint, screenw / 2.f - strlen(hint) * 4.f * scale,
                        screenh - 30.f * scale, 0);
        font_end(1, 1, 1);
}

#endif // BLOCKO_PMEDIT_C_INCLUDED

#ifndef BLOCKO_PMEDIT_C_INCLUDED
#define BLOCKO_PMEDIT_C_INCLUDED
#include "blocko.c"

// In-game player model editor (U key). The model floats in front of the
// frozen camera in a plain standing pose; WASD spins it like a turntable
// (easing to 360 deg/s over half a second). Clicking a prism selects it: the
// view eases to center on that prism (zoomed to fit), it gets a white outline
// while the rest of the model draws full-color behind it (Z-tested, so colors
// stay matchable), and left-clicks paint the targeted pixel with the palette
// panel's selected swatch (right-click, or shift/ctrl/alt/cmd + click,
// eyedrops instead: the pointed-at texel's color becomes the swatch). The
// panel, top-left in this view, holds all 16 palette slots - transparent
// first (painting with it erases: the shader discards sub-half alpha), then
// the 15 colors - over a hue-lightness picker that recolors the selected
// swatch's slot live, every texel wearing it following, since paint is
// palette-indexed. Under the picker, FLOOD FILL swaps the brush for a
// bucket (a click repaints the connected same-color region on that face)
// and SUPER FLOOD for a firehose (a click recoats the piece's every texel,
// all six sides); clicking the lit button, or leaving the piece view,
// hands the brush back. The PLACE ATTACHMENT POINT button
// switches the piece view to placing the rotation origin instead, and MOVE
// PART to editing the attach point in the parent's space. PLACE ATTACHMENT
// POINT shows only the active piece (rest of the model hidden) and its
// origin as a small color-cycling cube with ruled axes through it (1-texel
// bright/dim segments, depth-tested so the surface intersections read
// exactly); the gizmo rides the cursor while it's on the piece, a click
// plants the point there and leaves the mode, and the zoom keeps both the
// piece and a nudged-away gizmo in frame.
// MOVE PART shows the piece and its pink-rimmed parent; pointing anywhere
// on the parent replaces the piece with a half-transparent preview of it
// laid flush against that face at the cursor (the attach point shifts
// along the face normal so the piece's near side kisses the surface - the
// joint never moves), and a click commits that spot and leaves the mode.
// Off the parent the piece sits where it is and the
// arrow keys nudge the point a px at a time (camera-relative; repeats work;
// WASD still rotates the turntable). PLACE ATTACHMENT POINT moves only the
// pivot - the attach point shifts by the same delta so the piece itself
// stays put. MOVE PART never touches the origin. The parent button is two
// buttons in one: with a parented piece selected it reads DETACH - one
// click re-hangs the piece on the invisible player box, folding the old
// parent chain's offsets into the attach point so the piece stays visually
// put (the attach range walls permitting) - and with a root-hung piece
// it's SELECT PARENT. PARENT mode
// re-wires the hierarchy in one click: the selection moves to its own
// little view on the left (same turntable spin, wearing the white outline,
// hovering with a gentle 1Hz half-texel bob - it's unattached)
// while the main view keeps only the eligible parents (pieces that would
// loop the chain are hidden). Hovering one previews it with a pink rim; the
// click makes it the parent AND lays the piece flush against the clicked
// spot, MOVE PART-style, then it's back to the whole model, nothing
// selected. ESC or a far miss backs out. In the plain selected view the
// arrow keys nudge the piece around just like MOVE PART (mouse placement
// stays that mode's). The TYPE button shows the
// selected
// piece's animation type (HEAD,
// LEFT ARM, ...) - left-click cycles forward, right-click back. RESIZE
// reshapes the prism: left-click a face to pick it (it wears a see-through
// cycling-hue slab), then drag it - the face pushes out and pulls in to
// follow the cursor - or nudge with the arrow keys a px at a time, by
// relative angle: the face grows when the arrow leans toward its outward
// normal on screen, shrinks when it leans away, and does nothing when the
// arrow reads near-perpendicular to it (rotate a bit). The paint follows
// (the lateral faces gain a copy of their edge row, or lose one) and the
// joint stays put; with no face picked the arrows are idle.
// RESTING ANGLE poses the piece's standing-still pitch/yaw/roll, +-25 deg a
// degree at a time: arrows pitch/yaw, Q/E roll, still relative to the
// parent. The angles live in the model (saved and sent like everything
// else) and the world always shows them, animations swinging on top; in the
// editor the pose shows ONLY in this mode and in ANIMATE - the geometry
// modes keep the plain standing pose their math needs; clicking another
// piece switches to it.
// DELETE (or the Del/Backspace keys) removes the piece and its whole
// subtree (no undo yet). NEW PART spawns a fresh 4x4x4 piece in a gray
// checkerboard coat and drops straight into the PARENT one-click flow to
// hang it somewhere; on the click its joint snaps to the middle of the side
// facing the parent, so the piece hangs outward
// (backing all the way out cancels the piece). MAKE COPY (or ctrl-C) clones
// the selected piece - prism, joint, paint, type - into the same flow, except
// the clone keeps the original's joint so it swings the same. The ANIMATE
// button plays the model's animation (walking in place)
// on the whole model - WASD still spins it, the piece buttons grey out, any
// click (or ESC) stops it right back where you were, and the zoom stays fit
// to the standing pose so it doesn't pump. The STYLE button below it flips
// the model between WALK and FLAIL animation styles (a model property that
// travels with it over the net) and is click-exempt so you can A/B styles
// while ANIMATE plays. HIDE gets overlapping pieces out of the way: click
// it, then every piece clicked goes 90% see-through and dead to clicks
// (unpickable everywhere) until a click lands somewhere else. With pieces
// hidden the button reads UNHIDE (n); one click brings them all back, no
// mode. Clicking on
// another piece switches straight to it (in paint and RESIZE
// modes - wherever a click on it means nothing else); clicking off every
// piece or ESC goes back to the whole model; U closes the editor and
// announces the new look over the net. Saving is automatic: a session that
// changes anything writes its own numbered snapshot (00001.model, ...)
// under the per-user save dir (SDL_GetPrefPath), at most once a second, final
// write at close - the newest snapshot is what the game loads next run. A half-transparent
// dark green quad, one block's top face, floats at the in-game ground level
// in every view that shows the whole model (near-level pitches only), so a
// floating or sunken model reads at a glance; a wireframe box in the same
// green traces the player's collision box so the model can be sized to it.

static int pmedit_sel = -1;              // selected piece, -1 = whole model
static int pmedit_joint;                 // JOINT mode: edit the rotation origin
static int pmedit_socket;                // SOCKET mode: edit the parent attach point
static int pmedit_parent;                // PARENT mode: click a piece to re-parent to
static int pmedit_newpart;               // this PARENT flow is placing a piece that
                                         // cancels away: 1 = brand-new, 2 = a copy
static int pmedit_resize;                // RESIZE mode: click a face, arrows push/pull
static int pmedit_face;                  // RESIZE's picked face (orient 1..6), 0 = none
static int pmedit_hide;                  // HIDE mode: click pieces to ghost them away
static unsigned pmedit_hidden;           // bitmask of hidden (ghosted, unclickable) pieces
static int pmedit_animate;               // ANIMATE mode: play the placeholder anim
static int pmedit_picker;                // LOAD: the thumbnail-grid overlay
static int pmedit_restang;               // RESTING ANGLE mode: arrows/QE pose the piece
static float pmedit_bob_t;               // parent-mode hover bob clock, wraps at 1s
static unsigned pmedit_hist_sum;         // model checksum as last snapshotted
static int pmedit_hist_n;                // session's snapshot number, 0 = none yet
static float pmedit_hist_cool;           // snapshot debounce: one write a second
// in-memory undo/redo: a ring of whole-struct snapshots (~15.8KB each, the
// wire size). 128 deep is ~2MiB and far more history than a session needs.
// Commit-on-settle (pmedit_undo_track): a snapshot lands when the model
// stops changing, so a paint drag is one entry, not one per texel. Undo/redo
// restore LOCALLY only - peers see the model once, on editor exit.
#define PMEDIT_UNDO_CAP 128              // power of two
#define PMEDIT_UNDO_MASK (PMEDIT_UNDO_CAP - 1)
static struct pmodel pmedit_undo_ring[PMEDIT_UNDO_CAP];
static int pmedit_undo_next;             // one past the newest snapshot index
static int pmedit_undo_cur;              // index of the state on screen
static int pmedit_undo_oldest;           // oldest index still in the ring
static unsigned pmedit_undo_sum;         // checksum of ring[pmedit_undo_cur]
static unsigned pmedit_undo_prev;        // checksum last frame (settle detector)
static float pmedit_undo_cool;           // debounce: one NEW snapshot per window;
                                         // faster edits fold into the current one
#define PMEDIT_UNDO_DEBOUNCE 0.5f        // seconds between fresh snapshots
static int pmedit_color = PMEDIT_RED;    // the palette slot paint clicks lay down
static int pmedit_pal_drag;              // LMB is down in the color picker
static int pmedit_flood;                 // 0 = plain paint, 1 = FLOOD FILL the
                                         // clicked region, 2 = SUPER FLOOD the piece
static int pmedit_resize_drag;           // RESIZE: LMB held after picking a face
static float pmedit_resize_ax, pmedit_resize_ay; // the drag's anchor cursor
static int pmedit_resize_applied;        // px pushed/pulled since the anchor
static float pmedit_gizmo_mat[16];       // the active gizmo's px -> world frame
static float pmedit_myaw, pmedit_mpitch; // turntable angles
static float pmedit_yramp, pmedit_pramp; // seconds each axis key is held, <= 1
static int pmedit_kw, pmedit_ka, pmedit_ks, pmedit_kd;
static float pmedit_cen[3];              // eased center of interest (model frame)
static float pmedit_dist;                // eased eye-to-center distance
static int pmedit_snap;                  // skip easing on the first frame
static float pmedit_mats[PM_MAX_PIECES][16]; // px->world per piece, for picking
static float pmedit_proj[16];            // the editor's own projection (see below)
static float pmedit_pv[16];              // proj * leveled view, for world->screen
static int pmedit_nr;                    // pieces in pmedit_mats

// the editor views the model through a much narrower FOV than the world's
// 90 degrees, so it reads nearly orthographic: the camera sits far back and
// perspective foreshortening goes flat, so a big nose no longer looms over
// and hides the rest of the face. Smaller = flatter. Zoom-to-fit and the
// pick rays share the same matrix, so clicks land where they look.
#define PMEDIT_FOV 34.f
// and a hard zoom cap: never frame tighter than a piece this many px in
// radius would need, so a tiny piece sits small and centered instead of
// blown up crazy-close (the framing still uses the real box - only the
// fit distance is floored)
#define PMEDIT_MIN_RADIUS_PX 6.f
static int pmedit_paint_btn;             // mouse button held down painting, or 0
static float pmedit_mx, pmedit_my;       // latest cursor position
static int pmedit_prev_view;             // cam_view to restore when closing
static int pm_edit_rest_start, pm_edit_rest_count;   // instance ranges in
static int pm_edit_hull_start, pm_edit_hull_count;   // pmbuf: unselected
static int pm_edit_sel_start, pm_edit_sel_count;     // pieces, outline hull,
static int pm_edit_joint_start, pm_edit_joint_count; // selection, joint gizmo
static int pm_edit_floor_start, pm_edit_floor_count; // ground + hitbox frame
static int pm_edit_hide_start, pm_edit_hide_count;   // and the hidden ghosts

#define PMEDIT_PICK_ELIG (-100) // pick only pieces eligible to be the parent

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

static int pmedit_in_resize_btn(float x, float y)
{
        return pmedit_sel >= 0 &&
                x >= PMEDIT_BTN_X - 10 && x <= screenw - 8 && y >= 272 && y <= 332;
}

static int pmedit_in_restang_btn(float x, float y)
{
        return pmedit_sel >= 0 &&
                x >= PMEDIT_BTN_X - 10 && x <= screenw - 8 && y >= 336 && y <= 396;
}

static int pmedit_in_copy_btn(float x, float y)
{
        return pmedit_sel >= 0 &&
                x >= PMEDIT_BTN_X - 10 && x <= screenw - 8 && y >= 400 && y <= 460;
}

static int pmedit_in_delete_btn(float x, float y)
{
        return pmedit_sel >= 0 &&
                x >= PMEDIT_BTN_X - 10 && x <= screenw - 8 && y >= 464 && y <= 524;
}

static int pmedit_in_newpart_btn(float x, float y)
{
        return x >= PMEDIT_BTN_X - 10 && x <= screenw - 8 && y >= 528 && y <= 588;
}

static int pmedit_in_animate_btn(float x, float y)
{
        return x >= PMEDIT_BTN_X - 10 && x <= screenw - 8 && y >= 592 && y <= 652;
}

static int pmedit_in_style_btn(float x, float y)
{
        return x >= PMEDIT_BTN_X - 10 && x <= screenw - 8 && y >= 656 && y <= 716;
}

static int pmedit_in_hide_btn(float x, float y)
{
        return x >= PMEDIT_BTN_X - 10 && x <= screenw - 8 && y >= 720 && y <= 780;
}

// the palette panel, top-left, up whenever paint clicks are: 16 swatches in
// two rows of 8 (transparent's checker first, then the 15 colors - clicking
// one makes it the paint color), and the color picker below - the classic
// hue-lightness plane, white across the top, black across the bottom, the
// full-saturation rainbow through the middle. Clicking (or dragging) the
// picker recolors the SELECTED swatch's palette slot, and every texel
// wearing that slot follows live. Slot 0 stays transparent forever;
// painting with it erases (the shader discards texels under half alpha)
#define PMEDIT_SW_X0     16
#define PMEDIT_SW_Y0     16
#define PMEDIT_SW_SZ     36
#define PMEDIT_SW_STRIDE 40
#define PMEDIT_HSL_X0    16
#define PMEDIT_HSL_Y0    100
#define PMEDIT_HSL_X1    336
#define PMEDIT_HSL_Y1    340
#define PMEDIT_FF_Y0     348 // the FLOOD FILL button, full panel width
#define PMEDIT_FF_Y1     392
#define PMEDIT_SF_Y0     400 // SUPER FLOOD below it
#define PMEDIT_SF_Y1     444
#define PMEDIT_PAL_PAD   8 // the backdrop, and the panel's whole click-eating rect

static int pmedit_panel_on()
{
        return pmedit_sel >= 0 && !pmedit_joint && !pmedit_socket
                && !pmedit_parent && !pmedit_resize && !pmedit_restang
                && !pmedit_animate;
}

static int pmedit_in_panel(float x, float y)
{
        return x >= PMEDIT_SW_X0 - PMEDIT_PAL_PAD
                && x <= PMEDIT_HSL_X1 + PMEDIT_PAL_PAD
                && y >= PMEDIT_SW_Y0 - PMEDIT_PAL_PAD
                && y <= PMEDIT_SF_Y1 + PMEDIT_PAL_PAD;
}

static int pmedit_in_flood_btn(float x, float y)
{
        return x >= PMEDIT_HSL_X0 && x <= PMEDIT_HSL_X1
                && y >= PMEDIT_FF_Y0 && y <= PMEDIT_FF_Y1;
}

static int pmedit_in_sflood_btn(float x, float y)
{
        return x >= PMEDIT_HSL_X0 && x <= PMEDIT_HSL_X1
                && y >= PMEDIT_SF_Y0 && y <= PMEDIT_SF_Y1;
}

static int pmedit_in_swatch(float x, float y)
{
        for (int i = 0; i < PM_NR_COLORS; i++)
        {
                float x0 = PMEDIT_SW_X0 + i % 8 * PMEDIT_SW_STRIDE;
                float y0 = PMEDIT_SW_Y0 + i / 8 * PMEDIT_SW_STRIDE;
                if (x >= x0 && x <= x0 + PMEDIT_SW_SZ &&
                    y >= y0 && y <= y0 + PMEDIT_SW_SZ)
                        return i;
        }
        return -1;
}

// the picker's hue corners: linear RGB interpolation between adjacent
// primaries/secondaries traces the full-saturation hue wheel exactly, so
// six vertex-colored segments render the true gradient
static const float pmedit_hue6[7][3] = {
        {1, 0, 0}, {1, 1, 0}, {0, 1, 0}, {0, 1, 1},
        {0, 0, 1}, {1, 0, 1}, {1, 0, 0},
};

// picker coords (u hue, v lightness, both 0..1) -> packed palette color,
// matching the rendered gradient exactly
static unsigned pmedit_hsl(float u, float v)
{
        float t = u * 6;
        int k = ICLAMP((int)t, 0, 5);
        t -= k;
        float c[3];
        for (int a = 0; a < 3; a++)
        {
                c[a] = pmedit_hue6[k][a]
                        + (pmedit_hue6[k + 1][a] - pmedit_hue6[k][a]) * t;
                if (v < 0.5f) c[a] += (1 - c[a]) * (1 - v * 2);
                else          c[a] *= 1 - (v - 0.5f) * 2;
        }
        return PM_RGB((int)(c[0] * 255 + 0.5f), (int)(c[1] * 255 + 0.5f),
                      (int)(c[2] * 255 + 0.5f));
}

// a click or drag in the picker: recolor the selected swatch's slot. The
// coords clamp, so a drag that wanders out keeps tracking - the edges hold
// pure white, black and the corner hues
static void pmedit_pal_pick(float x, float y)
{
        if (!pmedit_color) return; // transparent isn't a color to edit
        float u = (x - PMEDIT_HSL_X0) / (PMEDIT_HSL_X1 - PMEDIT_HSL_X0);
        float v = (y - PMEDIT_HSL_Y0) / (PMEDIT_HSL_Y1 - PMEDIT_HSL_Y0);
        unsigned c = pmedit_hsl(CLAMP(u, 0, 1), CLAMP(v, 0, 1));
        if (pm_models[my_player].palette[pmedit_color] == c) return;
        pm_models[my_player].palette[pmedit_color] = c;
        pmodel_upload(my_player);
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

// piece i plays no part in the active gizmo mode: shown 90% see-through as a
// spatial reference rather than vanishing. PLACE ATTACHMENT POINT (joint)
// isolates the selected piece; MOVE PART (socket) keeps its parent solid too;
// SELECT PARENT (parent) keeps the eligible parents solid. The selected piece
// and mode-off views are never faint (return 0)
static int pmedit_mode_faint(struct pmodel *mo, int i)
{
        if (pmedit_sel < 0 || i == pmedit_sel) return 0;
        if (pmedit_joint)  return 1;
        if (pmedit_socket) return i != mo->piece[pmedit_sel].parent;
        if (pmedit_parent) return pmedit_cycle(i);
        return 0;
}

// face orient code (1..6) -> its normal's axis and sign
static const signed char pmedit_axis_of[7] = { 0, 1, 0, 2, 0, 2, 1 };
static const signed char pmedit_side_of[7] = { 0, -1, 1, 1, -1, -1, 1 };

// NEW PART: snap the joint to the middle of the piece's side FACING the
// parent (the opposite side from the parent face the socket is going on),
// so the new piece hangs outward from wherever it gets attached
static void pmedit_newpart_snap(struct pm_piece *pc, int face)
{
        int a = pmedit_axis_of[face], s = pmedit_side_of[face];
        for (int q = 0; q < 3; q++)
                pc->origin[q] = pc->corner[q] + pc->dims[q] / 2;
        pc->origin[a] = s > 0 ? pc->corner[a]
                              : pc->corner[a] + pc->dims[a];
}

// the attach point that puts pc flush against piece P's hovered face at the
// picked point h (P-local px, corner-relative) - the attach moves, the joint
// never does. The flush shift can push the attach past the parent's 16^3
// tile; that's fine, it's just a translation.
static void pmedit_flush_att(struct pm_piece *pc, int P, int face,
                float *h, int att[3])
{
        unsigned char *corner = pm_models[my_player].piece[P].corner;
        for (int q = 0; q < 3; q++)
                att[q] = ICLAMP((int)roundf(corner[q] + h[q]), 0, PM_TILE);
        int a = pmedit_axis_of[face];
        att[a] += pc->origin[a] - pc->corner[a]
                - (pmedit_side_of[face] < 0 ? pc->dims[a] : 0);
        att[a] = ICLAMP(att[a], 0, 2 * PM_TILE);
}

// MOVE PART: where would a click lay the piece? Picks the parent surface
// under the cursor and, on a hit, computes the flush attach point there.
// Returns whether the cursor is on the parent at all.
static int pmedit_sock_aim(struct pm_piece *pc, int att[3])
{
        int face, tu, tv;
        float h[3];
        if (pc->parent < 0 || pmedit_pick(pmedit_mx, pmedit_my,
                        (int)pc->parent, 0.f, &face, &tu, &tv, h) < 0)
                return 0;
        pmedit_flush_att(pc, (int)pc->parent, face, h, att);
        return 1;
}

// backing out of a NEW PART placement cancels it: the unplaced piece goes
// away with the mode (it was never attached to anything)
static void pmedit_newpart_cancel()
{
        if (!pmedit_newpart) return;
        pmedit_newpart = 0;
        if (pmedit_sel >= 0 &&
                        pm_piece_delete(&pm_models[my_player], pmedit_sel))
                pmodel_upload(my_player);
}

// MAKE COPY (the button, or ctrl-C): a full clone of the selection - prism,
// joint, paint, type - then straight into the parent-picking flow to hang it
// somewhere, exactly like NEW PART (backing out cancels the copy). Unlike
// NEW PART the joint stays the original's, so it swings the same
static void pmedit_copy()
{
        struct pmodel *mo = &pm_models[my_player];
        // no copying a piece that isn't placed yet
        if (pmedit_sel < 0 || pmedit_newpart
                        || mo->nr_pieces >= PM_MAX_PIECES)
                return;
        int i = mo->nr_pieces++;
        mo->piece[i] = mo->piece[pmedit_sel];
        mo->piece[i].parent = -1;
        memcpy(mo->texel[i], mo->texel[pmedit_sel], sizeof mo->texel[i]);
        pmodel_upload(my_player);
        pmedit_sel = i;
        pmedit_parent = 1;
        pmedit_newpart = 2;
        pmedit_joint = pmedit_socket = 0;
        pmedit_resize = pmedit_face = pmedit_restang = 0;
        pmedit_hide = 0;
}

// DELETE (the button, or the Del/Backspace keys): the piece and its whole
// subtree go away (no undo yet); the survivors' indices compact down
static void pmedit_delete()
{
        if (pmedit_sel < 0
                        || !pm_piece_delete(&pm_models[my_player], pmedit_sel))
                return;
        pmedit_sel = -1;
        pmedit_joint = pmedit_socket = pmedit_parent = 0;
        pmedit_resize = pmedit_face = pmedit_newpart = pmedit_restang = 0;
        pmedit_hidden = 0; // survivors' indices shifted
        pmodel_upload(my_player);
}

// history autosave: while the editor is open, any change to the model
// (checksummed, so paint, geometry, TYPE, STYLE, rest angles all count)
// lands in this session's numbered snapshot, debounced to one write a
// second; closing writes the tail regardless of the clock. The first
// change claims the next free number, later writes overwrite it, and the
// number is dropped at close - a closed session's file is never touched
static void pmedit_hist_save(int final)
{
        if (!final && pmedit_hist_cool > 0) return;
        unsigned sum = pm_checksum(&pm_models[my_player]);
        if (sum == pmedit_hist_sum) return;
        if (!pmedit_hist_n)
        {
                pmedit_hist_n = pm_hist_newest() + 1;
                fprintf(stderr, "pmodel: session snapshot %s/%05d.model\n",
                        pm_hist_dir, pmedit_hist_n);
        }
        pm_hist_write(pmedit_hist_n);
        pmedit_hist_sum = sum;
        pmedit_hist_cool = 1;
}

// start the undo timeline over with the current model as snapshot 0 (on open)
static void pmedit_undo_reset(void)
{
        pmedit_undo_ring[0] = pm_models[my_player];
        pmedit_undo_next = 1;
        pmedit_undo_cur = 0;
        pmedit_undo_oldest = 0;
        pmedit_undo_sum = pm_checksum(&pm_models[my_player]);
        pmedit_undo_prev = pmedit_undo_sum;
        pmedit_undo_cool = 0;
}

// fold the live model into a new snapshot after pmedit_undo_cur, dropping any
// redo tail and aging out the oldest entry once the ring is full
static void pmedit_undo_commit(void)
{
        int i = pmedit_undo_cur + 1;
        pmedit_undo_ring[i & PMEDIT_UNDO_MASK] = pm_models[my_player];
        pmedit_undo_cur = i;
        pmedit_undo_next = i + 1;
        if (pmedit_undo_next - pmedit_undo_oldest > PMEDIT_UNDO_CAP)
                pmedit_undo_oldest = pmedit_undo_next - PMEDIT_UNDO_CAP;
        pmedit_undo_sum = pm_checksum(&pm_models[my_player]);
        pmedit_undo_prev = pmedit_undo_sum;
}

// restore ring[cur] into the live model and refresh its texture layers. LOCAL
// ONLY: no pmodel_send_mine here - peers learn the final model on editor exit
static void pmedit_undo_restore(void)
{
        pm_models[my_player] = pmedit_undo_ring[pmedit_undo_cur & PMEDIT_UNDO_MASK];
        pmodel_upload(my_player);
        pmedit_undo_sum = pm_checksum(&pm_models[my_player]);
        pmedit_undo_prev = pmedit_undo_sum;
        pmedit_undo_cool = 0; // the next edit starts a fresh snapshot, not a fold
}

static void pmedit_undo(void)
{
        if (pmedit_undo_cur <= pmedit_undo_oldest) return;
        pmedit_undo_cur--;
        pmedit_undo_restore();
}

static void pmedit_redo(void)
{
        if (pmedit_undo_cur + 1 >= pmedit_undo_next) return;
        pmedit_undo_cur++;
        pmedit_undo_restore();
}

// every frame: once the model changes and then holds still for a frame, record
// it. A fresh snapshot only every PMEDIT_UNDO_DEBOUNCE seconds; edits faster
// than that (a stream of nudges) fold into the current snapshot in place, so a
// rapid burst is one undo step, not one per nudge - and the ring can't fill in
// a second. The first edit after opening or an undo commits immediately (cool
// is 0), keeping undo responsive
static void pmedit_undo_track(float dt)
{
        if (pmedit_undo_cool > 0) pmedit_undo_cool -= dt;
        unsigned sum = pm_checksum(&pm_models[my_player]);
        if (sum == pmedit_undo_sum) { pmedit_undo_prev = sum; return; } // clean
        if (sum != pmedit_undo_prev) { pmedit_undo_prev = sum; return; } // moving
        if (pmedit_undo_cool > 0)
        {
                // within the window: overwrite the current snapshot with the
                // latest state so the whole burst collapses to one undo step
                pmedit_undo_ring[pmedit_undo_cur & PMEDIT_UNDO_MASK] =
                        pm_models[my_player];
                pmedit_undo_sum = sum;
        }
        else
        {
                pmedit_undo_commit();
                pmedit_undo_cool = PMEDIT_UNDO_DEBOUNCE;
        }
}

// ---- model picker ---------------------------------------------------------
// A full-screen grid of live thumbnails, opened by the LOAD button in
// the whole-model view. Sources, in order: the named asset models (Bloc Croc,
// Buster Bo, the default), then this install's saved snapshots newest-first.
// Every source is read-only here; choosing one loads it live AND writes a
// fresh newest-numbered snapshot (so the pick loads next launch and later
// edits ride that new file). PM_NR_PREVIEW cells to a page; extras paginate.

// draw n solid pixel-space rects (x0,y0,x1,y1 each) in one rgb on the cursor
// pipeline. Each caller passes its OWN per-frame allocation: one populate per
// alloc per frame, so a recorded draw never reads a buffer a later call
// clobbered (the whole command buffer executes at submit).
static void pmedit_fill_rects(VkCommandBuffer cmdbuf, struct allocation *alloc,
                const float (*rects)[4], int n, const float rgb[3])
{
        if (n <= 0) return;
        static float buf[64 * 12];
        if (n > 64) n = 64;
        float *p = buf;
        for (int i = 0; i < n; i++)
        {
                float x0 = rects[i][0], y0 = rects[i][1];
                float x1 = rects[i][2], y1 = rects[i][3];
                *p++ = x0; *p++ = y0; *p++ = x1; *p++ = y1; *p++ = x1; *p++ = y0;
                *p++ = x1; *p++ = y1; *p++ = x0; *p++ = y0; *p++ = x0; *p++ = y1;
        }
        if (!alloc[vk.currentFrame].buf)
                vulkan_allocate_vertex_buffer(sizeof buf, &alloc[vk.currentFrame]);
        vulkan_populate_vertex_buffer(buf, n * 12 * sizeof *buf, &alloc[vk.currentFrame]);

        vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS,
                        vk.pipelines[cursor_pipe].pipeline);
        VkViewport viewport = { 0, 0, screenw, screenh, 0, 1 };
        VkRect2D scissor = { {0, 0}, {screenw, screenh} };
        vkCmdSetViewport(cmdbuf, 0, 1, &viewport);
        vkCmdSetScissor(cmdbuf, 0, 1, &scissor);
        struct { float proj[16]; float color[3]; float pad; } push = {
                .proj = { 2.f / screenw, 0, 0, 0,
                          0, 2.f / screenh, 0, 0,
                          0, 0, 1, 0,
                         -1, -1, 0, 1 },
        };
        memcpy(push.color, rgb, sizeof push.color);
        vkCmdPushConstants(cmdbuf, vk.pipelines[cursor_pipe].layout,
                        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                        0, sizeof push, &push);
        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(cmdbuf, 0, 1, &alloc[vk.currentFrame].buf, &offset);
        vkCmdDraw(cmdbuf, n * 6, 1, 0, 0);
}

// the LOAD button, top-left, only in the plain whole-model view
static int pmedit_in_changemodel_btn(float x, float y)
{
        return pmedit_sel < 0 && !pmedit_animate && !pmedit_hide
                && x >= 16 && x <= 236 && y >= 16 && y <= 60;
}

struct pm_pick_ent { char label[24]; char path[256]; };
#define PM_PICK_MAX 256
#define PM_PICK_COLS 3
#define PM_PICK_ROWS 2 // PM_PICK_COLS * PM_PICK_ROWS == PM_NR_PREVIEW
static struct pm_pick_ent pmedit_pick_ent[PM_PICK_MAX];
static int pmedit_pick_nr;                              // selectable models found
static int pmedit_pick_page;                           // page of PM_NR_PREVIEW up
static struct pmodel pmedit_pick_model[PM_NR_PREVIEW];  // the page's loaded models
static int pmedit_pick_loaded[PM_NR_PREVIEW];           // cell holds a valid model
static float pmedit_pick_spin;                          // shared slow turntable

static int pmedit_pick_pages(void)
{
        int p = (pmedit_pick_nr + PM_NR_PREVIEW - 1) / PM_NR_PREVIEW;
        return p < 1 ? 1 : p;
}

// catalog the selectable models: named assets first, then save-data snapshots
// newest number first. Only files that actually load make the list.
static void pmedit_pick_scan(void)
{
        pmedit_pick_nr = 0;
        static const struct { const char *label, *path; } named[] = {
                { "BLOC CROC", TINYC_DIR "/blocko-game/assets/models/player-bloc-croc.model" },
                { "BUSTER BO", TINYC_DIR "/blocko-game/assets/models/player-buster-bo.model" },
                { "DEFAULT",   PM_DEFAULT_FILE },
        };
        struct pmodel probe;
        for (int i = 0; i < 3; i++)
        {
                if (!pmodel_load(&probe, named[i].path)) continue;
                struct pm_pick_ent *e = &pmedit_pick_ent[pmedit_pick_nr++];
                snprintf(e->label, sizeof e->label, "%s", named[i].label);
                snprintf(e->path, sizeof e->path, "%s", named[i].path);
        }
        // snapshots: glob the numbers, sort descending, list newest first
        int nums[PM_PICK_MAX], nn = 0, count = 0;
        char **names = SDL_GlobDirectory(pm_hist_dir, "*.model", 0, &count);
        for (int i = 0; names && i < count && nn < PM_PICK_MAX; i++)
        {
                char *end;
                long n = strtol(names[i], &end, 10);
                if (n > 0 && !strcmp(end, ".model")) nums[nn++] = (int)n;
        }
        SDL_free(names);
        for (int a = 0; a < nn; a++)
        {
                int best = a;
                for (int b = a + 1; b < nn; b++)
                        if (nums[b] > nums[best]) best = b;
                int t = nums[a]; nums[a] = nums[best]; nums[best] = t;
        }
        for (int i = 0; i < nn && pmedit_pick_nr < PM_PICK_MAX; i++)
        {
                struct pm_pick_ent *e = &pmedit_pick_ent[pmedit_pick_nr++];
                snprintf(e->label, sizeof e->label, "SAVE %d", nums[i]);
                pm_hist_path(e->path, sizeof e->path, nums[i]);
        }
}

// load the current page's models off disk and upload them to the preview slots
static void pmedit_pick_load_page(void)
{
        for (int c = 0; c < PM_NR_PREVIEW; c++)
        {
                int e = pmedit_pick_page * PM_NR_PREVIEW + c;
                pmedit_pick_loaded[c] = e < pmedit_pick_nr
                        && pmodel_load(&pmedit_pick_model[c], pmedit_pick_ent[e].path);
        }
        pmodel_upload_previews(pmedit_pick_model, pmedit_pick_loaded);
}

static void pmedit_picker_open(void)
{
        pmedit_pick_scan();
        pmedit_pick_page = 0;
        pmedit_pick_spin = 0;
        pmedit_picker = 1;
        pmedit_pick_load_page();
}

static void pmedit_pick_flip(int d) // paginate, clamped
{
        int p = pmedit_pick_page + d;
        if (p < 0 || p >= pmedit_pick_pages()) return;
        pmedit_pick_page = p;
        pmedit_pick_load_page();
}

// choose cell c: adopt its model live, then write a fresh newest snapshot and
// make it this session's file, so the pick loads next launch and later edits
// update it. The undo tracker sees the swap and folds it into the timeline.
static void pmedit_pick_choose(int c)
{
        if (!pmedit_pick_loaded[c]) return;
        pm_models[my_player] = pmedit_pick_model[c];
        pmodel_upload(my_player);
        pmedit_hist_n = pm_hist_newest() + 1;
        pm_hist_write(pmedit_hist_n);
        pmedit_hist_sum = pm_checksum(&pm_models[my_player]);
        pmedit_hist_cool = 1;
        pmedit_picker = 0;
        pmedit_sel = -1;
        pmedit_joint = pmedit_socket = pmedit_parent = pmedit_newpart = 0;
        pmedit_resize = pmedit_face = pmedit_restang = 0;
        pmedit_hide = 0; pmedit_hidden = 0;
}

// the grid is a PM_PICK_COLS x PM_PICK_ROWS block inset from the screen edges,
// with title/hint bands above and below
static void pmedit_pick_cell_rect(int c, float *r)
{
        float mx = 90, top = 130, bot = 100;
        float cw = (screenw - 2 * mx) / PM_PICK_COLS;
        float ch = (screenh - top - bot) / PM_PICK_ROWS;
        int col = c % PM_PICK_COLS, row = c / PM_PICK_COLS;
        float pad = 14;
        r[0] = mx + col * cw + pad;         r[1] = top + row * ch + pad;
        r[2] = mx + (col + 1) * cw - pad;   r[3] = top + (row + 1) * ch - pad;
}

static void pmedit_pick_prev_rect(float *r)
{ r[0] = 90;              r[1] = screenh - 78; r[2] = 240;        r[3] = screenh - 34; }
static void pmedit_pick_next_rect(float *r)
{ r[0] = screenw - 240;  r[1] = screenh - 78; r[2] = screenw - 90; r[3] = screenh - 34; }

static int pmedit_in_rect(float x, float y, const float *r)
{ return x >= r[0] && x <= r[2] && y >= r[1] && y <= r[3]; }

// which visible cell the cursor is over, or -1
static int pmedit_pick_hover(void)
{
        float r[4];
        for (int c = 0; c < PM_NR_PREVIEW; c++)
        {
                if (!pmedit_pick_loaded[c]) continue;
                pmedit_pick_cell_rect(c, r);
                if (pmedit_in_rect(pmedit_mx, pmedit_my, r)) return c;
        }
        return -1;
}

static void pmedit_pick_click(int btn)
{
        if (btn != SDL_BUTTON_LEFT) return;
        float r[4];
        if (pmedit_pick_pages() > 1)
        {
                pmedit_pick_prev_rect(r);
                if (pmedit_in_rect(pmedit_mx, pmedit_my, r)) { pmedit_pick_flip(-1); return; }
                pmedit_pick_next_rect(r);
                if (pmedit_in_rect(pmedit_mx, pmedit_my, r)) { pmedit_pick_flip(1); return; }
        }
        int c = pmedit_pick_hover();
        if (c >= 0) pmedit_pick_choose(c);
}

// remote driving hook (remote.c "pmpick"): no arg opens the picker, "N" chooses
// cell N, "next"/"prev" paginate, "close" closes it. Returns the picker state.
int pmedit_remote_pick(const char *arg)
{
        if (!pmedit_on) return 0;
        if (!strncmp(arg, "next", 4)) { if (pmedit_picker) pmedit_pick_flip(1); }
        else if (!strncmp(arg, "prev", 4)) { if (pmedit_picker) pmedit_pick_flip(-1); }
        else if (!strncmp(arg, "close", 5)) pmedit_picker = 0;
        else
        {
                int n;
                if (pmedit_picker && sscanf(arg, "%d", &n) == 1) pmedit_pick_choose(n);
                else pmedit_picker_open();
        }
        return pmedit_picker;
}

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

// emit piece pc's highlight shell: the prism inflated by a thin membrane in
// a solid color, CAMERA-BACK faces only (an inverted hull) - the piece draws
// in front of all of it except a thin rim at its silhouette, and depth stays
// honest against every other piece (unlike the cleared-depth silhouette
// trick paint mode uses). M = the piece's px->world matrix. Returns the
// number of face instances emitted.
static int pmedit_shell(struct pmvert **bp, float *M, struct pm_piece *pc,
                int layer, float glow)
{
        float e = 0.15f;
        float t1[16], H[16];
        pm_mat_translate(t1, -e, -e, -e);
        mat4_multiply(H, M, t1);
        float d[3] = { pc->dims[0] + 2*e, pc->dims[1] + 2*e, pc->dims[2] + 2*e };
        // orient code order UP,EAST,NORTH,WEST,SOUTH,DOWN -> normal axis+sign
        static const signed char ax[PM_FACES] = {  1, 0, 2,  0,  2, 1 };
        static const signed char sn[PM_FACES] = { -1, 1, 1, -1, -1, 1 };
        // the pipeline back-face culls, so each back wall is emitted as the
        // OPPOSITE orient code translated onto it: the same quad, wound
        // facing the camera (the UVs differ, but the layer is solid color)
        static const signed char opp[PM_FACES] =
                        { DOWN, WEST, SOUTH, EAST, NORTH, UP };
        int n = 0;
        for (int fc = 0; fc < PM_FACES; fc++)
        {
                int a = ax[fc], s = sn[fc];
                float N[3] = { s * H[4*a], s * H[4*a+1], s * H[4*a+2] };
                float c[3] = { d[0]/2, d[1]/2, d[2]/2 };
                c[a] += s * d[a]/2;
                float P[4];
                mat4_f3_multiply(P, H, c[0], c[1], c[2]);
                if (N[0] * (P[0] - peye0) + N[1] * (P[1] - peye1)
                  + N[2] * (P[2] - peye2) <= 0) continue; // faces the camera
                float off[3] = { 0, 0, 0 }, t2[16], Hf[16];
                off[a] = s * d[a];
                pm_mat_translate(t2, off[0], off[1], off[2]);
                mat4_multiply(Hf, H, t2);
                *(*bp)++ = (struct pmvert){
                        .r0 = { Hf[0], Hf[4], Hf[8],  Hf[12] },
                        .r1 = { Hf[1], Hf[5], Hf[9],  Hf[13] },
                        .r2 = { Hf[2], Hf[6], Hf[10], Hf[14] },
                        .dims = { d[0], d[1], d[2] }, .orient = opp[fc],
                        .tex = layer, .illum = 0.f, .glow = glow,
                };
                n++;
        }
        return n;
}

// build the preview's instances into pmbuf: unselected pieces, then the white
// outline hull, then the selected piece. In paint/TYPE modes each group draws
// onto freshly cleared depth, so the selection always reads in full, in
// front; JOINT/SOCKET/PARENT share one honest depth buffer instead (piece
// overlaps read true and the gizmo pierces every surface exactly). Also
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
                        .tail_phase = anim_t * 2.f,
                        .t = anim_t,
                        .style = mo->style,
                };
        }
        // the test resting pose shows ONLY here (RESTING ANGLE mode and the
        // ANIMATE preview) - the flag drops right back so the in-world
        // resolves never pose with it
        pm_rest_apply = pmedit_restang || pmedit_animate;
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
        pm_rest_apply = 0;

        // center of interest + radius: the selected prism's bounding box
        // (center = its corner average), plus its parent's in SOCKET mode,
        // or the whole model's
        float lo[3] = {1e9f, 1e9f, 1e9f}, hi[3] = {-1e9f, -1e9f, -1e9f};
        int spar = pmedit_socket && pmedit_sel >= 0 ? mo->piece[pmedit_sel].parent : -1;
        for (int i = 0; i < pmedit_nr; i++)
        {
                // frame what the main view shows: ANIMATE the whole moving
                // model; PARENT the eligible parents; otherwise the
                // selection (plus its parent in SOCKET)
                if (pmedit_sel >= 0 && !pmedit_animate)
                {
                        if (pmedit_parent)
                        {
                                if (pmedit_cycle(i))
                                        continue;
                        }
                        else if (i != pmedit_sel && i != spar)
                                continue;
                }
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
        // PLACE ATTACHMENT POINT: the resting gizmo sits at the point, which
        // nudges can push outside the prism - keep it in frame too
        if (pmedit_joint && pmedit_sel >= 0 && !pmedit_animate)
        {
                unsigned char *o = mo->piece[pmedit_sel].origin;
                float p[4];
                mat4_f3_multiply(p, space[pmedit_sel], o[0], o[1], o[2]);
                for (int a = 0; a < 3; a++)
                {
                        if (p[a] < lo[a]) lo[a] = p[a];
                        if (p[a] > hi[a]) hi[a] = p[a];
                }
        }
        // nothing framable (PARENT with no eligible parents): the whole model
        if (lo[0] > hi[0]) for (int i = 0; i < pmedit_nr; i++)
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
        float cen[3] = { (lo[0]+hi[0])/2, (lo[1]+hi[1])/2, (lo[2]+hi[2])/2 };
        float radius = 0.5f * sqrtf((hi[0]-lo[0]) * (hi[0]-lo[0])
                                  + (hi[1]-lo[1]) * (hi[1]-lo[1])
                                  + (hi[2]-lo[2]) * (hi[2]-lo[2]));

        // the editor's own projection: a narrow FOV so the model reads
        // nearly orthographic (built here, before the render and the pick
        // rays both use it, so everything stays consistent)
        float ed_near = 100.f, ed_far = 1000.f * BS;
        float ed_frustw = ed_near * tanf(PMEDIT_FOV * PI / 360.f);
        float ed_frusth = ed_frustw * screenh / screenw;
        float pj[16] = {
                ed_near / ed_frustw,  0, 0, 0,
                0, -ed_near / ed_frusth,  0, 0,
                0, 0,        -ed_far / (ed_far - ed_near), -1,
                0, 0, -(ed_far * ed_near) / (ed_far - ed_near),  0,
        };
        memcpy(pmedit_proj, pj, sizeof pj);

        // proj * the leveled view (what pmedit_render draws with), kept for
        // projecting world points to the screen - the resize drag needs it
        {
                float fv[3], view[16];
                lookit(view, fv, peye0, peye1, peye2, 0, camplayer.yaw);
                translate(view, -peye0, -peye1, -peye2);
                mat4_multiply(pmedit_pv, pmedit_proj, view);
        }

        // zoom to fit: tan(half-fov) straight from the editor frustum
        float tanw = ed_frustw / ed_near;
        float tanh_ = ed_frusth / ed_near;
        float fill = pmedit_sel < 0 || pmedit_parent || pmedit_animate
                        ? 0.9f : 0.6f; // radius / half-extent
        // small pieces don't zoom in crazy far: floor the radius the fit
        // distance is computed from, so a tiny piece just sits smaller and
        // centered (the bbox framing above still uses the true size)
        float fit_radius = radius;
        if (pmedit_sel >= 0 && !pmedit_parent && !pmedit_animate)
        {
                float floor_r = PMEDIT_MIN_RADIUS_PX * PM_SCALE;
                if (fit_radius < floor_r) fit_radius = floor_r;
        }
        float dist = fit_radius / (fill * MIN(tanw, tanh_));
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

        // SELECT PARENT: the active piece moves to its own little view on
        // the left - same turntable spin, zoomed to the lonely piece - while
        // the main view keeps only the pieces that could become its parent.
        // Overwriting its pick matrix means clicks find it where it appears.
        float E2[16];
        if (pmedit_parent && pmedit_sel >= 0)
        {
                float lo2[3] = {1e9f, 1e9f, 1e9f}, hi2[3] = {-1e9f, -1e9f, -1e9f};
                for (int k = 0; k < 8; k++)
                {
                        float p[4];
                        mat4_f3_multiply(p, geom[pmedit_sel],
                                (k      & 1) * mo->piece[pmedit_sel].dims[0],
                                (k >> 1 & 1) * mo->piece[pmedit_sel].dims[1],
                                (k >> 2 & 1) * mo->piece[pmedit_sel].dims[2]);
                        for (int a = 0; a < 3; a++)
                        {
                                if (p[a] < lo2[a]) lo2[a] = p[a];
                                if (p[a] > hi2[a]) hi2[a] = p[a];
                        }
                }
                float cen2[3] = { (lo2[0]+hi2[0])/2, (lo2[1]+hi2[1])/2,
                                  (lo2[2]+hi2[2])/2 };
                float rad2 = 0.5f * sqrtf((hi2[0]-lo2[0]) * (hi2[0]-lo2[0])
                                        + (hi2[1]-lo2[1]) * (hi2[1]-lo2[1])
                                        + (hi2[2]-lo2[2]) * (hi2[2]-lo2[2]));
                float dist2 = rad2 / (0.28f * MIN(tanw, tanh_));
                if (dist2 < rad2 + 0.3f * BS) dist2 = rad2 + 0.3f * BS;
                float offx = -0.62f * tanw * dist2; // ~62% left of center
                // waiting for a parent, the piece hovers: a gentle bob, half
                // a texel of total travel at 1Hz, so it reads as unattached
                float bob = 0.25f * PM_SCALE * sinf(TAU * pmedit_bob_t);
                float q1[16], q2[16];
                pm_mat_translate(q1, -cen2[0], -cen2[1], -cen2[2]);
                mat4_multiply(q2, R, q1);
                pm_mat_translate(q1, peye0 + f[0] * dist2 + vm[0] * offx,
                                     peye1 + f[1] * dist2 + vm[4] * offx + bob,
                                     peye2 + f[2] * dist2 + vm[8] * offx);
                mat4_multiply(E2, q1, q2);
                mat4_multiply(pmedit_mats[pmedit_sel], E2, geom[pmedit_sel]);
        }

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
                // HIDDEN pieces and pieces the active gizmo mode ignores draw
                // faint in the ghost group below instead of solid here
                if (pmedit_hidden >> i & 1 || pmedit_mode_faint(mo, i)) continue;
                for (int fc = 0; fc < PM_FACES; fc++)
                        *b++ = PM_EDIT_FACE(pmedit_mats[i],
                                mo->piece[i].dims[0], mo->piece[i].dims[1], mo->piece[i].dims[2],
                                pmodel_tex_base + my_player * pm_slot_layers() + i * PM_FACES + fc,
                                0.9f, 0.8f);
        }
        pm_edit_rest_count = (int)(b - pmbuf) - pm_edit_rest_start;

        // faint pieces, 90% see-through on the same depth as the rest of the
        // model: the HIDDEN set (dead to clicks until UNHIDE) plus whatever the
        // active gizmo mode sets aside - kept as spatial reference, not gone
        pm_edit_hide_start = pm_edit_rest_start + pm_edit_rest_count;
        for (int i = 0; i < pmedit_nr; i++)
        {
                if (i == pmedit_sel) continue;
                if (pmedit_animate && mo->piece[i].type == PM_T_EYES
                                && pm_blinking(my_player, anim_t))
                        continue;
                if (!(pmedit_hidden >> i & 1 || pmedit_mode_faint(mo, i)))
                        continue;
                for (int fc = 0; fc < PM_FACES; fc++)
                {
                        *b = PM_EDIT_FACE(pmedit_mats[i],
                                mo->piece[i].dims[0], mo->piece[i].dims[1], mo->piece[i].dims[2],
                                pmodel_tex_base + my_player * pm_slot_layers() + i * PM_FACES + fc,
                                0.9f, 0.8f);
                        (b++)->tint = -0.1f; // ghost pipe reads alpha 0.1
                }
        }
        pm_edit_hide_count = (int)(b - pmbuf) - pm_edit_hide_start;

        // selection outline: the selected prism again, inflated by a pixel
        // membrane, in solid white (the extra layer past every slot's tiles).
        // Paint/TYPE modes draw it onto freshly cleared depth and the piece
        // over it, so the fatter silhouette shows as a rim around the piece;
        // JOINT/SOCKET use the depth-honest inverted hull instead, white on
        // the selection plus pink on the parent in SOCKET mode.
        pm_edit_hull_start = pm_edit_hide_start + pm_edit_hide_count;
        pm_edit_hull_count = 0;
        pm_edit_sel_count = 0;
        // MOVE PART: while the cursor is on the parent, the piece (rim and
        // all) leaves its current spot and shows as a see-through preview at
        // the exact flush spot a click would lay it (emitted with the gizmo
        // group below); off the parent it sits where it is, solid, and the
        // arrow keys nudge it. A root-parented piece has no parent surface,
        // so it always stays put.
        int sock_att[3];
        int sock_hot = pmedit_socket && pmedit_sel >= 0
                && pmedit_sock_aim(&mo->piece[pmedit_sel], sock_att);
        if (pmedit_sel >= 0)
        {
                struct pm_piece *pc = &mo->piece[pmedit_sel];
                if (pmedit_joint || pmedit_socket)
                {
                        if (!sock_hot)
                                pm_edit_hull_count = pmedit_shell(&b,
                                        pmedit_mats[pmedit_sel], pc,
                                        PM_LAYER_WHITE, 3.f); // reads white
                        if (pmedit_socket && pc->parent >= 0)
                                pm_edit_hull_count += pmedit_shell(&b,
                                        pmedit_mats[(int)pc->parent],
                                        &mo->piece[(int)pc->parent],
                                        PM_LAYER_PINK, 1.2f);
                }
                else if (pmedit_parent)
                {
                        // the piece sits alone in the left view wearing the
                        // white outline; pink candidate rims ride the gizmo
                        // group
                        pm_edit_hull_count = pmedit_shell(&b,
                                pmedit_mats[pmedit_sel], pc,
                                PM_LAYER_WHITE, 3.f); // reads white
                }
                else
                {
                        float e = 0.15f; // px of rim on every side - thin, so
                                         // the shell doesn't cover neighbors
                                         // where pieces touch
                        float t1[16], H[16];
                        pm_mat_translate(t1, -e, -e, -e);
                        mat4_multiply(H, pmedit_mats[pmedit_sel], t1);
                        for (int fc = 0; fc < PM_FACES; fc++)
                                *b++ = PM_EDIT_FACE(H,
                                        pc->dims[0] + 2*e, pc->dims[1] + 2*e,
                                        pc->dims[2] + 2*e,
                                        PM_LAYER_WHITE, 0.f, 3.f); // reads white
                        pm_edit_hull_count = PM_FACES;
                }

                if (!sock_hot)
                {
                        for (int fc = 0; fc < PM_FACES; fc++)
                                *b++ = PM_EDIT_FACE(pmedit_mats[pmedit_sel],
                                        pc->dims[0], pc->dims[1], pc->dims[2],
                                        pmodel_tex_base + my_player * pm_slot_layers()
                                                + pmedit_sel * PM_FACES + fc,
                                        0.9f, 0.8f);
                        pm_edit_sel_count = PM_FACES;
                }
        }
        pm_edit_sel_start = pm_edit_hull_start + pm_edit_hull_count;

        // JOINT gizmo: the rotation origin (in the piece's 16^3 SPACE - it
        // isn't corner-relative like the prism geometry) as a half-texel
        // cube in a cycling hue, with counter-cycling ruled axes running out
        // of its center in all 6 directions. MOVE PART draws no gizmo: the
        // see-through preview piece rides the cursor on the parent instead
        // (it lives in this group so the ghost pipeline can draw it last).
        pm_edit_joint_start = pm_edit_sel_start + pm_edit_sel_count;
        pm_edit_joint_count = 0;
        if (pmedit_sel >= 0)
        {
                struct pm_piece *pc = &mo->piece[pmedit_sel];
                float t1[16], G[16];
                // the arrow-key nudge steers by this frame in every mode,
                // including the plain selected view's MOVE PART-style nudge
                if (pmedit_joint)
                        mat4_multiply(pmedit_gizmo_mat, E, space[pmedit_sel]);
                else if (pc->parent >= 0)
                        mat4_multiply(pmedit_gizmo_mat, E, space[(int)pc->parent]);
                else
                        mat4_multiply(pmedit_gizmo_mat, E, root);

                if (pmedit_joint)
                {
                        // the gizmo (cube + ruled axes) rides the cursor
                        // whenever it's on the piece - showing exactly where
                        // a click would plant the point - and rests at the
                        // point itself otherwise (arrow nudges move it there)
                        int gface, gtu, gtv;
                        float gh[3];
                        int pt[3] = { pc->origin[0], pc->origin[1], pc->origin[2] };
                        if (pmedit_pick(pmedit_mx, pmedit_my, pmedit_sel, 0.f,
                                        &gface, &gtu, &gtv, gh) >= 0)
                                for (int a = 0; a < 3; a++)
                                        pt[a] = ICLAMP((int)roundf(pc->corner[a] + gh[a]),
                                                        0, PM_TILE);
                        int hue = pframe / 10 % PM_NR_HUES;

                        float c = 0.65f;
                        pm_mat_translate(t1, pt[0] - c/2, pt[1] - c/2, pt[2] - c/2);
                        mat4_multiply(G, pmedit_gizmo_mat, t1);
                        for (int fc = 0; fc < PM_FACES; fc++)
                                *b++ = PM_EDIT_FACE(G, c, c, c, PM_LAYER_HUES + hue, 0.f, 3.f);
                        pm_edit_joint_count = PM_FACES;

                        // the axes come as 1-texel segments alternating bright
                        // and dim, pinned to the piece space's texel grid, so
                        // distances read at a glance like a ruler
                        for (int a = 0; a < 3; a++)
                                for (int s = -20; s < 20; s++)
                                {
                                        float dims[3] = { 0.1f, 0.1f, 0.1f };
                                        dims[a] = 1.f;
                                        float off[3] = { pt[0] - 0.05f,
                                                pt[1] - 0.05f, pt[2] - 0.05f };
                                        off[a] = pt[a] + s;
                                        pm_mat_translate(t1, off[0], off[1], off[2]);
                                        mat4_multiply(G, pmedit_gizmo_mat, t1);
                                        for (int fc = 0; fc < PM_FACES; fc++)
                                                *b++ = PM_EDIT_FACE(G,
                                                        dims[0], dims[1], dims[2],
                                                        PM_LAYER_HUES + (hue + 3) % PM_NR_HUES,
                                                        0.f, (pt[a] + s) & 1 ? 3.f : 1.4f);
                                        pm_edit_joint_count += PM_FACES;
                                }
                }
                else if (sock_hot)
                {
                        // MOVE PART preview: the piece itself, half see-
                        // through, laid flush against the hovered parent face
                        // exactly where a click would put it (the transforms
                        // are all translations, so they collapse into one)
                        pm_mat_translate(t1,
                                sock_att[0] - pc->origin[0] + pc->corner[0],
                                sock_att[1] - pc->origin[1] + pc->corner[1],
                                sock_att[2] - pc->origin[2] + pc->corner[2]);
                        mat4_multiply(G, pmedit_gizmo_mat, t1);
                        for (int fc = 0; fc < PM_FACES; fc++)
                        {
                                *b = PM_EDIT_FACE(G,
                                        pc->dims[0], pc->dims[1], pc->dims[2],
                                        pmodel_tex_base + my_player * pm_slot_layers()
                                                + pmedit_sel * PM_FACES + fc,
                                        0.9f, 0.8f);
                                (b++)->tint = -0.5f; // ghost pipe reads alpha 0.5
                        }
                        pm_edit_joint_count += PM_FACES;
                }
                else if (pmedit_resize && pmedit_face)
                {
                        // RESIZE: the picked face wears a see-through slab in
                        // a cycling hue - the paint reads through it - so you
                        // can see what the arrows will push and pull
                        int fax = pmedit_axis_of[pmedit_face];
                        int hue = pframe / 10 % PM_NR_HUES;
                        float e = 0.15f, th = 0.2f;
                        float off[3] = { -e, -e, -e };
                        float dims[3] = { pc->dims[0] + 2*e, pc->dims[1] + 2*e,
                                          pc->dims[2] + 2*e };
                        dims[fax] = th;
                        off[fax] = pmedit_side_of[pmedit_face] > 0
                                        ? pc->dims[fax] + 0.02f : -th - 0.02f;
                        pm_mat_translate(t1, off[0], off[1], off[2]);
                        mat4_multiply(G, pmedit_mats[pmedit_sel], t1);
                        for (int fc = 0; fc < PM_FACES; fc++)
                        {
                                *b = PM_EDIT_FACE(G, dims[0], dims[1], dims[2],
                                        PM_LAYER_HUES + hue, 0.f, 3.f);
                                (b++)->tint = -0.45f; // ghost pipe: alpha 0.45
                        }
                        pm_edit_joint_count += PM_FACES;
                }
        }

        // PARENT mode: no gizmo, no ghost - just a pink rim on the eligible
        // parent under the cursor; the click re-wires to it and lays the
        // piece flush right there
        if (pmedit_sel >= 0 && pmedit_parent)
        {
                int gface, gtu, gtv;
                float gh[3];
                int hov = pmedit_pick(pmedit_mx, pmedit_my,
                                PMEDIT_PICK_ELIG, 0.f,
                                &gface, &gtu, &gtv, gh);
                if (hov >= 0)
                        pm_edit_joint_count += pmedit_shell(&b,
                                pmedit_mats[hov], &mo->piece[hov],
                                PM_LAYER_PINK, 1.2f);
        }

        // ground reference: one block's top face as a half-transparent dark
        // green quad at the in-game ground level (the feet), so a floating
        // or sunken model reads at a glance. Drawn after the model, blended
        // on its depth: a sunken model shows the floor slicing through it.
        // Only in the views that show the whole model, and only near level -
        // steep pitches would turn it into a big screen-covering sheet. MOVE
        // PART on a root-parented piece has no parent surface to reference, so
        // it borrows the hitbox wireframe (below) as a nudging frame
        pm_edit_floor_start = pm_edit_joint_start + pm_edit_joint_count;
        pm_edit_floor_count = 0;
        int floor_view = !pmedit_joint && !pmedit_socket && !pmedit_parent;
        int nudge_ref = pmedit_socket && pmedit_sel >= 0
                        && mo->piece[pmedit_sel].parent < 0;
        if (floor_view || nudge_ref)
        {
                if (floor_view && fabsf(pmedit_mpitch) <= TAU / 12)
                {
                        float ft[16], FG[16];
                        pm_mat_translate(ft, -BS / 2.f, PLYR_H / 2.f, -BS / 2.f);
                        mat4_multiply(FG, E, ft);
                        int fc = 0; // the UP face only: a single quad
                        *b = PM_EDIT_FACE(FG, BS, 0, BS, PM_LAYER_DKGREEN, 0.f, 3.f);
                        (b++)->tint = -0.5f; // ghost pipe reads alpha 0.5
                        pm_edit_floor_count = 1;
                }

                // hitbox wireframe: 12 thin bars tracing the player's
                // PLYR_W x PLYR_H x PLYR_W collision box in the same
                // see-through dark green as the floor. The box is square in
                // plan, so z reuses the x numbers; the y and z bars are
                // inset one thickness so no two bars share volume (which
                // would double-blend)
                const float T = 24.f;
                const float cx = -PLYR_W / 2.f, cy = -PLYR_H / 2.f;
                const float X = PLYR_W - T, Y = PLYR_H - T;
                const struct { float p[3], d[3]; } bar[12] = {
                        {{ cx,     cy,     cx     }, { PLYR_W, T, T }},
                        {{ cx,     cy,     cx + X }, { PLYR_W, T, T }},
                        {{ cx,     cy + Y, cx     }, { PLYR_W, T, T }},
                        {{ cx,     cy + Y, cx + X }, { PLYR_W, T, T }},
                        {{ cx,     cy + T, cx     }, { T, PLYR_H - 2 * T, T }},
                        {{ cx,     cy + T, cx + X }, { T, PLYR_H - 2 * T, T }},
                        {{ cx + X, cy + T, cx     }, { T, PLYR_H - 2 * T, T }},
                        {{ cx + X, cy + T, cx + X }, { T, PLYR_H - 2 * T, T }},
                        {{ cx,     cy,     cx + T }, { T, T, PLYR_W - 2 * T }},
                        {{ cx,     cy + Y, cx + T }, { T, T, PLYR_W - 2 * T }},
                        {{ cx + X, cy,     cx + T }, { T, T, PLYR_W - 2 * T }},
                        {{ cx + X, cy + Y, cx + T }, { T, T, PLYR_W - 2 * T }},
                };
                for (int e = 0; e < 12; e++)
                {
                        float bt[16], BM[16];
                        pm_mat_translate(bt, bar[e].p[0], bar[e].p[1], bar[e].p[2]);
                        mat4_multiply(BM, E, bt);
                        for (int fc = 0; fc < PM_FACES; fc++)
                        {
                                *b = PM_EDIT_FACE(BM,
                                        bar[e].d[0], bar[e].d[1], bar[e].d[2],
                                        PM_LAYER_DKGREEN, 0.f, 3.f);
                                (b++)->tint = -0.5f; // matches the floor
                        }
                }
                pm_edit_floor_count += 12 * PM_FACES;
        }
        #undef PM_EDIT_FACE

        polys += b - pmbuf - pm_edit_rest_start;
        return b;
}

// bind the blend pipe and draw the faint reference geometry - hidden and
// mode-set-aside pieces plus the ground quad and hitbox frame - then hand the
// solid pipe back. The ghost pipe writes no depth, so this reads see-through
// and never occludes the opaque model already laid down under it
static void pmedit_draw_faint(VkCommandBuffer cmdbuf, const void *push, unsigned pushsz)
{
        vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS,
                        vk.pipelines[pmodel_ghost_pipe].pipeline);
        vkCmdPushConstants(cmdbuf, vk.pipelines[pmodel_ghost_pipe].layout,
                        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                        0, pushsz, push);
        if (pm_edit_hide_count)
                vkCmdDraw(cmdbuf, 4, pm_edit_hide_count, 0, pm_edit_hide_start);
        if (pm_edit_floor_count)
                vkCmdDraw(cmdbuf, 4, pm_edit_floor_count, 0, pm_edit_floor_start);
        vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS,
                        vk.pipelines[pmodel_pipe].pipeline);
        vkCmdPushConstants(cmdbuf, vk.pipelines[pmodel_pipe].layout,
                        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                        0, pushsz, push);
}

// draw the preview over the finished world: clear depth so it always wins,
// the unselected pieces first, then (on cleared depth each) the white outline
// hull and the selected piece over it, so the selection always reads in full
// ---- picker geometry + render ---------------------------------------------
static struct allocation pmedit_pick_alloc[MAX_FRAMES_IN_FLIGHT];
static struct pmvert pmedit_pick_buf[PM_NR_PREVIEW * PM_MAX_PIECES * PM_FACES];
static int pmedit_pick_start[PM_NR_PREVIEW]; // per-cell instance range in the buf
static int pmedit_pick_ct[PM_NR_PREVIEW];
static int pmedit_pick_total;                // instances filled this frame
static float pmedit_pick_pv[PM_NR_PREVIEW][16]; // per-cell proj*view

// build the page's thumbnail instances and per-cell cameras. Each loaded model
// resolves to a plain standing pose centered at the origin (spun by the shared
// turntable); its own narrow-FOV camera frames it to its bounding box, and its
// faces sample its preview texture slot.
static void pmedit_pick_emit(void)
{
        struct pmvert *b = pmedit_pick_buf;
        for (int c = 0; c < PM_NR_PREVIEW; c++)
        {
                pmedit_pick_start[c] = b - pmedit_pick_buf;
                pmedit_pick_ct[c] = 0;
                if (!pmedit_pick_loaded[c]) continue;
                struct pmodel *mo = &pmedit_pick_model[c];
                float space[PM_MAX_PIECES][16], geom[PM_MAX_PIECES][16];
                pm_resolve(mo, -PLYR_W / 2.f, -PLYR_H / 2.f, -PLYR_W / 2.f,
                                pmedit_pick_spin, NULL, space, geom, NULL);

                // the standing pose's bounding box (model frame == world here)
                float lo[3] = {1e9f, 1e9f, 1e9f}, hi[3] = {-1e9f, -1e9f, -1e9f};
                for (int i = 0; i < mo->nr_pieces; i++)
                for (int k = 0; k < 8; k++)
                {
                        float p[4];
                        mat4_f3_multiply(p, geom[i],
                                (k      & 1) * mo->piece[i].dims[0],
                                (k >> 1 & 1) * mo->piece[i].dims[1],
                                (k >> 2 & 1) * mo->piece[i].dims[2]);
                        for (int a = 0; a < 3; a++)
                        {
                                if (p[a] < lo[a]) lo[a] = p[a];
                                if (p[a] > hi[a]) hi[a] = p[a];
                        }
                }
                float cen[3] = {(lo[0]+hi[0])/2, (lo[1]+hi[1])/2, (lo[2]+hi[2])/2};
                float radius = 0.5f * sqrtf((hi[0]-lo[0]) * (hi[0]-lo[0])
                                          + (hi[1]-lo[1]) * (hi[1]-lo[1])
                                          + (hi[2]-lo[2]) * (hi[2]-lo[2]));
                if (radius < 1.f) radius = 1.f;

                // the cell camera: a narrow FOV at the cell's aspect (near
                // orthographic), the eye lifted and pulled back for a 3/4 read
                float r[4]; pmedit_pick_cell_rect(c, r);
                float cw = r[2] - r[0], chh = r[3] - r[1];
                float fov = 24.f, ednear = 100.f, edfar = 1000.f * BS;
                float fw = ednear * tanf(fov * PI / 360.f), fh = fw * chh / cw;
                float pj[16] = {
                        ednear / fw, 0, 0, 0,
                        0, -ednear / fh, 0, 0,
                        0, 0, -edfar / (edfar - ednear), -1,
                        0, 0, -(edfar * ednear) / (edfar - ednear), 0,
                };
                float dist = radius / (0.72f * MIN(fw, fh) / ednear);
                if (dist < radius + 0.3f * BS) dist = radius + 0.3f * BS;

                // sit the model at the real camera eye, so main.frag's shared
                // fog and shadows (keyed on distance from ubo.view_pos) read
                // near zero - otherwise the thumbnails wear the flat sunset-fog
                // tint. Shift every piece's world translation by the same delta;
                // the cell's own camera frames it from there.
                float delta[3] = { peye0 - cen[0], peye1 - cen[1], peye2 - cen[2] };
                for (int i = 0; i < mo->nr_pieces; i++)
                {
                        geom[i][12] += delta[0];
                        geom[i][13] += delta[1];
                        geom[i][14] += delta[2];
                }
                // y is down: a smaller y is higher, so lift the eye by -y
                float eye[3] = { peye0, peye1 - 0.42f * dist, peye2 - 0.90f * dist };
                float f3[3] = { peye0, peye1, peye2 }, view[16];
                lookit(view, f3, eye[0], eye[1], eye[2], NO_PITCH, 0);
                translate(view, -eye[0], -eye[1], -eye[2]);
                mat4_multiply(pmedit_pick_pv[c], pj, view);

                for (int i = 0; i < mo->nr_pieces; i++)
                {
                        float *m = geom[i];
                        for (int f = 0; f < PM_FACES; f++)
                                *b++ = (struct pmvert){
                                        .r0 = { m[0], m[4], m[8],  m[12] },
                                        .r1 = { m[1], m[5], m[9],  m[13] },
                                        .r2 = { m[2], m[6], m[10], m[14] },
                                        .dims = { mo->piece[i].dims[0],
                                                  mo->piece[i].dims[1],
                                                  mo->piece[i].dims[2] },
                                        .orient = f + 1,
                                        .tex = PM_LAYER_PREVIEW + c * pm_slot_layers()
                                                + i * PM_FACES + f,
                                        .illum = 0.9f, .glow = 0.f,
                                };
                        pmedit_pick_ct[c] += PM_FACES;
                }
        }
        pmedit_pick_total = b - pmedit_pick_buf;
}

static void pmedit_pick_render(VkCommandBuffer cmdbuf)
{
        pmedit_pick_emit();
        int fr = vk.currentFrame;
        if (!pmedit_pick_alloc[fr].buf)
                vulkan_allocate_vertex_buffer(sizeof pmedit_pick_buf, &pmedit_pick_alloc[fr]);
        if (pmedit_pick_total)
                vulkan_populate_vertex_buffer(pmedit_pick_buf,
                        pmedit_pick_total * sizeof(struct pmvert), &pmedit_pick_alloc[fr]);

        // solid dark modal backdrop, then each visible cell's panel, hover lit
        static struct allocation bg[MAX_FRAMES_IN_FLIGHT], cellbg[MAX_FRAMES_IN_FLIGHT];
        static struct allocation hi[MAX_FRAMES_IN_FLIGHT];
        float full[1][4] = {{ 0, 0, (float)screenw, (float)screenh }};
        pmedit_fill_rects(cmdbuf, bg, full, 1, (float[3]){ 0.05f, 0.05f, 0.07f });
        float cells[PM_NR_PREVIEW][4];
        int nc = 0;
        for (int c = 0; c < PM_NR_PREVIEW; c++)
        {
                if (pmedit_pick_page * PM_NR_PREVIEW + c >= pmedit_pick_nr) continue;
                pmedit_pick_cell_rect(c, cells[nc++]);
        }
        pmedit_fill_rects(cmdbuf, cellbg, cells, nc, (float[3]){ 0.12f, 0.12f, 0.15f });
        int hov = pmedit_pick_hover();
        if (hov >= 0)
        {
                float hr[1][4]; pmedit_pick_cell_rect(hov, hr[0]);
                pmedit_fill_rects(cmdbuf, hi, hr, 1, (float[3]){ 0.20f, 0.23f, 0.32f });
        }

        // the thumbnails: each in its own cell viewport, on its own cleared depth
        if (pmedit_pick_total)
        {
                vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                vk.pipelines[pmodel_pipe].pipeline);
                VkDeviceSize offset = 0;
                vkCmdBindVertexBuffers(cmdbuf, 0, 1, &pmedit_pick_alloc[fr].buf, &offset);
                for (int c = 0; c < PM_NR_PREVIEW; c++)
                {
                        if (!pmedit_pick_ct[c]) continue;
                        float r[4]; pmedit_pick_cell_rect(c, r);
                        VkViewport vp = { r[0], r[1], r[2] - r[0], r[3] - r[1], 0, 1 };
                        VkRect2D sc = { {(int)r[0], (int)r[1]},
                                {(uint32_t)(r[2] - r[0]), (uint32_t)(r[3] - r[1])} };
                        vkCmdSetViewport(cmdbuf, 0, 1, &vp);
                        vkCmdSetScissor(cmdbuf, 0, 1, &sc);
                        VkClearAttachment ca = { .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                                .clearValue.depthStencil = {1.f, 0} };
                        VkClearRect cr = { .rect = sc, .baseArrayLayer = 0, .layerCount = 1 };
                        vkCmdClearAttachments(cmdbuf, 1, &ca, 1, &cr);
                        struct { float pv[16]; } push;
                        memcpy(push.pv, pmedit_pick_pv[c], sizeof push.pv);
                        vkCmdPushConstants(cmdbuf, vk.pipelines[pmodel_pipe].layout,
                                        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                                        0, sizeof push, &push);
                        vkCmdDraw(cmdbuf, 4, pmedit_pick_ct[c], 0, pmedit_pick_start[c]);
                }
                // hand a full-screen viewport/scissor back to the UI passes
                VkViewport vp = { 0, 0, screenw, screenh, 0, 1 };
                VkRect2D sc = { {0, 0}, {screenw, screenh} };
                vkCmdSetViewport(cmdbuf, 0, 1, &vp);
                vkCmdSetScissor(cmdbuf, 0, 1, &sc);
        }
}

void pmedit_render(VkCommandBuffer cmdbuf)
{
        if (!pmedit_on) return;
        if (pmedit_picker) { pmedit_pick_render(cmdbuf); return; }
        if (!(pm_edit_rest_count + pm_edit_sel_count + pm_edit_hide_count)) return;

        // the preview renders through the editor's leveled camera, not the
        // world's (which keeps whatever pitch the player froze it at)
        float f[3], pv[16], view[16];
        lookit(view, f, peye0, peye1, peye2, 0, camplayer.yaw);
        translate(view, -peye0, -peye1, -peye2);
        mat4_multiply(pv, pmedit_proj, view); // the editor's flat projection

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

        // JOINT/SOCKET/PARENT clear depth once and share it: pieces overlap
        // honestly and the highlights are inverted hulls (PARENT's views
        // don't overlap on screen anyway). Paint/TYPE modes clear per group
        // so the selection always reads in full, in front.
        int layers[3][2] = {
                { pm_edit_rest_start, pm_edit_rest_count },
                { pm_edit_hull_start, pm_edit_hull_count },
                { pm_edit_sel_start,  pm_edit_sel_count  },
        };
        int shared = pmedit_joint || pmedit_socket || pmedit_parent;
        int cleared = 0;
        for (int i = 0; i < 3; i++)
        {
                // paint/TYPE clears depth per group, so the faint geometry
                // (hidden pieces, ground quad, hitbox frame) has to ride the
                // unselected group's depth right here to occlude true: the
                // floor slices a sunken model, the frame hides behind pieces
                // in front. Shared gizmo modes clear once and draw it LAST
                // (below) so a faint piece layers over the active one rather
                // than hiding it
                if (!shared && i == 1 && (pm_edit_hide_count || pm_edit_floor_count))
                {
                        if (!cleared)
                        {
                                vkCmdClearAttachments(cmdbuf, 1, &ca, 1, &cr);
                                cleared = 1;
                        }
                        pmedit_draw_faint(cmdbuf, &push, sizeof push);
                }
                if (!layers[i][1]) continue;
                if (!shared || !cleared)
                {
                        vkCmdClearAttachments(cmdbuf, 1, &ca, 1, &cr);
                        cleared = 1;
                }
                vkCmdDraw(cmdbuf, 4, layers[i][1], 0, layers[i][0]);
        }
        // shared gizmo modes: the faint reference geometry draws last, after
        // the whole opaque model, so it never occludes the active piece - it
        // blends over it, honestly see-through (the ghost pipe writes no depth)
        if (shared && (pm_edit_hide_count || pm_edit_floor_count))
        {
                if (!cleared)
                {
                        vkCmdClearAttachments(cmdbuf, 1, &ca, 1, &cr);
                        cleared = 1;
                }
                pmedit_draw_faint(cmdbuf, &push, sizeof push);
        }
        // the gizmo shares the last layer's depth: where the cube and axis
        // lines pierce the piece's surface shows exactly, no x-ray. MOVE
        // PART's group is the see-through preview, and RESIZE's is the
        // picked face's see-through slab - swap in the blend pipe
        if (pm_edit_joint_count)
        {
                if (pmedit_socket || pmedit_resize)
                {
                        vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                        vk.pipelines[pmodel_ghost_pipe].pipeline);
                        vkCmdPushConstants(cmdbuf, vk.pipelines[pmodel_ghost_pipe].layout,
                                        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                                        0, sizeof push, &push);
                }
                vkCmdDraw(cmdbuf, 4, pm_edit_joint_count, 0, pm_edit_joint_start);
        }
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
        if (!pmedit_proj[0] || !pmedit_proj[5]) return -1;
        struct pmodel *mo = &pm_models[my_player];

        // cursor -> world ray, through the editor's own leveled frustum
        // (the same narrow-FOV projection the model is drawn with)
        float f[3], vm[16];
        lookit(vm, f, 0, 0, 0, 0, camplayer.yaw);
        float kx = (2.f * mx / screenw - 1.f) / pmedit_proj[0];
        float ky = (2.f * my / screenh - 1.f) / pmedit_proj[5];
        float D[3] = { f[0] + vm[0] * kx + vm[1] * ky,
                       f[1] + vm[4] * kx + vm[5] * ky,
                       f[2] + vm[8] * kx + vm[9] * ky };
        float O[3] = { peye0, peye1, peye2 };

        int best = -1;
        float best_t = 1e30f;
        int i0 = only < 0 ? 0 : only;
        int i1 = only < 0 ? pmedit_nr - 1 : only;
        int skip = only < -1 && only != PMEDIT_PICK_ELIG
                        ? -2 - only : -1; // only = -2-i: all EXCEPT i
        for (int i = i0; i <= i1; i++)
        {
                if (i == skip) continue;
                if (pmedit_hidden >> i & 1) continue; // hidden: dead to rays
                if (only == PMEDIT_PICK_ELIG && pmedit_cycle(i)) continue;
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

// paint the texel under the cursor with the selected swatch's slot; only
// exact hits on the piece's real surface paint. Returns whether the ray hit.
static int pmedit_paint()
{
        int face, tu, tv;
        if (pmedit_sel < 0) return 0;
        if (pmedit_pick(pmedit_mx, pmedit_my, pmedit_sel, 0.f, &face, &tu, &tv, NULL) < 0)
                return 0;
        unsigned char c = pmedit_color;
        unsigned char *t = pm_models[my_player].texel[pmedit_sel][face - 1];
        int at = tv * PM_TILE + tu;
        if (PM_TEXGET(t, at) != c)
        {
                PM_TEXSET(t, at, c);
                pmodel_upload(my_player);
        }
        return 1;
}

// the flood modes' click: FLOOD FILL repaints the clicked texel's connected
// same-color region on that face (4-way, walled by the face's visible
// extent); SUPER FLOOD recoats every texel on all six faces - the full
// 16x16 tiles, not just the visible extents, so a later RESIZE grows into
// the same color. Returns whether the ray hit the piece.
static int pmedit_flood_do()
{
        int face, tu, tv;
        if (pmedit_sel < 0) return 0;
        if (pmedit_pick(pmedit_mx, pmedit_my, pmedit_sel, 0.f, &face, &tu, &tv, NULL) < 0)
                return 0;
        struct pmodel *mo = &pm_models[my_player];
        unsigned char c = pmedit_color;
        if (pmedit_flood == 2)
        {
                memset(mo->texel[pmedit_sel], c | c << 4,
                                sizeof mo->texel[pmedit_sel]);
                pmodel_upload(my_player);
                return 1;
        }
        unsigned char *t = mo->texel[pmedit_sel][face - 1];
        int from = PM_TEXGET(t, tv * PM_TILE + tu);
        if (from == c) return 1;
        int eu, ev;
        pm_face_extent(&mo->piece[pmedit_sel], face, &eu, &ev);
        // cells recolor as they're pushed, so each enters the stack once
        short stack[PM_TILE * PM_TILE];
        int top = 0;
        stack[top++] = tv * PM_TILE + tu;
        PM_TEXSET(t, tv * PM_TILE + tu, c);
        while (top)
        {
                int at2 = stack[--top], u = at2 % PM_TILE, v = at2 / PM_TILE;
                static const signed char du[4] = { 1, -1, 0, 0 },
                                         dv[4] = { 0, 0, 1, -1 };
                for (int k = 0; k < 4; k++)
                {
                        int nu = u + du[k], nv = v + dv[k];
                        if (nu < 0 || nv < 0 || nu >= eu || nv >= ev)
                                continue;
                        int nat = nv * PM_TILE + nu;
                        if (PM_TEXGET(t, nat) != from) continue;
                        PM_TEXSET(t, nat, c);
                        stack[top++] = nat;
                }
        }
        pmodel_upload(my_player);
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
                if (btn == SDL_BUTTON_LEFT) pmedit_pal_drag = pmedit_resize_drag = 0;
                return;
        }
        if (btn != SDL_BUTTON_LEFT && btn != SDL_BUTTON_RIGHT) return;

        // the picker overlay eats every click while it's up
        if (pmedit_picker) { pmedit_pick_click(btn); return; }

        // LOAD opens the picker (whole-model view only)
        if (pmedit_in_changemodel_btn(pmedit_mx, pmedit_my))
        {
                if (btn == SDL_BUTTON_LEFT) pmedit_picker_open();
                return;
        }

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

        if (pmedit_in_hide_btn(pmedit_mx, pmedit_my))
        {
                if (btn != SDL_BUTTON_LEFT) return;
                if (pmedit_hide) { pmedit_hide = 0; return; } // done hiding
                if (pmedit_hidden) { pmedit_hidden = 0; return; } // UNHIDE (n)
                // HIDE mode: back out to the whole model, then every piece
                // clicked ghosts away until the click lands somewhere else
                pmedit_newpart_cancel();
                pmedit_hide = 1;
                pmedit_sel = -1;
                pmedit_joint = pmedit_socket = pmedit_parent = 0;
                pmedit_resize = pmedit_face = pmedit_restang = 0;
                return;
        }

        if (pmedit_in_joint_btn(pmedit_mx, pmedit_my))
        {
                if (btn == SDL_BUTTON_LEFT)
                {
                        pmedit_joint = !pmedit_joint;
                        pmedit_socket = pmedit_parent = pmedit_newpart = 0;
                        pmedit_resize = pmedit_face = pmedit_restang = 0;
                }
                return;
        }
        if (pmedit_in_socket_btn(pmedit_mx, pmedit_my))
        {
                if (btn == SDL_BUTTON_LEFT)
                {
                        pmedit_socket = !pmedit_socket;
                        pmedit_joint = pmedit_parent = pmedit_newpart = 0;
                        pmedit_resize = pmedit_face = pmedit_restang = 0;
                }
                return;
        }
        if (pmedit_in_parent_btn(pmedit_mx, pmedit_my))
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
        if (pmedit_in_type_btn(pmedit_mx, pmedit_my))
        {
                // left cycles forward, right cycles back
                unsigned char *ty = &pm_models[my_player].piece[pmedit_sel].type;
                *ty = (*ty + (btn == SDL_BUTTON_LEFT ? 1 : PM_T_COUNT - 1))
                                % PM_T_COUNT;
                return;
        }
        if (pmedit_in_resize_btn(pmedit_mx, pmedit_my))
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
        if (pmedit_in_restang_btn(pmedit_mx, pmedit_my))
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
        if (pmedit_in_copy_btn(pmedit_mx, pmedit_my))
        {
                if (btn == SDL_BUTTON_LEFT) pmedit_copy();
                return;
        }
        if (pmedit_in_delete_btn(pmedit_mx, pmedit_my))
        {
                if (btn == SDL_BUTTON_LEFT) pmedit_delete();
                return;
        }
        if (pmedit_in_newpart_btn(pmedit_mx, pmedit_my))
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
        if (pmedit_in_animate_btn(pmedit_mx, pmedit_my))
        {
                // keeps the selection and modes: stopping puts you right
                // back where you were
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

// a world point through the editor's view-projection to screen pixels
// (top-left origin, matching the cursor). Returns 0 behind the eye.
static int pmedit_to_screen(const float *w, float *sx, float *sy)
{
        float c[4];
        for (int r = 0; r < 4; r++)
                c[r] = pmedit_pv[r] * w[0] + pmedit_pv[r + 4] * w[1]
                     + pmedit_pv[r + 8] * w[2] + pmedit_pv[r + 12];
        if (c[3] <= 1e-4f) return 0;
        *sx = (c[0] / c[3] * 0.5f + 0.5f) * screenw;
        *sy = (c[1] / c[3] * 0.5f + 0.5f) * screenh;
        return 1;
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

// grey backdrops behind the button labels so they read as buttons - solid 2D
// rects on the cursor pipeline, recorded before the text batches so the
// labels land on top. The boxes ARE the hit rects.
static void pmedit_boxes()
{
        float r[12][4];
        int nr = 0;
        for (int i = 0; i < 8; i++) // the piece buttons, DELETE last
        {
                r[nr][0] = PMEDIT_BTN_X - 10; r[nr][1] = 16 + 64 * i;
                r[nr][2] = screenw - 8;       r[nr][3] = 76 + 64 * i;
                nr++;
        }
        for (int i = 0; i < 4; i++) // NEW PART, ANIMATE, STYLE, HIDE: always live
        {
                r[nr][0] = PMEDIT_BTN_X - 10; r[nr][1] = 528 + 64 * i;
                r[nr][2] = screenw - 8;       r[nr][3] = 588 + 64 * i;
                nr++;
        }

        float buf[12 * 12], *p = buf;
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
        vkCmdDraw(cmdbuf, (nr - 4) * 6, 1, 0, 0); // piece buttons

        memcpy(push.color, (float[3]){ 0.13f, 0.13f, 0.16f }, sizeof push.color);
        vkCmdPushConstants(cmdbuf, vk.pipelines[cursor_pipe].layout,
                        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                        0, sizeof push, &push);
        vkCmdDraw(cmdbuf, 24, 1, (nr - 4) * 6, 0); // the always-live buttons
}

// one quad into the palette panel's vertex stream, a color per corner -
// same corners, one color = a solid rect; the picker's gradient segments
// use all four
static float *pmedit_quad4(float *p, float x0, float y0, float x1, float y1,
                const float *c00, const float *c10,
                const float *c01, const float *c11)
{
        const struct { float x, y; const float *c; } v[6] = {
                {x0, y0, c00}, {x1, y1, c11}, {x1, y0, c10},
                {x1, y1, c11}, {x0, y0, c00}, {x0, y1, c01},
        };
        for (int i = 0; i < 6; i++)
        {
                *p++ = v[i].x;    *p++ = v[i].y;
                *p++ = v[i].c[0]; *p++ = v[i].c[1];
                *p++ = v[i].c[2]; *p++ = v[i].c[3];
        }
        return p;
}

static float *pmedit_quad(float *p, float x0, float y0, float x1, float y1,
                const float *c)
{
        return pmedit_quad4(p, x0, y0, x1, y1, c, c, c, c);
}

// build and draw the palette panel: backdrop, the 16 swatches (a white ring
// on the selected one, a checker on transparent), and the hue-lightness
// picker as six vertex-colored segments - between adjacent primary and
// secondary corners linear interpolation IS the hue wheel, so the gradient
// is exact, and pmedit_pal_pick computes the same colors the pixels show
static void pmedit_palette_ui()
{
        struct pmodel *mo = &pm_models[my_player];
        static float buf[34 * 6 * 6]; // 34 quads is the panel's worst case
        float *p = buf;

        p = pmedit_quad(p, PMEDIT_SW_X0 - PMEDIT_PAL_PAD,
                        PMEDIT_SW_Y0 - PMEDIT_PAL_PAD,
                        PMEDIT_HSL_X1 + PMEDIT_PAL_PAD,
                        PMEDIT_SF_Y1 + PMEDIT_PAL_PAD,
                        (float[4]){ 0.08f, 0.08f, 0.10f, 1 });

        // the flood buttons' boxes; their labels ride the font pass and
        // read yellow while the mode is on, like the right-hand column
        p = pmedit_quad(p, PMEDIT_HSL_X0, PMEDIT_FF_Y0,
                        PMEDIT_HSL_X1, PMEDIT_FF_Y1,
                        (float[4]){ 0.13f, 0.13f, 0.16f, 1 });
        p = pmedit_quad(p, PMEDIT_HSL_X0, PMEDIT_SF_Y0,
                        PMEDIT_HSL_X1, PMEDIT_SF_Y1,
                        (float[4]){ 0.13f, 0.13f, 0.16f, 1 });

        for (int i = 0; i < PM_NR_COLORS; i++)
        {
                float x0 = PMEDIT_SW_X0 + i % 8 * PMEDIT_SW_STRIDE;
                float y0 = PMEDIT_SW_Y0 + i / 8 * PMEDIT_SW_STRIDE;
                float x1 = x0 + PMEDIT_SW_SZ, y1 = y0 + PMEDIT_SW_SZ;
                if (i == pmedit_color) // the ring: a bigger white rect under
                        p = pmedit_quad(p, x0 - 3, y0 - 3, x1 + 3, y1 + 3,
                                        (float[4]){ 1, 1, 1, 1 });
                if (!i) // transparent wears the classic light/dark checker
                {
                        float xm = x0 + PMEDIT_SW_SZ / 2.f;
                        float ym = y0 + PMEDIT_SW_SZ / 2.f;
                        p = pmedit_quad(p, x0, y0, x1, y1,
                                        (float[4]){ 0.75f, 0.75f, 0.75f, 1 });
                        p = pmedit_quad(p, x0, y0, xm, ym,
                                        (float[4]){ 0.4f, 0.4f, 0.4f, 1 });
                        p = pmedit_quad(p, xm, ym, x1, y1,
                                        (float[4]){ 0.4f, 0.4f, 0.4f, 1 });
                        continue;
                }
                unsigned c = mo->palette[i];
                p = pmedit_quad(p, x0, y0, x1, y1, (float[4]){
                                (c       & 255) / 255.f,
                                (c >>  8 & 255) / 255.f,
                                (c >> 16 & 255) / 255.f, 1 });
        }

        float ym = (PMEDIT_HSL_Y0 + PMEDIT_HSL_Y1) / 2.f;
        const float wht[4] = { 1, 1, 1, 1 }, blk[4] = { 0, 0, 0, 1 };
        for (int k = 0; k < 6; k++)
        {
                float x0 = PMEDIT_HSL_X0
                        + (PMEDIT_HSL_X1 - PMEDIT_HSL_X0) * k / 6.f;
                float x1 = PMEDIT_HSL_X0
                        + (PMEDIT_HSL_X1 - PMEDIT_HSL_X0) * (k + 1) / 6.f;
                float h0[4] = { pmedit_hue6[k][0], pmedit_hue6[k][1],
                                pmedit_hue6[k][2], 1 };
                float h1[4] = { pmedit_hue6[k + 1][0], pmedit_hue6[k + 1][1],
                                pmedit_hue6[k + 1][2], 1 };
                p = pmedit_quad4(p, x0, PMEDIT_HSL_Y0, x1, ym,
                                wht, wht, h0, h1);
                p = pmedit_quad4(p, x0, ym, x1, PMEDIT_HSL_Y1,
                                h0, h1, blk, blk);
        }

        static struct allocation alloc[MAX_FRAMES_IN_FLIGHT];
        if (!alloc[vk.currentFrame].buf)
                vulkan_allocate_vertex_buffer(sizeof buf, &alloc[vk.currentFrame]);
        vulkan_populate_vertex_buffer(buf, (p - buf) * sizeof *buf,
                        &alloc[vk.currentFrame]);

        VkCommandBuffer cmdbuf = vk.commandBuffers[vk.imageIndex];
        vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS,
                        vk.pipelines[pmui_pipe].pipeline);
        VkViewport viewport = { 0, 0, screenw, screenh, 0, 1 };
        VkRect2D scissor = { {0, 0}, {screenw, screenh} };
        vkCmdSetViewport(cmdbuf, 0, 1, &viewport);
        vkCmdSetScissor(cmdbuf, 0, 1, &scissor);

        struct { float proj[16]; } push = {
                .proj = { 2.f / screenw, 0, 0, 0,
                          0, 2.f / screenh, 0, 0,
                          0, 0, 1, 0,
                         -1, -1, 0, 1 }, // pixel space, y 0 at the top
        };
        vkCmdPushConstants(cmdbuf, vk.pipelines[pmui_pipe].layout,
                        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                        0, sizeof push, &push);
        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(cmdbuf, 0, 1, &alloc[vk.currentFrame].buf, &offset);
        vkCmdDraw(cmdbuf, (int)(p - buf) / 6, 1, 0, 0);
}

// the editor draws its own cursor, a little pixel-art glyph that names the
// tool in hand - the OS cursor is hidden while editing. Each glyph is a
// NULL-terminated list of equal-length rows; the legend is 'K' black
// outline, 'W' white, 'C' the current paint color (so the brush and bucket
// carry the color they'll lay), ' ' transparent. The hotspot (the pixel
// that sits exactly on the pointer) is given per glyph.
static const char *pmg_pointer[] = { // cursor_default.png, tip top-left (0,0)
        "KK             ",
        "KWK            ",
        "KWWK           ",
        "KWWWK          ",
        "KWWWWK         ",
        "KWWWWWK        ",
        "KWWWWWWK       ",
        "KWWWWWWWK      ",
        "KWWWWWWWWK     ",
        "KWWWWWWWWWK    ",
        "KWWWWWWWWWWK   ",
        "KWWWWWWWWWWWK  ",
        "KWWWWWWWWWWWWK ",
        "KWWWWWWWWWWWWWK",
        "KWWWWWWKKKKKKK ",
        "KWWWWWWWK      ",
        "KWWWWKWWK      ",
        "KWWWK KWWK     ",
        "KWWK  KWWK     ",
        "KWK    KWWK    ",
        " K     KWWK    ",
        "        KWWK   ",
        "        KWWK   ",
        "         KK    ",
        0 };
static const char *pmg_parent[] = { // cursor_parenter.png, tip top-left (0,0)
        "KK               ",
        "KWK              ",
        "KWWK             ",
        "KWWWK            ",
        "KWWWWK           ",
        "KWWWWWK          ",
        "KWWWWWWK         ",
        "KWWWWWWWK        ",
        "KWWWWWWWWK       ",
        "KWWWWWWWWWK      ",
        "KWWWWWWWWWWK     ",
        "KWWWWWWWWWWWK    ",
        "KWWWWWWKKKKKK    ",
        "KWWWWKK          ",
        "KWWWKKKKKKKKK    ",
        "KWWK KCCCCCCCKK  ",
        "KWK  KCCCCCCCCCK ",
        "KWK  KCCCCCCCCCK ",
        "KK   KCCCKKKCCCCK",
        "     KCCCK  KCCCK",
        "     KCCCK  KCCCK",
        "     KCCCK  KCCCK",
        "     KCCCKKKCCCCK",
        "     KCCCCCCCCCK ",
        "     KCCCCCCCCCK ",
        "     KCCCCCCCKK  ",
        "     KCCCKKKK    ",
        "     KCCCK       ",
        "     KCCCK       ",
        "     KCCCK       ",
        "     KCCCK       ",
        "     KCCCK       ",
        "     KKKKK       ",
        0 };
// the four detailed tools are traced from blocko-game/assets/cursors/*.png
// (black/white/red/transparent -> K/W/C/space). Brush and dropper have their
// business end at the top-left tip (hotspot 0,0); the buckets pour from the
// bottom-left (hotspots set where the glyph is chosen below).
static const char *pmg_brush[] = { // cursor_paintbrush.png, tip top-left
        "KK                           ",
        "KCK                          ",
        "KCK                          ",
        "KCCKKKKKK                    ",
        "KCCCCCCCCK                   ",
        " KCCCCCCCCK                  ",
        "  KCCCCCCCWK                 ",
        "  KCCCCCCCWK                 ",
        " KCCCCCCCWWK                 ",
        " KCCCCCWWWWK                 ",
        " KCCCWWWWWKWK                ",
        "  KWWWWWWKWWK                ",
        "   KKWWWKWWKK                ",
        "     KKKWWKWWK               ",
        "        KKWWWWK              ",
        "          KWWWWK             ",
        "           KWWWWK            ",
        "            KWWWWK           ",
        "             KWWWWK          ",
        "              KWWWWK         ",
        "               KWWWWK        ",
        "                KWWWWK       ",
        "                 KWWWWK      ",
        "                  KWWWWK     ",
        "                   KWWWWK    ",
        "                    KWWWWK   ",
        "                     KWWWWK  ",
        "                      KWWWWK ",
        "                       KWWWWK",
        "                        KWWWK",
        "                         KWWK",
        "                          KK ",
        0 };
static const char *pmg_dropper[] = { // cursor_dropper.png, tip top-left
        "KK                           ",
        "KCK                          ",
        " KCKK                        ",
        "  KCCK                       ",
        "  KCCCK                      ",
        "   KCCCK                     ",
        "    KCCCK                    ",
        "     KCCCK                   ",
        "      KCCCK                  ",
        "       KCCCK                 ",
        "        KCWWK                ",
        "         KWWWK               ",
        "          KWWWK              ",
        "           KWWWK             ",
        "            KWWWK            ",
        "             KWWWK           ",
        "              KWWWK          ",
        "               KWWWK KK      ",
        "                KWWKKKK      ",
        "                 KKKKK       ",
        "                  KKKKKK     ",
        "                 KKKKKKKKKK  ",
        "                 KK KKKKKKKK ",
        "                    KKKKKKKKK",
        "                     KKKKKKKK",
        "                     KKKKKKKK",
        "                     KKKKKKKK",
        "                      KKKKKK ",
        "                       KKKK  ",
        0 };
static const char *pmg_bucket[] = { // cursor_bucket.png, pours bottom-left
        "           K               ",
        "          KWK              ",
        "          KWK              ",
        "          KWK              ",
        "        KKKWK              ",
        "       KWWKWK              ",
        "      KWKKKWKKK            ",
        "     KWKKKKWKWWKK          ",
        "     KWKKKKWKWWWCKK        ",
        "    KWKKKKKWKWWWCCCKK      ",
        "    KWKKKKKWKWWWCCCCCKK    ",
        "   KWKKKKKKWKWWWCCCCCWWKK  ",
        "   KWKKKKKKWKWWWCCCCCWWWWK ",
        "   KWKKKKKKWWKWCCCCCCWWWWWK",
        "  KKKKKKKWKWWKWCCCCCWWWWWWK",
        "  KCCCCCKWWKKWWCCCCCWWWWWWK",
        " KCCCCCKWWWWWWCCCCCCWWWWWWK",
        " KCCCCKWWWWWWCCCCCCCWWWWWWK",
        "KCCCCKWWWWWWWCCCCCCWWWWWWK ",
        "KCCCKWWWWWWWCCCCCCCWWWWWWK ",
        "KCCCKKWWWWWCCCCCCCWWWWWWWK ",
        "KCCK  KKWWCCCCCCCWWWWWWWK  ",
        "KCCK    KKCCCCCCWWWWWWWWK  ",
        "KCCK      KKCCCWWWWWWWWK   ",
        "KCCK        KKWWWWWWWWK    ",
        "KCK           KKWWWWWK     ",
        "KCK             KKWWK      ",
        "KCK               KK       ",
        " K                         ",
        0 };
static const char *pmg_super[] = { // cursor_superbucket.png, pours bottom-left
        "                   K      KK       ",
        "                  KWK   KKWWK      ",
        "                  KWK KKWWWWWK     ",
        "                  KWKKWWWWWWWWK    ",
        "                  KWKWWWWKKWWWWK   ",
        "                KKKWKWWWKKKKWWWWK  ",
        "              KKWWKWKWWWKKKKWWWWK  ",
        "            KKWWWWKWKWWWKKKKWWWWWK ",
        "           KKWWWWWKWKWWWKKKKWWWWWK ",
        "          KKCKWWWWKWKWWWKKKKWWWWWK ",
        "       KKKKCCCKWWWKWKWWWKKKKWWWWWWK",
        "      KCCCCCCCCKWWKWKWWWWKKWWWWWWWK",
        "     KCCCCCCCCCCKWKWKWWWWKKWWWWWWWK",
        "     KCCCKCCKKKKKKKWWKWWWWWWWWWWWWK",
        "     KCCK KKWKCCCKKWWKWWWKKWWWWWWWK",
        "      KK   KWKCCCKKKKWWWWKKWWWWWWK ",
        "           KWKCCCCKWWWWWWWWWWWWKK  ",
        "            KCCCCCKWWWWWWWWWWKK    ",
        "  KKK      KKCCCCCCKWWWWWWWKK      ",
        " KCCCK    KCCCCCCCCKWWWWWKK        ",
        " KCCK   KKCCCCCCCCCKWWWKK          ",
        "  KK   KCCCCCCCCKKKKWKK            ",
        "      KCCCCCCCCKWWWKK              ",
        "      KCCCCCCCK KKK                ",
        "     KCCCKCCCCK                    ",
        "     KCCKCCCCCK                    ",
        "     KCCKCCCCCK                    ",
        "    KCCCCCCCCK                     ",
        "    KCCCCCCCCK                     ",
        "   KCCCCCCCCCK                     ",
        "   KCCCCCCCCCK                     ",
        "  KKCCCCCCCCCK                     ",
        "  KCCCCCCCCCCK                     ",
        "  KCCCCCCCCCCK                     ",
        "  KCCCCCCCCCCK                     ",
        "  KCCCCCCKCCCCK                    ",
        " KKCCCCCCKKCCCK                    ",
        "KKCCCCCCCK KKK                     ",
        "KCCCCKKCCK                         ",
        "KCCCK  KK                          ",
        " KKK                               ",
        0 };
static const char *pmg_resize[] = { // cursor_resizer.png, center hotspot
        "                   KK            ",
        "                  KWK            ",
        "                  KWK            ",
        "                 KWWK            ",
        "                 KWWK            ",
        "                KWWWK            ",
        "                KWWWK            ",
        "               KWWWWK            ",
        "               KWWWWK            ",
        "              KWWWWWK            ",
        "              KWWWWWK            ",
        "      K      KWWWWWWK     K      ",
        "     KWK     KWWWWWWK    KWK     ",
        "    KWWK    KWWWWWWWK    KWWK    ",
        "   KWWWK    KWWWWWWWK    KWWWK   ",
        "  KWWWWK    KWWWWWWWK    KWWWWK  ",
        " KWWWWWKKKKKKWWWWKKKKKKKKKWWWWWK ",
        "KWWWWWWWWWWWKWWWWWWWWWWWWWWWWWWWK",
        " KWWWWWKKKKKKWWWWKKKKKKKKKWWWWWK ",
        "  KWWWWK    KWWWWWWWK    KWWWWK  ",
        "   KWWWK    KWWWWWWWK    KWWWK   ",
        "    KWWK    KWWWWWWWK    KWWK    ",
        "     KWK    KWWWWWWK     KWK     ",
        "      K     KWWWWWWK      K      ",
        "            KWWWWWK              ",
        "            KWWWWWK              ",
        "            KWWWWK               ",
        "            KWWWWK               ",
        "            KWWWK                ",
        "            KWWWK                ",
        "            KWWK                 ",
        "            KWWK                 ",
        "            KWK                  ",
        "            KWK                  ",
        "            KK                   ",
        0 };
static const char *pmg_mover[] = { // cursor_mover.png, center hotspot
        "                K                ",
        "               KWK               ",
        "              KWWWK              ",
        "             KWWWWWK             ",
        "            KWWWWWWWK            ",
        "           KWWWWWWWWWK           ",
        "          KWWWWWWWWWWWK          ",
        "           KKKKKWKKKKK           ",
        "               KWK               ",
        "               KWK               ",
        "      K        KWK        K      ",
        "     KWK       KWK       KWK     ",
        "    KWWK        K        KWWK    ",
        "   KWWWK                 KWWWK   ",
        "  KWWWWK                 KWWWWK  ",
        " KWWWWWKKKKK    K    KKKKKWWWWWK ",
        "KWWWWWWWWWWWK  KWK  KWWWWWWWWWWWK",
        " KWWWWWKKKKK    K    KKKKKWWWWWK ",
        "  KWWWWK                 KWWWWK  ",
        "   KWWWK                 KWWWK   ",
        "    KWWK        K        KWWK    ",
        "     KWK       KWK       KWK     ",
        "      K        KWK        K      ",
        "               KWK               ",
        "               KWK               ",
        "           KKKKKWKKKKK           ",
        "          KWWWWWWWWWWWK          ",
        "           KWWWWWWWWWK           ",
        "            KWWWWWWWK            ",
        "             KWWWWWK             ",
        "              KWWWK              ",
        "               KWK               ",
        "                K                ",
        0 };

static const char *pmg_hide[] = { // cursor_eye.png, tip top-left (0,0)
        "KK                      ",
        "KWK                     ",
        "KWWK                    ",
        "KWWWK                   ",
        "KWWWWK                  ",
        "KWWWWWK                 ",
        "KWWWWWWK                ",
        "KWWWWWWWK               ",
        "KWWWWWWWWK              ",
        "KWWWWWWWWWK             ",
        "KWWWWWWWWWWK  K   K     ",
        "KWWWWWWWWWWWK K  K   K  ",
        "KWWWWWWKKKKKK K  K  K   ",
        "KWWWWKK             K   ",
        "KWWWK     KKKKKKK      K",
        "KWWK   KKKWWWWWWWKKK  K ",
        "KWK   KWWKWKKKWWWKWWK   ",
        "KWK  KWWKWKWKKKWWWKWWK  ",
        "KK  KWWWKWKKKKKWWWKWWWK ",
        "    KWWWKWKKKKKWWWKWWWK ",
        "     KWWKWWKKKWWWWKWWK  ",
        "      KWWKWWWWWWWKWWK   ",
        "       KKKWWWWWWWKKK    ",
        "          KKKKKKK       ",
        0 };
static const char *pmg_rotate[] = { // cursor_rotate.png, hotspot (9,9)
        "        K                     ",
        "       KWK                    ",
        "       KWK                    ",
        "       KWK                    ",
        "       KWK                    ",
        "       KWK                    ",
        "        K                     ",
        " KKKKK     KKKKK              ",
        "KWWWWWK K KWWWWWK      K      ",
        " KKKKK     KKKKK      KWK     ",
        "        K             KWWK    ",
        "       KWK            KWWWK   ",
        "       KWK        KKKKKWWWWK  ",
        "       KWK      KKWWWWWWWWWWK ",
        "       KWK     KWWWWWWWWWWWWWK",
        "       KWK    KWWWWWWWWWWWWWK ",
        "        K    KWWWWWWKKKWWWWK  ",
        "             KWWWWKK  KWWWK   ",
        "            KWWWWK    KWWK    ",
        "            KWWWWK    KWK     ",
        "            KWWWK      K      ",
        "            KWWWK             ",
        "         KKKKWWWKKKK          ",
        "        KWWWWWWWWWWWK         ",
        "         KWWWWWWWWWK          ",
        "          KWWWWWWWK           ",
        "           KWWWWWK            ",
        "            KWWWK             ",
        "             KWK              ",
        "              K               ",
        0 };

// append one glyph's quads at (mx,my) with hotspot (hx,hy). ccol is the
// packed color 'C' resolves to.
static float *pmedit_glyph(float *p, const char **rows, int hx, int hy,
                float mx, float my, float s, unsigned ccol)
{
        float black[4] = { 0, 0, 0, 1 }, white[4] = { 0.95f, 0.95f, 0.95f, 1 };
        float col[4] = { (ccol & 255) / 255.f, (ccol >> 8 & 255) / 255.f,
                         (ccol >> 16 & 255) / 255.f, 1 };
        for (int r = 0; rows[r]; r++)
                for (int c = 0; rows[r][c]; c++)
                {
                        const float *q;
                        switch (rows[r][c]) {
                        case 'K': q = black; break;
                        case 'W': q = white; break;
                        case 'C': q = col;   break;
                        default:  continue;
                        }
                        float x0 = mx + (c - hx) * s, y0 = my + (r - hy) * s;
                        p = pmedit_quad(p, x0, y0, x0 + s, y0 + s, q);
                }
        return p;
}

static void pmedit_cursor_ui()
{
        // which tool is in hand? over the button column or the palette panel
        // it's a plain pointer (you're clicking UI); on the model it's the
        // active paint tool, or the mode's own glyph. Every glyph is traced
        // art drawn at 2x, so it all stays about a cursor's height
        const char **g = pmg_pointer;
        int hx = 0, hy = 0;
        float gs = 2.f;
        int over_ui = pmedit_mx >= PMEDIT_BTN_X - 10
                || (pmedit_panel_on() && pmedit_in_panel(pmedit_mx, pmedit_my));
        if (over_ui)
                ; // pointer
        else if (pmedit_panel_on())
        {
                // a copy modifier held previews the eyedropper (RMB copies too)
                if (SDL_GetModState() & (SDL_KMOD_SHIFT | SDL_KMOD_CTRL
                                | SDL_KMOD_ALT | SDL_KMOD_GUI))
                        g = pmg_dropper; // tip top-left (0,0)
                else if (pmedit_flood == 2) { g = pmg_super;  hy = 39; } // pour tip
                else if (pmedit_flood == 1) { g = pmg_bucket; hy = 27; } // pour tip
                else                          g = pmg_brush;  // tip top-left (0,0)
        }
        // the positioning modes get their own glyphs: a four-way arrow to
        // resize (centered), a move cross to place a part or its attachment
        // point (centered), and the P cursor to pick a new parent (tip 0,0)
        else if (pmedit_resize)          { g = pmg_resize; hx = 16; hy = 17; }
        else if (pmedit_joint || pmedit_socket) { g = pmg_mover; hx = 16; hy = 16; }
        else if (pmedit_parent)            g = pmg_parent; // tip top-left (0,0)
        else if (pmedit_restang)         { g = pmg_rotate; hx = 9; hy = 9; }
        else if (pmedit_hide)              g = pmg_hide;   // tip top-left (0,0)

        unsigned ccol = pmedit_color ? pm_models[my_player].palette[pmedit_color]
                                     : PM_RGB(150, 150, 150); // eraser: gray
        if (g == pmg_parent) // its P reads pink, like the parent button/rims
                ccol = PM_RGB(255, 70, 160);
        static float buf[1500 * 6 * 6]; // the superbucket's worst case
        float *p = pmedit_glyph(buf, g, hx, hy, pmedit_mx, pmedit_my, gs, ccol);

        static struct allocation alloc[MAX_FRAMES_IN_FLIGHT];
        if (!alloc[vk.currentFrame].buf)
                vulkan_allocate_vertex_buffer(sizeof buf, &alloc[vk.currentFrame]);
        vulkan_populate_vertex_buffer(buf, (p - buf) * sizeof *buf,
                        &alloc[vk.currentFrame]);

        VkCommandBuffer cmdbuf = vk.commandBuffers[vk.imageIndex];
        vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS,
                        vk.pipelines[pmui_pipe].pipeline);
        VkViewport viewport = { 0, 0, screenw, screenh, 0, 1 };
        VkRect2D scissor = { {0, 0}, {screenw, screenh} };
        vkCmdSetViewport(cmdbuf, 0, 1, &viewport);
        vkCmdSetScissor(cmdbuf, 0, 1, &scissor);
        struct { float proj[16]; } push = {
                .proj = { 2.f / screenw, 0, 0, 0,
                          0, 2.f / screenh, 0, 0,
                          0, 0, 1, 0,
                         -1, -1, 0, 1 },
        };
        vkCmdPushConstants(cmdbuf, vk.pipelines[pmui_pipe].layout,
                        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                        0, sizeof push, &push);
        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(cmdbuf, 0, 1, &alloc[vk.currentFrame].buf, &offset);
        vkCmdDraw(cmdbuf, (int)(p - buf) / 6, 1, 0, 0);
}

// the picker overlay's 2D layer: title, a name under each thumbnail, the page
// controls, and the hint. The thumbnails and panels themselves draw earlier,
// in pmedit_pick_render (they need the 3D pipeline and per-cell depth).
static void pmedit_pick_ui(void)
{
        VkCommandBuffer cmdbuf = vk.commandBuffers[vk.imageIndex];

        char *title = "CHOOSE A MODEL";
        font_begin(screenw, screenh);
        font_add_text(title, screenw / 2.f - strlen(title) * 9.f, 64.f, 3);
        font_end(1, 1, 1);

        for (int c = 0; c < PM_NR_PREVIEW; c++)
        {
                int e = pmedit_pick_page * PM_NR_PREVIEW + c;
                if (e >= pmedit_pick_nr) continue;
                char *lbl = pmedit_pick_ent[e].label;
                float r[4]; pmedit_pick_cell_rect(c, r);
                float mid = (r[0] + r[2]) / 2.f;
                font_begin(screenw, screenh);
                font_add_text(lbl, mid - strlen(lbl) * 6.f, r[3] - 26.f, 2);
                font_end(0.85f, 0.9f, 0.95f);
        }

        if (pmedit_pick_pages() > 1)
        {
                static struct allocation pgbg[MAX_FRAMES_IN_FLIGHT];
                float pr[4], nr[4];
                pmedit_pick_prev_rect(pr);
                pmedit_pick_next_rect(nr);
                float btns[2][4];
                memcpy(btns[0], pr, sizeof pr);
                memcpy(btns[1], nr, sizeof nr);
                pmedit_fill_rects(cmdbuf, pgbg, btns, 2, (float[3]){ 0.13f, 0.13f, 0.16f });

                float d = pmedit_pick_page > 0 ? 0.85f : 0.3f;
                font_begin(screenw, screenh);
                font_add_text("< PREV", pr[0] + 22, pr[1] + 12, 3);
                font_end(d, d, d);
                d = pmedit_pick_page + 1 < pmedit_pick_pages() ? 0.85f : 0.3f;
                font_begin(screenw, screenh);
                font_add_text("NEXT >", nr[0] + 22, nr[1] + 12, 3);
                font_end(d, d, d);

                char pg[32];
                snprintf(pg, sizeof pg, "PAGE %d / %d",
                                pmedit_pick_page + 1, pmedit_pick_pages());
                font_begin(screenw, screenh);
                font_add_text(pg, screenw / 2.f - strlen(pg) * 6.f, screenh - 60.f, 2);
                font_end(0.7f, 0.7f, 0.7f);
        }

        char *hint = "click a model to switch to it   left/right page   ESC cancel";
        float scale = MIN(roundf(screenw / 600.f), roundf(screenh / 400.f));
        if (scale < 1) scale = 1;
        font_begin(screenw, screenh);
        font_add_text(hint, screenw / 2.f - strlen(hint) * 4.f * scale,
                        screenh - 26.f * scale, 0);
        font_end(1, 1, 1);

        pmedit_cursor_ui();
}

void pmedit_draw_ui()
{
        if (!pmedit_on) return;
        if (pmedit_picker) { pmedit_pick_ui(); return; }

        pmedit_boxes();

        // LOAD button, top-left in the plain whole-model view
        if (pmedit_sel < 0 && !pmedit_animate && !pmedit_hide)
        {
                static struct allocation cmbg[MAX_FRAMES_IN_FLIGHT];
                float r[1][4] = {{ 16, 16, 236, 60 }};
                pmedit_fill_rects(vk.commandBuffers[vk.imageIndex], cmbg, r, 1,
                                (float[3]){ 0.13f, 0.14f, 0.20f });
                font_begin(screenw, screenh);
                font_add_text("LOAD", 30.f, 30.f, 3);
                font_end(0.7f, 0.85f, 1.f);
        }
        if (pmedit_panel_on())
        {
                pmedit_palette_ui();

                font_begin(screenw, screenh);
                font_add_text("FLOOD FILL", PMEDIT_HSL_X0 + 10, PMEDIT_FF_Y0 + 10.f, 3);
                if (pmedit_flood == 1) font_end(1, 1, 0.25f);
                else font_end(0.55f, 0.7f, 0.55f);

                font_begin(screenw, screenh);
                font_add_text("SUPER FLOOD", PMEDIT_HSL_X0 + 10, PMEDIT_SF_Y0 + 10.f, 3);
                if (pmedit_flood == 2) font_end(1, 1, 0.25f);
                else font_end(0.55f, 0.7f, 0.55f);
        }

        // piece buttons always show; they grey down to "disabled" when no
        // piece is selected or ANIMATE is playing
        int enabled = pmedit_sel >= 0 && !pmedit_animate;
        float dim = enabled ? 0.55f : 0.28f;

        font_begin(screenw, screenh);
        font_add_text("PLACE ATTACHMENT POINT", PMEDIT_BTN_X, 34.f, 2);
        if (enabled && pmedit_joint) font_end(1, 1, 0.25f);
        else font_end(dim, dim, dim);

        font_begin(screenw, screenh);
        font_add_text("MOVE PART", PMEDIT_BTN_X, 92.f, 3);
        if (enabled && pmedit_socket) font_end(1, 1, 0.25f);
        else font_end(dim, dim, dim);

        font_begin(screenw, screenh);
        font_add_text(pmedit_sel >= 0
                        && pm_models[my_player].piece[pmedit_sel].parent >= 0
                        ? "DETACH" : "SELECT PARENT", PMEDIT_BTN_X, 156.f, 3);
        if (enabled && pmedit_parent) font_end(1, 0.55f, 0.8f);
        else if (enabled) font_end(1, 0.27f, 0.63f); // pink like the rims
        else font_end(dim, dim, dim);

        font_begin(screenw, screenh);
        font_add_text(pmedit_sel >= 0 ? (char *)pmedit_type_name[
                        pm_models[my_player].piece[pmedit_sel].type] : "TYPE",
                        PMEDIT_BTN_X, 220.f, 3);
        if (enabled) font_end(0.55f, 0.7f, 0.55f);
        else font_end(dim, dim, dim);

        font_begin(screenw, screenh);
        font_add_text("RESIZE", PMEDIT_BTN_X, 284.f, 3);
        if (enabled && pmedit_resize) font_end(1, 1, 0.25f);
        else font_end(dim, dim, dim);

        font_begin(screenw, screenh);
        font_add_text("RESTING ANGLE", PMEDIT_BTN_X, 348.f, 3);
        if (enabled && pmedit_restang) font_end(1, 1, 0.25f);
        else font_end(dim, dim, dim);

        int can_new = pm_models[my_player].nr_pieces < PM_MAX_PIECES
                        && !pmedit_animate;
        font_begin(screenw, screenh);
        font_add_text("MAKE COPY", PMEDIT_BTN_X, 412.f, 3);
        if (enabled && can_new) font_end(0.55f, 0.7f, 0.55f);
        else font_end(0.28f, 0.28f, 0.28f);

        font_begin(screenw, screenh);
        font_add_text("DELETE", PMEDIT_BTN_X, 476.f, 3);
        if (enabled) font_end(0.8f, 0.35f, 0.35f);
        else font_end(dim, dim, dim);

        font_begin(screenw, screenh);
        font_add_text("NEW PART", PMEDIT_BTN_X, 540.f, 3);
        if (can_new) font_end(0.55f, 0.7f, 0.55f);
        else font_end(0.28f, 0.28f, 0.28f);

        font_begin(screenw, screenh);
        font_add_text("ANIMATE", PMEDIT_BTN_X, 604.f, 3);
        if (pmedit_animate) font_end(1, 1, 0.25f);
        else font_end(0.55f, 0.55f, 0.55f);

        font_begin(screenw, screenh);
        font_add_text(pm_models[my_player].style == PM_STYLE_FLAIL ?
                        "FLAIL" : "WALK", PMEDIT_BTN_X, 668.f, 3);
        font_end(0.55f, 0.7f, 0.55f);

        // with pieces hidden and the mode off, the button turns into the
        // one-click bring-them-all-back
        int nhid = 0;
        for (unsigned m = pmedit_hidden; m; m >>= 1) nhid += m & 1;
        char hidebuf[24] = "HIDE";
        if (nhid && !pmedit_hide) sprintf(hidebuf, "UNHIDE (%d)", nhid);
        font_begin(screenw, screenh);
        font_add_text(hidebuf, PMEDIT_BTN_X, 732.f, 3);
        if (pmedit_hide) font_end(1, 1, 0.25f);
        else font_end(0.55f, 0.7f, 0.55f);

        // RESTING ANGLE reads its live numbers off the hint line
        static char restbuf[80];
        if (pmedit_restang && pmedit_sel >= 0)
                sprintf(restbuf, "PITCH %+d   YAW %+d   ROLL %+d"
                        "   arrows pitch/yaw   Q/E roll",
                        pm_models[my_player].piece[pmedit_sel].rest[0],
                        pm_models[my_player].piece[pmedit_sel].rest[1],
                        pm_models[my_player].piece[pmedit_sel].rest[2]);

        char *hint = pmedit_animate ?
                "WASD rotate   click anywhere to stop" :
                pmedit_hide ?
                "click a piece to hide it   click away when done   WASD rotate" :
                pmedit_sel < 0 ?
                "click a piece to paint it   WASD rotate   U done" :
                pmedit_joint ?
                "point to preview   LMB place the point   arrows nudge   WASD rotate" :
                pmedit_socket ?
                "point to preview   LMB place the part   arrows nudge   WASD rotate" :
                pmedit_parent ?
                (pmedit_newpart ?
                "LMB place the new piece on a parent   WASD rotate" :
                "LMB place the part on its new parent   WASD rotate") :
                pmedit_resize ?
                (pmedit_face ?
                "drag the face to resize   arrows push/pull 1px   LMB pick another face" :
                "LMB pick a face, then drag it to resize   WASD rotate") :
                pmedit_restang ? restbuf :
                pmedit_flood == 2 ?
                "LMB recoat the whole piece   RMB copy a color   click away to go back" :
                pmedit_flood ?
                "LMB fill the same-color region   RMB copy a color   click away to go back" :
                "LMB paint   RMB copy a color   ctrl-C copy piece   Del delete   click away to go back";
        float scale = MIN(roundf(screenw / 600.f), roundf(screenh / 400.f));
        if (scale < 1) scale = 1;
        font_begin(screenw, screenh);
        font_add_text(hint, screenw / 2.f - strlen(hint) * 4.f * scale,
                        screenh - 30.f * scale, 0);
        font_end(1, 1, 1);

        pmedit_cursor_ui(); // the tool cursor, on top of everything
}

#endif // BLOCKO_PMEDIT_C_INCLUDED

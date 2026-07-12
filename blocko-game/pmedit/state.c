#include "../blocko.c"
#ifndef BLOCKO_PMEDIT_STATE_C_INCLUDED
#define BLOCKO_PMEDIT_STATE_C_INCLUDED

// pmedit/state.c - the editor's shared state: selection, modes, camera easing, pick matrices

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

#endif // BLOCKO_PMEDIT_STATE_C_INCLUDED

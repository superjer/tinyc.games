#include "../blocko.c"
#ifndef BLOCKO_PMEDIT_LAYOUT_C_INCLUDED
#define BLOCKO_PMEDIT_LAYOUT_C_INCLUDED

// pmedit/layout.c - button layout + hit tests: one source of truth for what's clickable

// ---------------------------------------------------------------------------
// button layout: one source of truth
//
// The editor shows a different set of buttons depending on what's selected.
// pmedit_ui_state() names the four cases; pmedit_btn_rect() hands every
// consumer - the hit-tests, the grey backdrops (pmedit_boxes) and the labels
// (pmedit_draw_ui) - the SAME pixel rect for a button, so what's drawn is
// always exactly what's clickable. A button a state doesn't use reports
// "not present" and simply vanishes.
//
//   NONE   nothing selected, the whole model. Top-left X closes the editor; a
//          MODEL group (LOAD / NEW PART / ANIMATE) rides the top-right.
//   PIECE  a piece is selected, no tool running. Three right-hand groups -
//          PART TYPE (the type list + RESTING ANGLE), EDIT GEOMETRY (the four
//          modal tools) and THIS PART (MAKE COPY / HIDE / DELETE); the paint
//          palette sits on the left. Top-left BACK deselects.
//   MODAL  a per-piece tool is running (attach point, move, parent, resize,
//          resting angle, hide). Every panel clears out: only BACK and a
//          top-centre title + control line remain.
//   ANIM   the walk/flail preview plays: BACK stops it, a WALK/FLAIL toggle
//          sits under the title.
enum { PMEDIT_S_NONE, PMEDIT_S_PIECE, PMEDIT_S_MODAL, PMEDIT_S_ANIM };

static int pmedit_ui_state(void)
{
        if (pmedit_animate) return PMEDIT_S_ANIM;
        if (pmedit_joint || pmedit_socket || pmedit_parent
                        || pmedit_resize || pmedit_restang || pmedit_hide)
                return PMEDIT_S_MODAL;
        if (pmedit_sel >= 0) return PMEDIT_S_PIECE;
        return PMEDIT_S_NONE;
}

// the button column: labels left-align at PMEDIT_BTN_X, chips span the column
#define PMEDIT_BTN_X  (screenw - 330)
#define PMEDIT_COL_X0 (PMEDIT_BTN_X - 10)
#define PMEDIT_COL_X1 (screenw - 8)

// the PART TYPE list: PM_T_COUNT rows, one per type, inside the top group box
#define PMEDIT_TYPE_Y0 44
#define PMEDIT_TYPE_RH 20

enum {
        PB_BACK, PB_LOAD, PB_NEWPART, PB_ANIMATE,      // NONE + the model group
        PB_RESTANG,                                    // PART TYPE box
        PB_JOINT, PB_SOCKET, PB_PARENT, PB_RESIZE,     // EDIT GEOMETRY box
        PB_COPY, PB_HIDE, PB_DELETE,                   // THIS PART box
        PB_STYLE,                                       // ANIM
};

// fill r={x0,y0,x1,y1} with a button's pixel rect; return 1 if the button is
// present (and clickable) in the current UI state, else 0
static int pmedit_btn_rect(int b, float r[4])
{
        int st = pmedit_ui_state();
        float x0 = PMEDIT_COL_X0, x1 = PMEDIT_COL_X1;
        #define PMR(a,bb,c,d) do { r[0]=(a); r[1]=(bb); r[2]=(c); r[3]=(d); return 1; } while (0)
        switch (b)
        {
        case PB_BACK: // X when nothing's selected, BACK otherwise; always up
                PMR(16, 16, st == PMEDIT_S_NONE ? 60 : 148, 60);
        case PB_LOAD:    if (st != PMEDIT_S_NONE)  return 0; PMR(x0, 56,  x1, 100);
        case PB_NEWPART: if (st != PMEDIT_S_NONE)  return 0; PMR(x0, 108, x1, 152);
        case PB_ANIMATE: if (st != PMEDIT_S_NONE)  return 0; PMR(x0, 160, x1, 204);
        case PB_RESTANG: if (st != PMEDIT_S_PIECE) return 0; PMR(x0, 252, x1, 292);
        case PB_JOINT:   if (st != PMEDIT_S_PIECE) return 0; PMR(x0, 340, x1, 384);
        case PB_SOCKET:  if (st != PMEDIT_S_PIECE) return 0; PMR(x0, 390, x1, 434);
        case PB_PARENT:  if (st != PMEDIT_S_PIECE) return 0; PMR(x0, 440, x1, 484);
        case PB_RESIZE:  if (st != PMEDIT_S_PIECE) return 0; PMR(x0, 490, x1, 534);
        case PB_COPY:    if (st != PMEDIT_S_PIECE) return 0; PMR(x0, 580, x1, 624);
        case PB_HIDE:    if (st != PMEDIT_S_PIECE) return 0; PMR(x0, 630, x1, 674);
        case PB_DELETE:  if (st != PMEDIT_S_PIECE) return 0; PMR(x0, 680, x1, 724);
        case PB_STYLE:   if (st != PMEDIT_S_ANIM)  return 0;
                PMR(screenw / 2.f - 90, 96, screenw / 2.f + 90, 140);
        }
        #undef PMR
        return 0;
}

static int pmedit_hit(float x, float y, const float r[4])
{
        return x >= r[0] && x <= r[2] && y >= r[1] && y <= r[3];
}

static int pmedit_in_btn(int b, float x, float y)
{
        float r[4];
        return pmedit_btn_rect(b, r) && pmedit_hit(x, y, r);
}

// which PART TYPE list row is under the cursor (a type index), or -1
static int pmedit_type_row(float x, float y)
{
        if (pmedit_ui_state() != PMEDIT_S_PIECE) return -1;
        if (x < PMEDIT_COL_X0 || x > PMEDIT_COL_X1) return -1;
        if (y < PMEDIT_TYPE_Y0
                        || y >= PMEDIT_TYPE_Y0 + PM_T_COUNT * PMEDIT_TYPE_RH)
                return -1;
        return (int)((y - PMEDIT_TYPE_Y0) / PMEDIT_TYPE_RH);
}

// the palette panel, top-left, up whenever paint clicks are: 16 swatches in
// two rows of 8 (transparent's checker first, then the 15 colors - clicking
// one makes it the paint color), and the color picker below - the classic
// hue-lightness plane, white across the top, black across the bottom, the
// full-saturation rainbow through the middle. Clicking (or dragging) the
// picker recolors the SELECTED swatch's palette slot, and every texel
// wearing that slot follows live. Slot 0 stays transparent forever;
// painting with it erases (the shader discards texels under half alpha)
// the panel sits below the top-left BACK button (which owns y 16..60), so
// every row is pushed down to clear it
#define PMEDIT_SW_X0     16
#define PMEDIT_SW_Y0     72
#define PMEDIT_SW_SZ     36
#define PMEDIT_SW_STRIDE 40
#define PMEDIT_HSL_X0    16
#define PMEDIT_HSL_Y0    156
#define PMEDIT_HSL_X1    336
#define PMEDIT_HSL_Y1    396
#define PMEDIT_FF_Y0     404 // the FLOOD FILL button, full panel width
#define PMEDIT_FF_Y1     448
#define PMEDIT_SF_Y0     456 // SUPER FLOOD below it
#define PMEDIT_SF_Y1     500
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

#endif // BLOCKO_PMEDIT_LAYOUT_C_INCLUDED

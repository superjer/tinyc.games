#include "../blocko.c"
#ifndef BLOCKO_PMEDIT_PALETTE_C_INCLUDED
#define BLOCKO_PMEDIT_PALETTE_C_INCLUDED

// pmedit/palette.c - the color picker's math and the paint/flood brushes

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

#endif // BLOCKO_PMEDIT_PALETTE_C_INCLUDED

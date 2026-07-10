#include "blocko.c"
#ifndef BLOCKO_BLOCKLIGHT_C_INCLUDED
#define BLOCKO_BLOCKLIGHT_C_INCLUDED

// Sky-only block lighting: a block is lit iff it's above its column's ground
// line, so light is a pure function of gndheight - no stored light values,
// no spreading. Meshes read baked corner brightness (cornlight); a corner
// averages the 8 blocks meeting there, which darkens corners crowded by
// ground - free ambient occlusion.

// one corner's brightness from the ground lines of the four columns meeting
// there: each column contributes its two blocks at the corner (y-1 and y).
// 0.12 per lit block, 8 blocks -> full sky is 0.96
static inline float sun_corner(int y, int ga, int gb, int gc, int gd)
{
        #define LIT2(g) ((y < (g)) ? 2 : (y == (g)) ? 1 : 0)
        return 0.12f * (LIT2(ga) + LIT2(gb) + LIT2(gc) + LIT2(gd));
        #undef LIT2
}

// runs on the terrain thread - coords are in the terrain thread's window
// mapping, so only the T-variant macros are safe here (a scoot can land
// mid-generation, making scootx and tscootx briefly disagree)
void recalc_corner_lighting(int xlo, int xhi, int zlo, int zhi)
{
        for (int x = xlo; x < xhi; x++) for (int z = zlo; z < zhi; z++)
        {
                int x_ = (x == 0) ? 0 : x - 1;
                int z_ = (z == 0) ? 0 : z - 1;
                int ga = TGNDH_(x_, z_), gb = TGNDH_(x, z_),
                    gc = TGNDH_(x_, z ), gd = TGNDH_(x, z );

                // everything below the deepest ground line is dark: compute
                // down to just past it and zero the rest (the ring slot holds
                // the previous occupant's values)
                int ylim = MAX(MAX(ga, gb), MAX(gc, gd)) + 2;
                if (ylim > TILESH) ylim = TILESH;

                // the corner array stores columns contiguously in y
                float *corn = &TCORN_(x, 0, z);
                for (int y = 0; y < ylim; y++)
                        corn[y] = sun_corner(y, ga, gb, gc, gd);
                if (ylim < TILESH)
                        memset(corn + ylim, 0, (TILESH - ylim) * sizeof *corn);
        }
}

// main thread, after an edit moves column (x,z)'s ground line: recompute the
// four corner columns touching it and mark the touched chunks for remesh.
// The +1 corners past the window's high edge have no ring slot (the mask
// would wrap them onto the opposite edge), so stop short there
void recalc_corners_at(int x, int z)
{
        int xhi = MIN(x + 2, TILESW), zhi = MIN(z + 2, TILESD);
        DIRTY_LIGHT(B2C(x), B2C(z));
        DIRTY_LIGHT(B2C(xhi-1), B2C(z));
        DIRTY_LIGHT(B2C(x), B2C(zhi-1));
        DIRTY_LIGHT(B2C(xhi-1), B2C(zhi-1));

        for (int cx = x; cx < xhi; cx++) for (int cz = z; cz < zhi; cz++)
        {
                int x_ = (cx == 0) ? 0 : cx - 1;
                int z_ = (cz == 0) ? 0 : cz - 1;
                int ga = GNDH_(x_, z_), gb = GNDH_(cx, z_),
                    gc = GNDH_(x_, cz), gd = GNDH_(cx, cz);
                float *corn = &CORN_(cx, 0, cz);
                for (int y = 0; y < TILESH; y++)
                        corn[y] = sun_corner(y, ga, gb, gc, gd);
        }
}

#endif // BLOCKO_BLOCKLIGHT_C_INCLUDED

#include "blocko.c"
#ifndef BLOCKO_BLOCKMODEL_C_INCLUDED
#define BLOCKO_BLOCKMODEL_C_INCLUDED

// blockmodel.c - the shape of a block when drawn OFF the world grid: the held
// block (hand.c), a dropped item (item.c), and the mining stand-in (mine.c).
// One place decides how many faces a block has and which texture + shader orient
// each face uses, so a new block shape is defined once here instead of copied
// into all three renderers. The in-world mesh (mesh.c) keeps its own path - it
// also does neighbor culling and real per-corner lighting - but shares the same
// shader orient codes, so the geometry itself is still defined once (in the
// shaders' block_geom.glsl).

// the terrain texture-array layer for one face of a block, by face orient. Used
// by both block_model below and the in-world mining stand-in so a loose copy
// looks exactly like the block it came from.
int tile_face_tex(int t, int orient)
{
        switch (t)
        {
                case GRAS: return orient == UP ? 0 : orient == DOWN ? 2 : 1;
                case GSLP: return orient == UP ? 0 : orient == DOWN ? 2 : 1;
                case MTGR: return orient == UP ? 37 : orient == DOWN ? 2 : 38;
                case DIRT: return 2;
                case STON: return 5;
                case SAND: return 6;
                case WATR: return 7;
                case ORE:  return 11;
                case OREH: return 12;
                case HARD: return 13;
                case WOOD: return 14;
                case GRAN: return 15;
                case RLEF: return 16;
                case YLEF: return 17;
                case SLEF: return 39;
                case LITE: return 18;
                // BARR has no mesh.c case, so it stays invisible in the world grid,
                // but a stand-in / dropped item / hand copy is labelled so you can
                // tell what it is off the grid
                case BARR: return 43;
                default:   return 42; // debug: the labelled "open" tile, so a stray
                                      // OPEN (or any stray tile) is obvious instead
                                      // of masquerading as STON
        }
}

// most faces a block model can have (cube 6, wedge 5); size caller buffers here.
#define BLOCK_MODEL_MAX_FACES 6

// the eight cell corner lights, in the canonical order used by mesh.c and
// mine.c: upper (y-) sw,se,nw,ne, then lower (y+) sw,se,nw,ne. Index a corner as
// lev*4 + hor, lev 0 upper / 1 lower, hor 0 sw / 1 se / 2 nw / 3 ne.
//   {usw,use,unw,une, dsw,dse,dnw,dne}

// write block type t's face list into buf, lit per corner: sun[8]/glo[8] give
// the eight cell corners (canonical order above) and each face's four verts pull
// the corner they land on, so a loose copy matches the block's world lighting.
// `facing` rotates the slope wedge (SLOPE_S/W/N/E); cube blocks ignore it.
// Returns the face count - draw with vkCmdDraw(4, n, ...). This plus the shader
// orient codes (block_geom.glsl) is the single definition of a block's shape.
static int block_model_lit(struct vbufv *buf, int t, int facing,
                const float sun[8], const float glo[8])
{
        struct vbufv *b = buf;

        if (t == GSLP)
        {
                // grass slope wedge: sloped top, two diagonal-grass triangle
                // walls, a grass back wall, and a dirt bottom - the same faces
                // and corner-light mapping mesh.c emits in the world. orient
                // 30 + kind*4 + facing selects the piece (kinds: 0 top, 1 west
                // tri, 2 east tri, 3 back). Each face vert takes the cell corner
                // it lands on; the 90*facing rotation permutes the horizontal
                // corner sw->nw->ne->se (slope_rot) while leaving y (lev) alone.
                int f = facing & 3;
                static const int tex_for[4]  = { 0, 44, 44, 1 };
                static const int slope_rot[4] = { 2, 0, 3, 1 };
                static const int base_lev[4][4] = {
                        { 1, 1, 0, 0 }, { 1, 0, 1, 1 }, { 0, 1, 1, 1 }, { 0, 0, 1, 1 },
                };
                static const int base_hor[4][4] = {
                        { 0, 1, 2, 3 }, { 0, 2, 0, 2 }, { 3, 1, 3, 1 }, { 2, 3, 2, 3 },
                };
                for (int k = 0; k < 4; k++)
                {
                        float s[4], g[4];
                        for (int q = 0; q < 4; q++)
                        {
                                int h = base_hor[k][q];
                                for (int r = 0; r < f; r++) h = slope_rot[h];
                                int idx = base_lev[k][q] * 4 + h;
                                s[q] = sun[idx]; g[q] = glo[idx];
                        }
                        *b++ = (struct vbufv){ (float)tex_for[k], (float)(30 + k*4 + f),
                                0, 0, 0, s[0], s[1], s[2], s[3], g[0], g[1], g[2], g[3], 1 };
                }
                // dirt bottom: standard DOWN face (corners dse,dsw,dne,dnw)
                *b++ = (struct vbufv){ 2, DOWN, 0, 0, 0,
                        sun[5], sun[4], sun[7], sun[6], glo[5], glo[4], glo[7], glo[6], 1 };
        }
        else
        {
                // a solid cube: one textured face per side, each vert taking the
                // corner it lands on (same order as mesh.c's cube emission).
                static const int cube_orient[6] = { UP, SOUTH, NORTH, WEST, EAST, DOWN };
                static const int cube_corner[6][4] = {
                        { 0, 1, 2, 3 }, // UP:    usw use unw une
                        { 1, 0, 5, 4 }, // SOUTH: use usw dse dsw
                        { 2, 3, 6, 7 }, // NORTH: unw une dnw dne
                        { 0, 2, 4, 6 }, // WEST:  usw unw dsw dnw
                        { 3, 1, 7, 5 }, // EAST:  une use dne dse
                        { 5, 4, 7, 6 }, // DOWN:  dse dsw dne dnw
                };
                for (int k = 0; k < 6; k++)
                {
                        const int *c = cube_corner[k];
                        *b++ = (struct vbufv){ (float)tile_face_tex(t, cube_orient[k]),
                                (float)cube_orient[k], 0, 0, 0,
                                sun[c[0]], sun[c[1]], sun[c[2]], sun[c[3]],
                                glo[c[0]], glo[c[1]], glo[c[2]], glo[c[3]], 1 };
                }
        }

        return (int)(b - buf);
}

// flat-lit convenience: every corner gets the same illum il / glow gl. Used by
// the held block (hand.c) and dropped items (item.c), which read fine flat.
static int block_model(struct vbufv *buf, int t, int facing, float il, float gl)
{
        float sun[8], glo[8];
        for (int i = 0; i < 8; i++) { sun[i] = il; glo[i] = gl; }
        return block_model_lit(buf, t, facing, sun, glo);
}

#endif // BLOCKO_BLOCKMODEL_C_INCLUDED

#include "blocko.c"
#ifndef BLOCKO_COLLISION_C_INCLUDED
#define BLOCKO_COLLISION_C_INCLUDED

int legit_tile(int x, int y, int z)
{
        return x >= 0 && x < TILESW
            && y >= 0 && y < TILESH
            && z >= 0 && z < TILESD;
}

//collide a rect with a rect
int collide(struct box l, struct box r)
{
        int xcollide = l.x + l.w >= r.x && l.x < r.x + r.w;
        int ycollide = l.y + l.h >= r.y && l.y < r.y + r.h;
        int zcollide = l.z + l.d >= r.z && l.z < r.z + r.d;
        return xcollide && ycollide && zcollide;
}

// how many solid steps a slope is modelled as for collision. Each step is
// BS/SLOPE_STEPS tall (100 units at BS=1000) - well under the auto-step height,
// so walking up reads as a smooth ramp while every step is a real wall.
#define SLOPE_STEPS 10

//collide a rect with a block
int block_collide(int bx, int by, int bz, struct box box, int wet)
{
        // no window clamp: sim_tile answers at any coord (solid barrier
        // outside all sim coverage, open air past the top/bottom)
        int t = sim_tile(bx, by, bz);

        if (wet)
                return t == WATR
                        ? collide(box, (struct box){BS*bx, BS*by, BS*bz, BS, BS, BS})
                        : 0;

        // a slope collides as a staircase of solid steps rising toward its high
        // side: the sloped top walks like a smooth ramp (each step clears the
        // auto-step in player.c) while the sides and tall back stay solid, so you
        // no longer slip through the wedge. Facing (tileo) picks the climb axis.
        // Only the window has orient; a slope in a far sim area (no tileo) falls
        // back to a full cube, matching "mobs treat slopes as cubes".
        if (t == GSLP)
        {
                if (!legit_tile(bx, by, bz))
                        return collide(box, (struct box){BS*bx, BS*by, BS*bz, BS, BS, BS});

                float ox = BS*bx, oy = BS*by, oz = BS*bz;
                float sw = (float)BS / SLOPE_STEPS;
                int f = TO_(bx, by, bz) & 3;
                for (int i = 0; i < SLOPE_STEPS; i++)
                {
                        float h = (i + 1) * sw;   // low side thin, high side full cell
                        struct box s = { ox, oy + BS - h, oz, BS, h, BS };
                        switch (f) {
                        case SLOPE_S: s.z = oz + i*sw;          s.d = sw; break; // high north
                        case SLOPE_N: s.z = oz + BS - (i+1)*sw; s.d = sw; break; // high south
                        case SLOPE_W: s.x = ox + i*sw;          s.w = sw; break; // high east
                        case SLOPE_E: s.x = ox + BS - (i+1)*sw; s.w = sw; break; // high west
                        }
                        if (collide(box, s))
                                return 1;
                }
                return 0;
        }

        if (t <= LASTSOLID)
                return collide(box, (struct box){BS*bx, BS*by, BS*bz, BS, BS, BS});

        return 0;
}

//collide a box with nearby world tiles
int world_collide(struct box box, int wet)
{
        for (int i = -1; i < 2; i++) for (int j = -1; j < 3; j++) for (int k = -1; k < 2; k++)
        {
                // floorf: mobs in sim areas can sit at negative window
                // coords, where int truncation would round the wrong way
                int bx = (int)floorf(box.x/BS) + i;
                int by = (int)floorf(box.y/BS) + j;
                int bz = (int)floorf(box.z/BS) + k;

                if (block_collide(bx, by, bz, box, wet))
                        return 1;
        }

        return 0;
}

#endif // BLOCKO_COLLISION_C_INCLUDED

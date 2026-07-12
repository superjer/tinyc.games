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

// fill `out` with the SLOPE_STEPS solid step boxes for the slope cell at
// (bx,by,bz), rising toward its high side (facing = tileo bits 0-1). Shared by
// block_collide and the mining raycast (rayshot) so walking on a slope and
// clicking it agree on exactly the same shape.
void slope_boxes(int bx, int by, int bz, int facing, struct box out[SLOPE_STEPS])
{
        float ox = BS*bx, oy = BS*by, oz = BS*bz;
        float sw = (float)BS / SLOPE_STEPS;
        int f = facing & 3;
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
                out[i] = s;
        }
}

// does the ray from `e` along direction `d` hit box `b` going forward (t >= 0)?
// slab method; axes with a near-zero direction component only hit if the origin
// already lies within that slab.
int ray_hits_box(float ex, float ey, float ez, float dx, float dy, float dz, struct box b)
{
        float tmin = 0.f, tmax = 1e30f;
        float o[3] = { ex, ey, ez }, dd[3] = { dx, dy, dz };
        float lo[3] = { b.x, b.y, b.z }, hi[3] = { b.x+b.w, b.y+b.h, b.z+b.d };
        for (int k = 0; k < 3; k++)
        {
                if (dd[k] > -1e-6f && dd[k] < 1e-6f)
                {
                        if (o[k] < lo[k] || o[k] > hi[k]) return 0; // parallel & outside
                        continue;
                }
                float t1 = (lo[k] - o[k]) / dd[k];
                float t2 = (hi[k] - o[k]) / dd[k];
                if (t1 > t2) { float tmp = t1; t1 = t2; t2 = tmp; }
                if (t1 > tmin) tmin = t1;
                if (t2 < tmax) tmax = t2;
                if (tmin > tmax) return 0;
        }
        return tmax >= 0.f;
}

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
        // no longer slip through the wedge. sim_tileo picks the climb axis from
        // whichever copy answered (main window or a sim area), so mobs far from
        // the host walk the same staircase their owner does.
        if (t == GSLP)
        {
                struct box steps[SLOPE_STEPS];
                slope_boxes(bx, by, bz, sim_tileo(bx, by, bz), steps);
                for (int i = 0; i < SLOPE_STEPS; i++)
                        if (collide(box, steps[i]))
                                return 1;
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

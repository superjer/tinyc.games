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

//collide a rect with a block
int block_collide(int bx, int by, int bz, struct box box, int wet)
{
        // no window clamp: sim_tile answers at any coord (solid barrier
        // outside all sim coverage, open air past the top/bottom)
        int t = sim_tile(bx, by, bz);

        if (wet && t == WATR)
                return collide(box, (struct box){BS*bx, BS*by, BS*bz, BS, BS, BS});

        if (!wet && t <= LASTSOLID)
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

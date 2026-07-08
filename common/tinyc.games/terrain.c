#ifndef TINYCGAMES_TERRAIN_C_INCLUDED
#define TINYCGAMES_TERRAIN_C_INCLUDED

#include <math.h>
#include "taylor_noise.c"
#include "utils.c"

_Thread_local int get_height_hit, get_height_miss;
int world_seed = 160659;

float zigzag(float val, int zags)
{
        float bigval = val * zags;
        int zag = floorf(bigval);
        float floor = (float)zag / zags;
        if (zag % 2)
                return remap(val, floor, floor + 1.f / zags, 1.f, 0.f);
        else
                return remap(val, floor, floor + 1.f / zags, 0.f, 1.f);
}

float get_height(int x, int y)
{
        struct hmemo {
                int x, y, seed;
                float val;
        };
        static _Thread_local struct hmemo hmemos[37][1217];
        struct hmemo *m = &hmemos[(x + 0x01000000) % 37][(y + 0x01000000) % 1217];
        if (m->x == x && m->y == y && m->seed == world_seed && m->val)
        {
                get_height_hit++;
                return m->val;
        }
        get_height_miss++;

        // legend
        //if (x < 20 && y < 640) return (y - 240) / 20 / 20.f;

        float val = .05f 
                + noise(x, y, 1300, world_seed, 2) * .6f
                + noise(x, y,  800, world_seed, 1) * .30f
                + noise(x, y,  400, world_seed, 1) * .15f
                + noise(x, y,  200, world_seed, 1) * .08f
                + noise(x, y,  100, world_seed, 1) * .04f;
        
        // basic slope
        //val = x * .0007f + .3f;
        
        //val += (zigzag(val, 100) - .5f) * .02f;

        float plateauness = remap(noise(x, y, 1200, world_seed^34899346, 1), .50f, .55f, 0.f, 1.f);
        if (plateauness > 0.f)
        {
                float T1 = remap(noise(x, y, 700, world_seed, 8), 0.f, 1.f, .47f, .51f);
                float T2 = T1 + remap(noise(x, y, 212, world_seed, 2), 0.f, 1.f, -.02f, .12f);
                float T3 = T2 + remap(noise(x, y, 274, world_seed, 2), 0.f, 1.f, -.02f, .12f);
                float shelf_val = val;
                if (shelf_val <= .48f)
                        shelf_val = excl_remap(shelf_val, .46f, .48f, .46f       , T1         );
                else if (shelf_val <= .54f)
                        shelf_val = excl_remap(shelf_val, .48f, .54f, T1         , T1 + .0005f);
                else if (shelf_val <= .56f)
                        shelf_val = excl_remap(shelf_val, .54f, .56f, T1 + .0005f, T2         );
                else if (shelf_val <= .62f)
                        shelf_val = excl_remap(shelf_val, .56f, .62f, T2         , T2 + .0005f);
                else if (shelf_val <= .64f)
                        shelf_val = excl_remap(shelf_val, .62f, .64f, T2 + .0005f, T3         );
                else if (shelf_val <= .70f)
                        shelf_val = excl_remap(shelf_val, .64f, .70f, T3         , T3 + .0005f);
                else
                        shelf_val = excl_remap(shelf_val, .70f,  1.f, T3 + .0005f, 1.f        );
                val = lerp(plateauness, val, shelf_val);
        }

        // big oceans: a very low frequency mask presses lowlands below sea
        // level (0.5); the mountain pass in get_filtered_height runs after
        // this and can still raise islands out of the water
        float oceaniness = remap(noise(x, y, 9000, world_seed^41741741, 2), .53f, .61f, 0.f, 1.f);
        if (oceaniness > 0.f)
        {
                float ocean_floor = .38f + .10f * val; // keep a little relief
                val = lerp(oceaniness, val, MIN(val, ocean_floor));
        }

        m->x = x;
        m->y = y;
        m->seed = world_seed;
        m->val = val;
        return val;
}

float get_filtered_height(int x, int y)
{
        int x2 = x;
        int y2 = y;

        // wacky spiral rotation
        {
                int originx = 300;
                int originy = 150;
                int tx = x2 - originx;
                int ty = y2 - originy;
                float dist = sqrtf(tx * tx + ty * ty) / 600.f;
                if (dist < 1.f)
                {
                        float ang =  .7f * (1.f - dist) * (1.f - dist) * (1.f - dist)
                                * remap(noise(x, y, 1080, world_seed, 3), .5f, 1.f, 0.f, 10.f);
                        x2 = originx + tx * cosf(ang) - ty * sinf(ang);
                        y2 = originy + tx * sinf(ang) + ty * cosf(ang);
                }
        }

        // wacky spiral rotation reverse!
        {
                int originx = 350;
                int originy = 100;
                int tx = x2 - originx;
                int ty = y2 - originy;
                float dist = sqrtf(tx * tx + ty * ty) / 1000.f;
                if (dist < 1.f)
                {
                        float ang = -.7f * (1.f - dist) * (1.f - dist) * (1.f - dist)
                                * remap(noise(x, y, 1120, world_seed, 3), .5f, 1.f, 0.f, 10.f);
                        x2 = originx + tx * cosf(ang) - ty * sinf(ang);
                        y2 = originy + tx * sinf(ang) + ty * cosf(ang);
                }
        }

        // bubble
        {
                int originx = 700;
                int originy = 700;
                int tx = x2 - originx;
                int ty = y2 - originy;
                float dist = sqrtf(tx * tx + ty * ty) / 500.f;
                if (dist < 1.f)
                {
                        if (dist < .5f) dist = 1.f - dist;
                        float nx = tx / dist;
                        float ny = ty / dist;
                        float m = (1.f - dist) * (1.f - dist) * 5.f;
                        x2 += nx * m;
                        y2 += ny * m;
                }
        }

        float h = get_height(x2, y2);

        float mountains = noise(x, y, 1200, world_seed^46447731, 2);
        if (mountains > .7f)
        {
                mountains -= .7f;
                mountains *= 3.f;

                float calds = noise(x, y, 140, world_seed^96264448, 2);
                if (calds > .59f)
                {
                        mountains -= (calds - .59f);
                }
                calds = 1.f - calds;
                if (calds > .59f)
                {
                        mountains -= (calds - .59f);
                }

                float stacks = noise(x, y, 205, world_seed^77000325, 1);
                if (stacks > .61f)
                {
                        stacks -= .61f;
                        mountains += stacks;
                }

                if (mountains > 0.f)
                        h += mountains;
        }

        float mounds = noise(x, y, 290, world_seed^98453517, 1);
        if (mounds > .65f)
        {
                mounds -= .65f;
                h += mounds;
        }

        float lumps = noise(x, y, 175, world_seed^36447731, 1);
        if (lumps > .65f)
        {
                lumps -= .65f;
                h += lumps;
        }

        float pits = noise(x, y, 430, world_seed^77488339, 2);
        if (pits > .65f)
        {
                pits -= .65f;
                h -= pits;
        }

        float smoothouts = noise(x, y, 990, world_seed^13546936, 1);
        if (smoothouts > .65f)
        {
                float s = remap(smoothouts, .65f, 1.f, 0.f, 1000.f);
                h = get_height(x2 + s, y2 + s)
                  + get_height(x2 - s, y2 + s)
                  + get_height(x2 + s, y2 - s)
                  + get_height(x2 - s, y2 - s);
                h /= 4.f;
        }

        // mountain ranges: a broad mask picks range regions, folded ("ridged")
        // noise carves crests and valleys inside them; amplitude rides the
        // crests so ranges crossing ocean surface as island chains
        float mrange = remap(noise(x, y, 4000, world_seed^11223344, 2), .60f, .72f, 0.f, 1.f);
        if (mrange > 0.f)
        {
                float r1 = noise(x, y, 700, world_seed^22334455, 2);
                float ridge = 1.f - 2.f * fabsf(r1 - .5f); // 1 at ridgelines
                ridge *= ridge;
                float r2 = noise(x, y, 250, world_seed^33445566, 2);
                float ridge2 = 1.f - 2.f * fabsf(r2 - .5f);
                h += mrange * (.05f + .55f * ridge + .18f * ridge2 * ridge2);
        }

        // soft ceiling: round peaks off rather than pancake at the world top
        // (the game maps h=0.5 to sea level and gives ~1.0 of headroom above)
        if (h > 1.25f) h = 1.25f + (h - 1.25f) * .5f;
        if (h > 1.50f) h = 1.50f;

        return h;
}

#endif // TINYCGAMES_TERRAIN_C_INCLUDED

#ifndef TINYCGAMES_TERRAIN_C_INCLUDED
#define TINYCGAMES_TERRAIN_C_INCLUDED

#include "taylor_noise.c"
#include "utils.c"

int get_height_hit, get_height_miss;
int seed = 160659;

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
                int x, y;
                float val;
        };
        static struct hmemo hmemos[37][1217];
        struct hmemo *m = &hmemos[(x + 0x01000000) % 37][(y + 0x01000000) % 1217];
        if (m->x == x && m->y == y && m->val)
        {
                get_height_hit++;
                return m->val;
        }
        get_height_miss++;

        // legend
        //if (x < 20 && y < 640) return (y - 240) / 20 / 20.f;

        float val = .05f 
                + noise(x, y, 1300, seed, 2) * .6f
                + noise(x, y,  800, seed, 1) * .30f
                + noise(x, y,  400, seed, 1) * .15f
                + noise(x, y,  200, seed, 1) * .08f
                + noise(x, y,  100, seed, 1) * .04f;
        
        // basic slope
        //val = x * .0007f + .3f;
        
        //val += (zigzag(val, 100) - .5f) * .02f;

        /*
        float oceaniness = remap(noise(x, y, 2030, seed^41741741, 1), .55f, .7f, 0.f, 1.f);
        if (oceaniness > 0.f && val < 0.51f)
        {
                float ocean_val = .2f * val;
                float ocean_val2 = val * (1.f - oceaniness) + ocean_val * oceaniness;
                float ocean_blend = remap(val, 0.46f, 0.49f, 0.f, 1.f);
                val = val * (1.f - ocean_blend) + ocean_val2 * ocean_blend;
        }
        */

        float plateauness = remap(noise(x, y, 1200, seed^34899346, 1), .50f, .55f, 0.f, 1.f);
        if (plateauness > 0.f)
        {
                float T1 = remap(noise(x, y, 700, seed, 8), 0.f, 1.f, .47f, .51f);
                float T2 = T1 + remap(noise(x, y, 212, seed, 2), 0.f, 1.f, -.02f, .12f);
                float T3 = T2 + remap(noise(x, y, 274, seed, 2), 0.f, 1.f, -.02f, .12f);
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

        m->x = x;
        m->y = y;
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
                                * remap(noise(x, y, 1080, seed, 3), .5f, 1.f, 0.f, 10.f);
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
                                * remap(noise(x, y, 1120, seed, 3), .5f, 1.f, 0.f, 10.f);
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

        float mountains = noise(x, y, 1200, seed^46447731, 2);
        if (mountains > .7f)
        {
                mountains -= .7f;
                mountains *= 3.f;

                float calds = noise(x, y, 140, seed^96264448, 2);
                if (calds > .59f)
                {
                        mountains -= (calds - .59f);
                }
                calds = 1.f - calds;
                if (calds > .59f)
                {
                        mountains -= (calds - .59f);
                }

                float stacks = noise(x, y, 205, seed^77000325, 1);
                if (stacks > .61f)
                {
                        stacks -= .61f;
                        mountains += stacks;
                }

                if (mountains > 0.f)
                        h += mountains;
        }

        float mounds = noise(x, y, 290, seed^98453517, 1);
        if (mounds > .65f)
        {
                mounds -= .65f;
                h += mounds;
        }

        float lumps = noise(x, y, 175, seed^36447731, 1);
        if (lumps > .65f)
        {
                lumps -= .65f;
                h += lumps;
        }

        float pits = noise(x, y, 430, seed^77488339, 2);
        if (pits > .65f)
        {
                pits -= .65f;
                h -= pits;
        }

        float smoothouts = noise(x, y, 990, seed^13546936, 1);
        if (smoothouts > .65f)
        {
                float s = remap(smoothouts, .65f, 1.f, 0.f, 1000.f);
                h = get_height(x2 + s, y2 + s)
                  + get_height(x2 - s, y2 + s)
                  + get_height(x2 + s, y2 - s)
                  + get_height(x2 - s, y2 - s);
                h /= 4.f;
        }

        return h;
}

#endif // TINYCGAMES_TERRAIN_C_INCLUDED

#ifndef TINY_C_GAMES_TERRAIN_
#define TINY_C_GAMES_TERRAIN_

#include "taylor_noise.c"
#include "utils.c"

int get_height_hit, get_height_miss;
int seed = 104;

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
        if (x < 20 && y < 640) return (y - 240) / 20 / 20.f;

        float val = noise(x, y, 400, seed, 1);
        //val += (zigzag(val, 100) - .5f) * .02f;
        float denom = 1.f;

        float oceaniness = remap(noise(x, y, 2030, seed, 1), .5f, .6f, 0.f, 1.f);
        if (oceaniness > 0.f && val > 0.46f)
        {
                float ocean_val = .2f * val;
                float ocean_val2 = val * (1.f - oceaniness) + ocean_val * oceaniness;
                float ocean_blend = remap(val, 0.46f, 0.49f, 0.f, 1.f);
                val = val * (1.f - ocean_blend) + ocean_val2 * ocean_blend;
        }

        // totally nuts
        if (val < .48f)
        {
                float flippy = remap(noise(x, y, 2008, seed, 1), .65f, .68f, 0.f, 1.f);
                if (flippy > 0.f)
                {
                        float val2 = remap(val, 0.f, .48f, 1.f, .48f);
                        val = val2 * flippy + val * (1.f - flippy);
                }
        }

        float plateauness = remap(noise(x, y, 1200, seed, 1), .50f, .55f, 0.f, 1.f);
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

        float deepiness = remap(noise(x, y, 1150, seed, 1), .52f, .62f, 0.f, 1.f);
        if (deepiness > 0.f)
        {
                float deep_val = val;
                if (deep_val <= .49f && deep_val >= .44f)
                        deep_val = remap(deep_val, .44f, .49f, .2f, .49f);
                else if (deep_val <= .44f)
                        deep_val = remap(deep_val, 0.f, .44f, 0.f, .2f);
                val = deep_val * deepiness + val * (1.f - deepiness);
        }

        float intensity_med = CLAMP(remap(noise(x, y, 370, seed, 3) - .5f, -.5f, .5f, -12.f, 8.f), 0.f, 1.f) * .5f;
        if (intensity_med > 0.f)
        {
                val += noise(x, y, 100, seed, 3) * intensity_med;
                denom += intensity_med;
        }

        float intensity_sm = CLAMP(remap(noise(x, y, 100, seed, 3) - .5f, -.5f, .5f, -12.f, 8.f), 0.f, 1.f) * .5f;
        if (intensity_sm > 0.f)
        {
                val += noise(x, y, 100, seed, 3) * intensity_sm;
                denom += intensity_sm;
        }

        float intensity_xs = CLAMP(remap(noise(x, y, 80, seed, 3) - .5f, -.5f, .5f, -8.f, 8.f), -1.f, 0.f) * .05f;
        val += noise(x, y, 16, seed, 3) * intensity_xs;
        denom += intensity_xs;

        val /= denom;
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
                int originx = 600;
                int originy = 600;
                int tx = x2 - originx;
                int ty = y2 - originy;
                float dist = sqrtf(tx * tx + ty * ty) / 600.f;
                if (dist < 1.f)
                {
                        float ang =  .7f * (1.f - dist) * (1.f - dist) * (1.f - dist)
                                * remap(noise(x, y, 1080, seed, 3), .5f, 1.f, 2.f, 10.f);
                        x2 = originx + tx * cosf(ang) - ty * sinf(ang);
                        y2 = originy + tx * sinf(ang) + ty * cosf(ang);
                }
        }

        // wacky spiral rotation reverse!
        {
                int originx = 1000;
                int originy = 1000;
                int tx = x2 - originx;
                int ty = y2 - originy;
                float dist = sqrtf(tx * tx + ty * ty) / 600.f;
                if (dist < 1.f)
                {
                        float ang = -.7f * (1.f - dist) * (1.f - dist) * (1.f - dist)
                                * remap(noise(x, y, 1120, seed, 3), .5f, 1.f, 2.f, 10.f);
                        x2 = originx + tx * cosf(ang) - ty * sinf(ang);
                        y2 = originy + tx * sinf(ang) + ty * cosf(ang);
                }
        }

        float h = get_height(x2, y2);

        float smoothness = remap(noise(x2, y2, 500, seed, 1), .55f, 1.f, 0.f, 1.f);
        if (smoothness > 0.f)
        {
                smoothness = 1.f;
                float smooth_h = 0.f;
                for (int i = -3; i <= 3; i++) for (int j = -3; j <= 3; j++)
                        if (i || j)
                                smooth_h += get_height(x2 + i, y2 + j);
                smooth_h /= 48.f;
                h = lerp(smoothness, h, smooth_h);
        }

        return h;
}

#endif

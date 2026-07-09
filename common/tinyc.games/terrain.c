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

static float terrain_raw_height(int x, int y)
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
                h += mrange * (.06f + .85f * ridge + .30f * ridge2 * ridge2);
        }

        // ceiling bounce: mountains can't punch through the sky. crests that
        // would rise past a gently wandering ceiling (~Y10) fold back down
        // instead of pancaking flat, turning would-be domes into calderas - a
        // raised rim ringing a sunken crater floor. the ceiling wanders on a
        // low-frequency noise so rims sit at different heights and crater
        // mouths vary in size; the fold depth is clamped so floors stay
        // above the mid-mountain and don't punch back down to the plains.
        // (the game maps h=0.5 to sea level and h~1.56 to just below Y=0.)
        float ceil_h = 1.53f + .11f * (noise(x, y, 300, world_seed^0x0ca1de7, 1) - .5f);
        if (h > ceil_h)
        {
                float over = h - ceil_h;
                if (over > 0.38f) over = 0.38f; // clamp crater depth
                h = ceil_h - over;
        }

        return h;
}

#define LEDGE_CELL 192.f  // key-point spacing grid (blocks)

// one ledge key point per grid cell. everything about it (whether it exists,
// where inside the cell it sits, its radius, flat-core fraction and outline
// seed) is hashed from the cell, then cached so the raster scan derives it once
// per cell instead of once per sample. target (the natural height the ledge
// flattens to) is filled lazily the first time a query actually lands inside.
struct ledge_kp {
        int ci, cj, seed;
        char active, have_target;
        float kx, ky, R, core, target;
        float ca, sa, aspect;   // orientation + across-axis stretch (ovals/ridges)
        float wob_amp;          // rim wobble strength (high = pinched/degenerate)
        int kseed;
};

static struct ledge_kp *ledge_cell(int ci, int cj)
{
        static _Thread_local struct ledge_kp cells[8192];
        struct ledge_kp *c = &cells[noise_hash(ci, cj, world_seed ^ 0x1ed9e5) & 8191];
        if (c->ci == ci && c->cj == cj && c->seed == world_seed)
                return c;
        c->ci = ci;
        c->cj = cj;
        c->seed = world_seed;
        c->active = 0;
        c->have_target = 0;

        unsigned s = noise_hash(ci, cj, world_seed ^ 0x1ed9e5);
        if (!s) s = 1;
        float jx = (noise_rng(&s) >> 8) * (1.f / 0x1000000);
        float jy = (noise_rng(&s) >> 8) * (1.f / 0x1000000);
        float ractive = (noise_rng(&s) >> 8) * (1.f / 0x1000000);
        float rr = (noise_rng(&s) >> 8) * (1.f / 0x1000000);
        float rc = (noise_rng(&s) >> 8) * (1.f / 0x1000000);
        float rang = (noise_rng(&s) >> 8) * (1.f / 0x1000000);
        float rasp = (noise_rng(&s) >> 8) * (1.f / 0x1000000);
        float rwob = (noise_rng(&s) >> 8) * (1.f / 0x1000000);

        if (ractive > .85f) return c; // ~85% of in-range cells host a ledge

        c->kx = ci * LEDGE_CELL + jx * LEDGE_CELL;
        c->ky = cj * LEDGE_CELL + jy * LEDGE_CELL;

        // only inside mountain ranges (same mask as the range pass)
        float mr = remap(noise((int)c->kx, (int)c->ky, 4000,
                        world_seed^11223344, 2), .60f, .72f, 0.f, 1.f);
        if (mr <= 0.f) return c;

        c->R = 5.f + rr * 75.f;      // radius 5..80 (10..160 across)
        c->core = .30f + rc * .40f;  // flat-top fraction 0.3..0.7

        float ang = rang * 6.2831853f;
        c->ca = cosf(ang);
        c->sa = sinf(ang);
        c->aspect = 1.f + rasp * 2.5f;   // 1..3.5: round through long ridge
        c->wob_amp = .7f + rwob * 1.4f;  // 0.7..2.1: gentle rim through pinched-off lobes

        c->kseed = (int)noise_hash(ci, cj, world_seed ^ 0x513a9e);
        c->active = 1;
        return c;
}

float get_filtered_height(int x, int y)
{
        float h = terrain_raw_height(x, y);

        // ledges & plateaus: scattered invisible key points inside mountain
        // ranges pull the surrounding terrain toward the key point's natural
        // height, carving a flat shelf to stand on. the effect eases to zero
        // at the rim so it blends smoothly into the untouched slope. each key
        // point's size (10..160 blocks across), flat-core fraction and wobbly
        // outline are hashed from its cell, so no two ledges look the same.
        {
                int qcx = (int)floorf(x / LEDGE_CELL);
                int qcy = (int)floorf(y / LEDGE_CELL);
                float best_w = 0.f;
                float best_target = 0.f;
                for (int ci = qcx-1; ci <= qcx+1; ci++)
                for (int cj = qcy-1; cj <= qcy+1; cj++)
                {
                        struct ledge_kp *c = ledge_cell(ci, cj);
                        if (!c->active) continue;

                        float dx = x - c->kx, dy = y - c->ky;
                        float draw = sqrtf(dx * dx + dy * dy);
                        if (draw >= c->R * (.25f + c->wob_amp)) continue; // past max wobbled rim

                        // anisotropic distance: rotate into the key point's frame and
                        // stretch the across-axis, turning circles into ovals and ridges
                        float rx =  dx * c->ca + dy * c->sa;
                        float ry = (-dx * c->sa + dy * c->ca) * c->aspect;
                        float d = sqrtf(rx * rx + ry * ry);

                        // two octaves of coherent noise carve lobes, bites and pinches
                        // into the rim; a big amplitude can starve the radius to nothing,
                        // splitting the shelf into crescents or disconnected patches
                        float w1 = noise(x, y, (int)(c->R * 1.3f) + 8, c->kseed, 2);
                        float w2 = noise(x, y, (int)(c->R * 0.5f) + 5, c->kseed ^ 0x2b7, 2);
                        float wob = w1 * .6f + w2 * .4f;   // ~0..1
                        float rad = c->R * (.25f + c->wob_amp * wob);
                        if (rad < 0.5f) rad = 0.5f;

                        float t = d / rad;             // 0 center .. 1 rim
                        if (t >= 1.f) continue;

                        float w;
                        if (t <= c->core)
                                w = 1.f;               // flat shelf
                        else
                        {
                                w = (1.f - t) / (1.f - c->core);
                                w = w * w * (3.f - 2.f * w); // smoothstep ease-out
                        }

                        if (w <= best_w) continue;     // a stronger ledge already wins here
                        if (!c->have_target)
                        {
                                c->target = terrain_raw_height((int)c->kx, (int)c->ky);
                                c->have_target = 1;
                        }
                        best_w = w;
                        best_target = c->target;
                }

                if (best_w > 0.f)
                        h = lerp(best_w, h, best_target);
        }

        return h;
}

#endif // TINYCGAMES_TERRAIN_C_INCLUDED

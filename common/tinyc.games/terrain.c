#ifndef TINYCGAMES_TERRAIN_C_INCLUDED
#define TINYCGAMES_TERRAIN_C_INCLUDED

#include <math.h>
#include "taylor_noise.c"
#include "utils.c"
#include "warp_config.c"
#include "ledge_config.c"

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

// one domain-warp key point: a spiral that twists the sampling grid or a
// bubble that bulges/pinches it. position, radius, kind, strength and spin are
// hashed per cell + slot, then cached so the raster scan derives them once per
// cell rather than per sample. all knobs live in warp_config.c.
struct warp_kp {
        float ox, oy, radius, strength;
        char is_bubble;
        int nseed;              // seed for the spiral's angle-variation noise
};

struct warp_cell {
        int ci, cj, seed, n;
        struct warp_kp kp[WARP_MAX];
};

static struct warp_cell *warp_cell_get(int ci, int cj)
{
        static _Thread_local struct warp_cell cells[WARP_CACHE];
        struct warp_cell *wc = &cells[noise_hash(ci, cj, world_seed ^ WARP_SALT_CELL) & (WARP_CACHE - 1)];
        if (wc->ci == ci && wc->cj == cj && wc->seed == world_seed)
                return wc;
        wc->ci = ci;
        wc->cj = cj;
        wc->seed = world_seed;
        wc->n = 0;

        // low-frequency density field: warps cluster in some regions, skip others
        float density = remap(noise(ci * (int)WARP_CELL, cj * (int)WARP_CELL,
                        WARP_DENSITY_SZ, world_seed ^ WARP_SALT_DENSITY, 2),
                        WARP_DENSITY_LO, WARP_DENSITY_HI, 0.f, 1.f);
        if (density <= 0.f) return wc;

        unsigned s = noise_hash(ci, cj, world_seed ^ WARP_SALT_CELL);
        if (!s) s = 1;

        for (int k = 0; k < WARP_MAX; k++)
        {
                float slot = (noise_rng(&s) >> 8) * (1.f / 0x1000000);
                float jx = (noise_rng(&s) >> 8) * (1.f / 0x1000000);
                float jy = (noise_rng(&s) >> 8) * (1.f / 0x1000000);
                float rr = (noise_rng(&s) >> 8) * (1.f / 0x1000000);
                float rt = (noise_rng(&s) >> 8) * (1.f / 0x1000000);
                float rs = (noise_rng(&s) >> 8) * (1.f / 0x1000000);
                float rsign = (noise_rng(&s) >> 8) * (1.f / 0x1000000);

                if (slot >= density) continue; // this slot stays empty

                struct warp_kp *w = &wc->kp[wc->n++];
                w->ox = ci * WARP_CELL + jx * WARP_CELL;
                w->oy = cj * WARP_CELL + jy * WARP_CELL;
                w->radius = WARP_R_MIN + rr * (WARP_R_MAX - WARP_R_MIN);
                w->is_bubble = (rt < WARP_BUBBLE_FRAC);
                float sign = (rsign < .5f) ? -1.f : 1.f;
                if (w->is_bubble)
                        w->strength = sign * (WARP_BUBBLE_STR_MIN + rs * (WARP_BUBBLE_STR_MAX - WARP_BUBBLE_STR_MIN));
                else
                        w->strength = sign * (WARP_SPIRAL_STR_MIN + rs * (WARP_SPIRAL_STR_MAX - WARP_SPIRAL_STR_MIN));
                w->nseed = (int)noise_hash(ci, cj * WARP_MAX + k, world_seed ^ WARP_SALT_NOISE);
        }
        return wc;
}

static float terrain_raw_height(int x, int y)
{
        // scattered domain warp: spirals twist the sampling grid, bubbles bulge
        // it. every active key point near (x,y) accumulates its displacement,
        // then the base height is sampled at the warped coordinate. everything
        // downstream keeps using the true (x,y).
        float ddx = 0.f, ddy = 0.f;
        int qcx = (int)floorf(x / WARP_CELL);
        int qcy = (int)floorf(y / WARP_CELL);
        for (int ci = qcx-1; ci <= qcx+1; ci++)
        for (int cj = qcy-1; cj <= qcy+1; cj++)
        {
                struct warp_cell *wc = warp_cell_get(ci, cj);
                for (int k = 0; k < wc->n; k++)
                {
                        struct warp_kp *w = &wc->kp[k];
                        float tx = x - w->ox, ty = y - w->oy;
                        float dist = sqrtf(tx * tx + ty * ty) / w->radius;
                        if (dist >= 1.f) continue;

                        if (w->is_bubble)
                        {
                                float dd = dist;
                                if (dd < .5f) dd = 1.f - dd; // ring bulge peaks mid-radius
                                float m = (1.f - dd) * (1.f - dd) * w->strength;
                                ddx += (tx / dd) * m;
                                ddy += (ty / dd) * m;
                        }
                        else // spiral: rotate (x,y) about the key point by ang
                        {
                                float f = 1.f - dist;
                                f = f * f * f;               // cubic falloff to the rim
                                float ang = w->strength * f * remap(
                                        noise(x, y, WARP_SPIRAL_NOISE_SZ, w->nseed, 3),
                                        .5f, 1.f, 0.f, WARP_SPIRAL_ANG);
                                float cs = cosf(ang), sn = sinf(ang);
                                ddx += tx * (cs - 1.f) - ty * sn;
                                ddy += tx * sn + ty * (cs - 1.f);
                        }
                }
        }
        int x2 = (int)(x + ddx);
        int y2 = (int)(y + ddy);

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

// each grid cell holds 0..LEDGE_MAX ledge key points. how many is steered by a
// low-frequency density field, so whole regions come out dense with shelves,
// sparse, or bare. everything about each key point (position within the cell,
// radius, flat-core fraction, orientation, wobble) is hashed from the cell +
// slot, then cached so the raster scan derives it once per cell rather than per
// sample. target (the natural height the ledge flattens to) is filled lazily
// the first time a query actually lands inside. all tuning knobs live in
// ledge_config.c.
struct ledge_kp {
        char have_target;
        float kx, ky, R, core, target;
        float ca, sa, aspect;   // orientation + across-axis stretch (ovals/ridges)
        float wob_amp;          // rim wobble strength (high = pinched/degenerate)
        int kseed;
};

struct ledge_cell {
        int ci, cj, seed, n;
        struct ledge_kp kp[LEDGE_MAX];
};

static struct ledge_cell *ledge_cell_get(int ci, int cj)
{
        static _Thread_local struct ledge_cell cells[LEDGE_CACHE];
        struct ledge_cell *cc = &cells[noise_hash(ci, cj, world_seed ^ LEDGE_SALT_CELL) & (LEDGE_CACHE - 1)];
        if (cc->ci == ci && cc->cj == cj && cc->seed == world_seed)
                return cc;
        cc->ci = ci;
        cc->cj = cj;
        cc->seed = world_seed;
        cc->n = 0;

        // low-frequency density field: broad regions run thick with ledges,
        // others have a few, others none. each slot is filled with probability
        // = density, so the count varies cell to cell within a region.
        float density = remap(noise(ci * (int)LEDGE_CELL, cj * (int)LEDGE_CELL,
                        LEDGE_DENSITY_SZ, world_seed ^ LEDGE_SALT_DENSITY, 2),
                        LEDGE_DENSITY_LO, LEDGE_DENSITY_HI, 0.f, 1.f);
        if (density <= 0.f) return cc;

        unsigned s = noise_hash(ci, cj, world_seed ^ LEDGE_SALT_CELL);
        if (!s) s = 1;

        for (int k = 0; k < LEDGE_MAX; k++)
        {
                float slot = (noise_rng(&s) >> 8) * (1.f / 0x1000000);
                float jx = (noise_rng(&s) >> 8) * (1.f / 0x1000000);
                float jy = (noise_rng(&s) >> 8) * (1.f / 0x1000000);
                float rr = (noise_rng(&s) >> 8) * (1.f / 0x1000000);
                float rc = (noise_rng(&s) >> 8) * (1.f / 0x1000000);
                float rang = (noise_rng(&s) >> 8) * (1.f / 0x1000000);
                float rasp = (noise_rng(&s) >> 8) * (1.f / 0x1000000);
                float rwob = (noise_rng(&s) >> 8) * (1.f / 0x1000000);

                if (slot >= density) continue; // this slot stays empty

                float kx = ci * LEDGE_CELL + jx * LEDGE_CELL;
                float ky = cj * LEDGE_CELL + jy * LEDGE_CELL;

                // only inside mountain ranges (mirrors the range mask in terrain_raw_height)
                float mr = remap(noise((int)kx, (int)ky, LEDGE_RANGE_SZ,
                                world_seed ^ LEDGE_RANGE_SEED, 2),
                                LEDGE_RANGE_LO, LEDGE_RANGE_HI, 0.f, 1.f);
                if (mr <= 0.f) continue;

                struct ledge_kp *c = &cc->kp[cc->n++];
                c->have_target = 0;
                c->kx = kx;
                c->ky = ky;
                c->R = LEDGE_R_MIN + rr * (LEDGE_R_MAX - LEDGE_R_MIN);
                c->core = LEDGE_CORE_MIN + rc * (LEDGE_CORE_MAX - LEDGE_CORE_MIN);

                float ang = rang * LEDGE_TWO_PI;
                c->ca = cosf(ang);
                c->sa = sinf(ang);
                c->aspect = LEDGE_ASPECT_MIN + rasp * (LEDGE_ASPECT_MAX - LEDGE_ASPECT_MIN);
                c->wob_amp = LEDGE_WOB_MIN + rwob * (LEDGE_WOB_MAX - LEDGE_WOB_MIN);

                c->kseed = (int)noise_hash(ci, cj * LEDGE_SLOT_SPREAD + k, world_seed ^ LEDGE_SALT_KP);
        }
        return cc;
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
                        struct ledge_cell *cc = ledge_cell_get(ci, cj);
                        for (int k = 0; k < cc->n; k++)
                        {
                                struct ledge_kp *c = &cc->kp[k];

                                float dx = x - c->kx, dy = y - c->ky;
                                float draw = sqrtf(dx * dx + dy * dy);
                                if (draw >= c->R * (LEDGE_RAD_BASE + c->wob_amp)) continue; // past max wobbled rim

                                // anisotropic distance: rotate into the key point's frame and
                                // stretch the across-axis, turning circles into ovals and ridges
                                float rx =  dx * c->ca + dy * c->sa;
                                float ry = (-dx * c->sa + dy * c->ca) * c->aspect;
                                float d = sqrtf(rx * rx + ry * ry);

                                // two octaves of coherent noise carve lobes, bites and pinches
                                // into the rim; a big amplitude can starve the radius to nothing,
                                // splitting the shelf into crescents or disconnected patches
                                float w1 = noise(x, y, (int)(c->R * LEDGE_WOB1_FREQ) + LEDGE_WOB1_FLOOR, c->kseed, 2);
                                float w2 = noise(x, y, (int)(c->R * LEDGE_WOB2_FREQ) + LEDGE_WOB2_FLOOR, c->kseed ^ LEDGE_SALT_WOB2, 2);
                                float wob = w1 * LEDGE_WOB1_WT + w2 * LEDGE_WOB2_WT;   // ~0..1
                                float rad = c->R * (LEDGE_RAD_BASE + c->wob_amp * wob);
                                if (rad < LEDGE_RAD_FLOOR) rad = LEDGE_RAD_FLOOR;

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
                                if (best_w >= 1.f) goto ledge_done; // on a flat core; nothing beats it
                        }
                }
ledge_done:

                if (best_w > 0.f)
                        h = lerp(best_w, h, best_target);
        }

        return h;
}

#endif // TINYCGAMES_TERRAIN_C_INCLUDED

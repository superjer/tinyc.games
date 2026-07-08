#include "blocko.c"
#ifndef BLOCKO_FORMATIONS_C_INCLUDED
#define BLOCKO_FORMATIONS_C_INCLUDED

// formations.c - overhanging rock via surface-anchored blob walks
//
// 3D density noise makes overhangs but also floating islands; this can't.
// Each large 2D cell hashes to at most one formation: a chain of overlapping
// spheres random-walked up and out of the terrain from an anchor embedded in
// the ground. Sphere 0 intersects the ground and every sphere overlaps the
// previous one, so the whole blob is connected to the terrain by
// construction. The chunker asks each column for extra solid y-spans
// (form_spans) and stamps them as stone.
//
// Everything derives from noise_hash(cell, world_seed): deterministic across
// platforms and threads, same guarantees as taylor noise.

#define FORM_CELL 64            // one formation max per cell of this size
#define FORM_CELL_BITS 6
#define FORM_MAX_SPHERES 28

int   form_enable = 1;
float form_region = .56f;       // region mask threshold (lower = more regions)
float form_chance = .50f;       // per-cell spawn chance inside a region
int   form_steps  = 12;         // max steps in the trunk walk
float form_rmin   = 3.f;
float form_rmax   = 15.f;
int   form_config_gen;          // bump to invalidate memos after knob changes

struct formation {
        int ci, cj, gen, state, n;
        float x[FORM_MAX_SPHERES], y[FORM_MAX_SPHERES],
              z[FORM_MAX_SPHERES], r[FORM_MAX_SPHERES],
              sq[FORM_MAX_SPHERES]; // vertical squash: 1 = sphere, < 1 = flat
};
// indexed by cell coords directly so neighboring cells never collide
static _Thread_local struct formation form_memos[32][16];

#define FRAND01(s) ((noise_rng(&(s)) >> 8) * (1.f / 0x1000000))

// one limb of a formation: emit flat overlapping ellipsoids along a
// horizontal random walk, recursing to fork off limbs like tree branches.
// Each blob is pinned toward the terrain surface at its own (x,z), so on
// open ground formations half-bury into subtle stone outcrops and ledges.
// The pin chases the ground at a connectivity-limited rate: walking off a
// cliff, blobs can't descend as fast as the terrain does and are left
// hanging past the edge - that's where the overhangs come from. "Plunge"
// blobs are rounder, may descend faster, and dive back to lower ground,
// planting arch legs.
static void form_walk(struct formation *f, unsigned *s,
                float px, float py, float pz, float dirx, float dirz,
                float r, float sq, int steps, int depth,
                float anchorx, float anchorz)
{
        float maxdrift = FORM_CELL - 2.f * form_rmax;

        for (int i = 0; i < steps; i++)
        {
                if (f->n >= FORM_MAX_SPHERES) return;

                f->x[f->n] = px;
                f->y[f->n] = py;
                f->z[f->n] = pz;
                f->r[f->n] = r;
                f->sq[f->n] = sq;
                f->n++;

                // fork a smaller limb off to the side
                if (depth < 2 && i > 0 && steps - i > 1 && FRAND01(*s) < .3f)
                {
                        float ba = (.6f + FRAND01(*s)) * (FRAND01(*s) < .5f ? -1.f : 1.f);
                        float ca = cosf(ba), sa = sinf(ba);
                        form_walk(f, s, px, py, pz,
                                dirx * ca - dirz * sa, dirx * sa + dirz * ca,
                                r * (.55f + .25f * FRAND01(*s)), sq,
                                steps - i, depth + 1, anchorx, anchorz);
                }

                float nr = r * (.6f + .9f * FRAND01(*s)); // sizes vary a lot
                if (nr > form_rmax) nr = form_rmax;
                if (nr < 2.f) return;
                float nsq = .25f + .2f * FRAND01(*s);     // flat
                if (FRAND01(*s) < .12f)
                        nsq = .6f + .3f * FRAND01(*s);    // plunge: round leg

                float ang = (FRAND01(*s) - .5f) * 1.9f;   // wander +/- ~55 deg
                float ca = cosf(ang), sa = sinf(ang);
                float ndx = dirx * ca - dirz * sa;
                float ndz = dirx * sa + dirz * ca;
                dirx = ndx;
                dirz = ndz;

                float horiz = (r + nr) * (.4f + .2f * FRAND01(*s));
                px += dirx * horiz;
                pz += dirz * horiz;

                // chase the ground, at most as fast as squash-metric overlap
                // with the previous blob allows; the .7 keeps the overlap
                // lens thick enough to survive voxelization and erosion
                float ground = TILESH
                        - get_filtered_height((int)px, (int)pz) * TERRAIN_VSCALE;
                float sqmin = sq < nsq ? sq : nsq;
                float span = .7f * (r + nr);
                float dymax = sqmin * sqrtf(span*span - horiz*horiz);
                float dy = ground + .3f * nr * nsq - py;  // center just underground
                if (dy >  dymax) dy =  dymax;
                if (dy < -dymax) dy = -dymax;
                py += dy;
                r = nr;
                sq = nsq;

                float ddx = px - anchorx, ddz = pz - anchorz;
                if (ddx*ddx + ddz*ddz > maxdrift*maxdrift) return;
        }
}

struct formation *get_formation(int ci, int cj)
{
        struct formation *f = &form_memos[ci & 31][cj & 15];
        if (f->state && f->ci == ci && f->cj == cj && f->gen == form_config_gen)
                return f;

        f->ci = ci;
        f->cj = cj;
        f->gen = form_config_gen;
        f->state = 1;
        f->n = 0;

        unsigned s = noise_hash(ci, cj, world_seed ^ 0xB10B);
        if (!s) s = 1;

        // clustered, not sprinkled: only cells inside a low-frequency mask
        float region = noise(ci * FORM_CELL + FORM_CELL/2, cj * FORM_CELL + FORM_CELL/2,
                        2500, world_seed ^ 0x0F0F0F0F, 2);
        if (region < form_region || FRAND01(s) > form_chance)
                return f;

        // pick an anchor on the surface, preferring the steepest ground
        // sampled so crags grow off cliff brows and mountainsides
        int ax = 0, az = 0;
        float ah = 0.f, agx = 0.f, agz = 0.f, best = -1.f;
        for (int t = 0; t < 5; t++)
        {
                int tx = ci * FORM_CELL + noise_rng(&s) % FORM_CELL;
                int tz = cj * FORM_CELL + noise_rng(&s) % FORM_CELL;
                float h = get_filtered_height(tx, tz);
                if (h < .46f) continue; // deep ocean: no formations
                float gx = (get_filtered_height(tx+4, tz) - get_filtered_height(tx-4, tz)) / 8.f;
                float gz = (get_filtered_height(tx, tz+4) - get_filtered_height(tx, tz-4)) / 8.f;
                float steep = gx*gx + gz*gz;
                if (steep > best)
                {
                        best = steep;
                        ax = tx; az = tz; ah = h;
                        agx = gx; agz = gz;
                }
        }
        if (best < 0.f) return f; // whole cell is deep water

        // walk outward = downhill, so blobs lean over lower ground;
        // random heading on flats (freestanding hoodoos)
        float dirx, dirz;
        float gl = sqrtf(agx*agx + agz*agz);
        if (gl * TERRAIN_VSCALE < .15f)
        {
                float a = FRAND01(s) * 6.2831853f;
                dirx = cosf(a);
                dirz = sinf(a);
        }
        else
        {
                dirx = -agx / gl;
                dirz = -agz / gl;
        }

        int nsteps = 6 + noise_rng(&s) % (form_steps > 6 ? form_steps - 5 : 1);

        // the anchor drift limit inside form_walk keeps every ellipsoid
        // close enough to the anchor cell that a 3x3 cell query from any
        // covered column finds this formation
        float r = form_rmin + FRAND01(s) * (form_rmax - form_rmin) * .8f;
        float sq = .25f + .2f * FRAND01(s);
        form_walk(f, &s, ax + .5f,
                TILESH - ah * TERRAIN_VSCALE + .3f * r * sq, az + .5f,
                dirx, dirz, r, sq, nsteps, 0, ax + .5f, az + .5f);

        return f;
}

// solid y-intervals formations add to column (ax,az); returns interval count
int form_spans(int ax, int az, int *lo, int *hi, int max)
{
        if (!form_enable) return 0;
        int n = 0;
        int ci = ax >> FORM_CELL_BITS;
        int cj = az >> FORM_CELL_BITS;

        // Erosion so blobs don't read as smooth squashed spheres. Both parts
        // reshape only a blob's TOP; the belly and every span's bottom row
        // survive untouched, so nothing can be disconnected - removing rows
        // downward from a dome's top can never strand what remains.
        float grit = -1.f;

        for (int i = ci-1; i <= ci+1; i++) for (int j = cj-1; j <= cj+1; j++)
        {
                struct formation *f = get_formation(i, j);
                for (int k = 0; k < f->n && n < max; k++)
                {
                        float dx = ax + .5f - f->x[k];
                        float dz = az + .5f - f->z[k];
                        float dsq = dx*dx + dz*dz;
                        float r = f->r[k];
                        float rsq = r * r;
                        if (dsq >= rsq) continue;
                        float sq = f->sq[k];
                        float u = dsq / rsq;
                        // superellipse belly: flatter underside and more
                        // vertical rim walls than a sphere's egg bulge
                        float hd = sq * r * sqrtf(1.f - u*u);

                        // mesa top: flat tiers from a stateless per-blob
                        // hash, pushed off-center so one side is a steep
                        // wall and the other a terraced slope. Round plunge
                        // blobs keep their dome - their tops are buried
                        // under the next blob anyway.
                        float hu = hd;
                        if (sq < .5f)
                        {
                                unsigned bs = noise_hash(i * FORM_MAX_SPHERES + k, j, world_seed ^ 0x715A);
                                if (!bs) bs = 1;
                                float ex = dx - (FRAND01(bs) - .5f) * .8f * r;
                                float ez = dz - (FRAND01(bs) - .5f) * .8f * r;
                                float t1 = (.15f + .25f * FRAND01(bs)) * rsq;
                                float t2 = t1 + (.2f + .3f * FRAND01(bs)) * rsq;
                                float e = ex*ex + ez*ez;
                                hu = sq * r * (e < t1 ? .75f + .25f * FRAND01(bs) :
                                               e < t2 ? .4f + .2f * FRAND01(bs) :
                                                        .15f + .1f * FRAND01(bs));
                                if (hu > hd) hu = hd;
                        }

                        // per-column grit chews pits into the top surface
                        if (grit < 0.f)
                        {
                                unsigned es = noise_hash(ax, az, world_seed ^ 0xE60DE);
                                if (!es) es = 1;
                                grit = 1.6f * FRAND01(es) - .8f;
                                if (grit < 0.f) grit = 0.f; // half the columns unchewed
                        }
                        hu -= grit;

                        // the thin outer apron gets dropped (chunky rim
                        // wall instead of a feathered circular skirt); every
                        // kept span retains the blob's equator row, so the
                        // disk always stays laterally connected there
                        if (hd < .5f && dsq > .8f * rsq) continue;
                        float fr = f->y[k] - floorf(f->y[k]);
                        if (hu < fr) hu = fr;

                        int h = (int)floorf(f->y[k] + hd);
                        int l = (int)ceilf(f->y[k] - hu);
                        if (l < 1) l = 1;
                        if (h > TILESH-2) h = TILESH-2;
                        if (l <= h)
                        {
                                lo[n] = l;
                                hi[n] = h;
                                n++;
                        }
                }
        }
        return n;
}

#endif // BLOCKO_FORMATIONS_C_INCLUDED

#include "blocko.c"
#ifndef BLOCKO_CHUNKER_C_INCLUDED
#define BLOCKO_CHUNKER_C_INCLUDED

#include "../common/tinyc.games/taylor_noise.c"
#include "../common/tinyc.games/terrain.c"

// cave systems live in absolute coords, one per region. the walk reflects
// off the region walls and the world top/bottom, so caves stay where the
// region's own chunks will carve them instead of wandering off unseen
#define REGW (CHUNKW*4)
#define REGD (CHUNKD*4)
#define MAX_CAVE_POINTS 16000
#define QCAVE(x,y,z,radius_sq) ((struct qcave){x, y, z, radius_sq})
#define REFLECT(v, lo, hi) { if (v < (lo)) v = 2*(lo) - v; if (v > (hi)) v = 2*(hi) - v; }

static void gen_pass(unsigned *t0, int which)
{
        unsigned t = SDL_GetTicks();
        #pragma omp atomic
        gen_pass_ms[which] += t - *t0;
        *t0 = t;
}

// generate soil, caves, and initial sunlight for columns [xlo,xhi) x [zlo,zhi).
// a range never crosses a chunk boundary, and nothing here reads or writes
// world state outside the range - neighbor heights come straight from noise
void gen_columns(int xlo, int xhi, int zlo, int zhi)
{
        unsigned t0 = SDL_GetTicks();

        // heights for every column plus a 1-column border - the steepness
        // test reads each neighbor's height, so compute each column once
        static _Thread_local int hmap[CHUNKD+4][CHUNKW+4];
        #define HMAP(xx, zz) hmap[(zz)-(zlo-1)][(xx)-(xlo-1)]
        for (int z = zlo-1; z <= zhi; z++) for (int xx = xlo-1; xx <= xhi; xx++)
                HMAP(xx, z) = TILESH
                        - get_filtered_height(xx - tscootx, z - tscootz) * TERRAIN_VSCALE;
        gen_pass(&t0, GEN_HMAP);

        // fill a run of one column with one tile, clipped to the solid region
        #define BAND(a, b, v) { int a_ = MAX(a, gnd), b_ = MIN(b, TILESH-1); \
                if (b_ > a_) memset(t + a_, v, b_ - a_); }

        for (int x = xlo; x < xhi; x++) for (int z = zlo; z < zhi; z++)
        {
                int ax = x - tscootx; // absolute world coords: feed these to all
                int az = z - tscootz; // noise/seeds so terrain survives scooting

                int hmaph = HMAP(x, z);

                int hx0 = HMAP(x+1, z);
                int hz0 = HMAP(x, z+1);
                int hx1 = HMAP(x-1, z);
                int hz1 = HMAP(x, z-1);

                bool sharp_dn = hmaph - hx0 < -1 || hmaph - hz0 < -1 || hmaph - hx1 < -1 || hmaph - hz1 < -1;
                bool sharp_up = hmaph - hx0 >  1 || hmaph - hz0 >  1 || hmaph - hx1 >  1 || hmaph - hz1 >  1;
                bool sharper_dn = hmaph - hx0 < -3 || hmaph - hz0 < -3 || hmaph - hx1 < -3 || hmaph - hz1 < -3;
                bool sharper_up = hmaph - hx0 >  3 || hmaph - hz0 >  3 || hmaph - hx1 >  3 || hmaph - hz1 >  3;
                bool steep = (sharp_dn && sharp_up) || sharper_dn;

                float reejin = noise(ax, az, 350, 12345, 1) - 0.5f;
                int lev1 = SEA_LEVEL - 60 + (int)(reejin * 100.f);
                int lev2 = SEA_LEVEL - 30 + (int)(reejin * 100.f);
                int lev3 = SEA_LEVEL      + (int)(reejin * 50.f);
                int lev4 = SEA_LEVEL + 45 + (int)(reejin * 50.f);

                // vegetation lines, by absolute altitude (smaller y = higher):
                // ordinary grass gives way to mountain grass (MTGR) well above
                // the shore at mtn_line, and MTGR gives way to bare rock at
                // barren near the peaks. a fine noise ragged-edges both so they
                // aren't clean contours. keyed to altitude, not the soil bands,
                // so mountain grass never creeps back down toward sea level.
                float rough = noise(ax, az, 40, 0x51ef, 1) - 0.5f;
                int mtn_line = SEA_LEVEL - 55 + (int)(rough * 16.f); // grass -> MTGR
                int barren   = SEA_LEVEL - 98 + (int)(rough * 26.f); // MTGR -> rock

                int flo[16], fhi[16];
                int fn = form_spans(ax, az, flo, fhi, 16);

                unsigned char *t = &TT_(x, 0, z);
                int gnd = CLAMP(hmaph, 0, TILESH-1);

                // air and sea above the ground line, as two runs
                int sky = MIN(gnd, SEA_LEVEL+1);
                memset(t, OPEN, sky);
                memset(t + sky, WATR, gnd - sky);

                // formation spans hang in the air region. sort and merge
                // touching spans so the soil cap depth below counts
                // consecutive blocks across overlapping blobs
                for (int i = 1; i < fn; i++) for (int j = i; j > 0 && flo[j] < flo[j-1]; j--)
                {
                        int tmp;
                        tmp = flo[j]; flo[j] = flo[j-1]; flo[j-1] = tmp;
                        tmp = fhi[j]; fhi[j] = fhi[j-1]; fhi[j-1] = tmp;
                }
                int fm = 0;
                for (int i = 1; i < fn; i++)
                {
                        if (flo[i] <= fhi[fm] + 1) { if (fhi[i] > fhi[fm]) fhi[fm] = fhi[i]; }
                        else { fm++; flo[fm] = flo[i]; fhi[fm] = fhi[i]; }
                }
                if (fn) fn = fm + 1;

                for (int i = 0; i < fn; i++)
                {
                        // formations are solid granite, top to bottom
                        int l = flo[i];
                        int h = MIN(fhi[i], gnd - 1);
                        if (l > h) continue;
                        memset(t + l, GRAN, h + 1 - l);
                }

                // solid ground: constant runs between the soil band levels
                BAND(gnd,  lev1, STON);
                BAND(lev1, lev2, DIRT);
                BAND(lev2, lev3, DIRT);
                BAND(lev3, MIN(lev4, hmaph + 4), SAND);
                BAND(MAX(lev3, hmaph + 4), lev4, STON);
                BAND(lev4, TILESH-1, GRAN);
                // surface block: leave the sand band showing at and
                // below the waterline (beaches and the shallow bed) -
                // it appears wherever lev3 dips below the local surface.
                // above the sandy shelf, pick by altitude: bare rock on
                // the peaks, mountain grass on the upper slopes, then
                // ordinary grass on down to the shore.
                if (hmaph < lev3)
                {
                        if      (hmaph > SEA_LEVEL + 1) t[hmaph] = SAND; // shallow underwater
                        else if (hmaph < barren)   ; // bare stone peak (leave the STON band)
                        else if (hmaph < mtn_line) t[hmaph] = MTGR;
                        else                       t[hmaph] = GRAS;
                }
                // steep cliffs are bare stone below the top two blocks, so the
                // surface soil/grass stays but the exposed face is rock
                if (steep)
                        BAND(gnd + 2, TILESH-1, STON);
                t[TILESH-1] = GRAN;

                // sprinkle ore through the stone. a positional hash shared
                // across each 2x2x2 cell clumps hits into little veins rather
                // than lone specks; only stone converts, so grass/dirt/sand
                // surfaces and the deep granite are left alone
                for (int y = gnd; y < TILESH-1; y++)
                {
                        if (t[y] != STON) continue;
                        unsigned seed = SEED3(ax >> 1, y >> 1, az >> 1);
                        if      (RANDP(5)) t[y] = ORE;  // a real ore vein
                        else if (RANDP(7)) t[y] = OREH; // a hint of ore in the rock
                }
        }
        #undef BAND
        gen_pass(&t0, GEN_SOIL);

        // find nearby bezier curvy caves
        // (& with power-of-2 floors correctly when negative)
        int rxlo = (xlo - tscootx) & ~(REGW-1);
        int rzlo = (zlo - tscootz) & ~(REGD-1);
        unsigned seed = SEED2(rxlo, rzlo);
        // find region center
        int rxcenter = rxlo + REGW/2;
        int rzcenter = rzlo + REGD/2;
        struct point PC = (struct point){rxcenter, TILESH - RANDI(1, 25), rzcenter};
        REFLECT(PC.y, 8, TILESH - 8);
        struct point P0;
        struct point P1;
        struct point P2;
        struct point P3 = PC;
        int nr_caves = cave_enable ? RANDI(0, 12) : 0;

        // cave system stretchiness
        int sx = RANDI(10, 60);
        int sy = RANDI(10, 60);
        int sz = RANDI(10, 60);

        struct qcave cave_points[MAX_CAVE_POINTS];
        int cave_p_len = 0;

        for (int i = 0; i < nr_caves; i++)
        {
                // random walk from center of region, or end of last curve
                P0 = RANDP(33) ? PC : P3;
                P1 = (struct point){P0.x + RANDI(-sx, sx), P0.y + RANDI(-sy, sy), P0.z + RANDI(-sz, sz)};
                P2 = (struct point){P1.x + RANDI(-sx, sx), P1.y + RANDI(-sy, sy), P1.z + RANDI(-sz, sz)};
                P3 = (struct point){P2.x + RANDI(-sx, sx), P2.y + RANDI(-sy, sy), P2.z + RANDI(-sz, sz)};
                // a bezier stays inside its control points' box, so
                // reflecting the controls keeps the whole curve in bounds
                REFLECT(P1.x, rxlo + 8, rxlo + REGW - 8);
                REFLECT(P2.x, rxlo + 8, rxlo + REGW - 8);
                REFLECT(P3.x, rxlo + 8, rxlo + REGW - 8);
                REFLECT(P1.z, rzlo + 8, rzlo + REGD - 8);
                REFLECT(P2.z, rzlo + 8, rzlo + REGD - 8);
                REFLECT(P3.z, rzlo + 8, rzlo + REGD - 8);
                REFLECT(P1.y, 8, TILESH - 8);
                REFLECT(P2.y, 8, TILESH - 8);
                REFLECT(P3.y, 8, TILESH - 8);

                float root_radius = 0.f, delta = 0.f;

                for (float t = 0.f; t <= 1.f; t += 0.001f)
                {
                        if (cave_p_len >= MAX_CAVE_POINTS) break;

                        if (root_radius == 0.f || RANDP(0.002f))
                        {
                                root_radius = RAND01;
                                delta = RANDF(-0.001f, 0.001f);
                        }

                        root_radius += delta;
                        float radius_sq = root_radius * root_radius * root_radius * root_radius * 50.f;
                        radius_sq = CLAMP(radius_sq, 1.f, 50.f);

                        float s = 1.f - t;
                        int x = (int)(s*s*s*P0.x + 3.f*t*s*s*P1.x + 3.f*t*t*s*P2.x + t*t*t*P3.x);
                        int y = (int)(s*s*s*P0.y + 3.f*t*s*s*P1.y + 3.f*t*t*s*P2.y + t*t*t*P3.y);
                        int z = (int)(s*s*s*P0.z + 3.f*t*s*s*P1.z + 3.f*t*t*s*P2.z + t*t*t*P3.z);
                        // keep any point close enough to reach a column in
                        // range (max radius ~7), so caves cross chunk borders
                        int lx = x + tscootx; // store points in window coords
                        int lz = z + tscootz; // for the carve pass
                        if (lx >= xlo-8 && lx < xhi+8 && y >= 0 && y <= TILESH - 1 && lz >= zlo-8 && lz < zhi+8)
                                cave_points[cave_p_len++] = QCAVE(lx, y, lz, radius_sq);
                }
        }

        // carve caves: each point hollows its little sphere, clipped to the
        // range - cost scales with cave volume, not with column count
        for (int i = 0; i < cave_p_len; i++)
        {
                struct qcave c = cave_points[i];
                int r = (int)sqrtf((float)c.radius_sq);
                int cxlo = MAX(c.x - r, xlo), cxhi = MIN(c.x + r, xhi - 1);
                int czlo = MAX(c.z - r, zlo), czhi = MIN(c.z + r, zhi - 1);
                int cylo = MAX(c.y - r, 0),   cyhi = MIN(c.y + r, TILESH - 3);
                for (int x = cxlo; x <= cxhi; x++) for (int z = czlo; z <= czhi; z++)
                        for (int y = cylo; y <= cyhi; y++)
                                if (DIST_SQ(c.x - x, c.y - y, c.z - z) <= c.radius_sq
                                                && TT_(x, y, z) != WATR)
                                        TT_(x, y, z) = OPEN;
        }
        gen_pass(&t0, GEN_CAVES);

        // set gndheight and initial lighting
        for (int x = xlo; x < xhi; x++) for (int z = zlo; z < zhi; z++)
        {
                unsigned char *t = &TT_(x, 0, z);
                unsigned char *sun = &TSUN_(x, 0, z);

                // open sky is the bulk of the column: full sun in one run
                int y = 0;
                while (y < TILESH-1 && t[y] == OPEN) y++;
                memset(sun, 15, y);

                // water and anything else above ground, block by block:
                // sunlight dims through water, and water pours down into
                // air pockets the caves carved below it
                int light_level = 15;
                int wet = false;
                for (; y < TILESH-1 && !(t[y] < LASTSOLID); y++)
                {
                        if (wet && t[y] == OPEN)
                                t[y] = WATR;
                        if (t[y] == WATR)
                        {
                                wet = true;
                                if (light_level) light_level--;
                                if (light_level) light_level--;
                        }
                        sun[y] = light_level;
                }

                if (y < TILESH-1) // found the ground
                {
                        TGNDH_(x, z) = y;
                        if (y)
                        {
                                sun[y-1] = 0;
                                // the queue works in the main thread's
                                // window mapping, which can differ mid-scoot
                                int mx = x - tscootx + scootx;
                                int mz = z - tscootz + scootz;
                                if (mx >= 0 && mx < TILESW && mz >= 0 && mz < TILESD)
                                        sun_enqueue_raw(mx, y-1, mz, light_level);
                        }
                        // the ground block and everything below start dark
                        memset(sun + y, 0, TILESH-1 - y);
                }
        }
        gen_pass(&t0, GEN_LIGHT);
        #undef HMAP
}

// pass 1: a chunk's edge columns only, so neighbors' pass 2 can read one
// column past their own border without anyone writing outside their chunk
void gen_chunk_pass1(int cx, int cz)
{
        int xlo = cx * CHUNKW, xhi = xlo + CHUNKW;
        int zlo = cz * CHUNKD, zhi = zlo + CHUNKD;
        gen_columns(xlo, xhi, zlo, zlo+1);      // north edge
        gen_columns(xlo, xhi, zhi-1, zhi);      // south edge
        gen_columns(xlo, xlo+1, zlo+1, zhi-1);  // west edge
        gen_columns(xhi-1, xhi, zlo+1, zhi-1);  // east edge
}

// pass 2: the interior, then everything that reads neighboring columns.
// requires pass 1 on this chunk and all 8 neighbors
void gen_chunk_pass2(int cx, int cz)
{
        int xlo = cx * CHUNKW, xhi = xlo + CHUNKW;
        int zlo = cz * CHUNKD, zhi = zlo + CHUNKD;

        gen_columns(xlo+1, xhi-1, zlo+1, zhi-1);

        unsigned t0 = SDL_GetTicks();

        // correcting pass over the chunk, contain floating water
        for (int x = MAX(xlo, 1); x < MIN(xhi, TILESW-1); x++)
                for (int z = MAX(zlo, 1); z < MIN(zhi, TILESD-1); z++)
                        for (int y = SEA_LEVEL + 20; y < TILESH-2; y++)
        {
                if (TT_(x, y, z) == WATR)
                {
                        if (TT_(x  , y  , z-1) == OPEN ||
                            TT_(x  , y  , z+1) == OPEN ||
                            TT_(x-1, y  , z  ) == OPEN ||
                            TT_(x+1, y  , z  ) == OPEN ||
                            TT_(x  , y+1, z  ) == OPEN)
                                TT_(x, y, z) = WOOD;
                }
        }
        gen_pass(&t0, GEN_WATER);

        // trees?
        unsigned seed = SEED2(xlo - tscootx, zlo - tscootz) ^ 0x5eed7ee5;
        float p191 = noise(zlo - tscootz, xlo - tscootx, 1300, 999, 1);
        int randp = p191 > 0.51f ? 96 : p191 > 0.45f ? 85 : 0;
        if (!tree_enable) randp = 0;
        if (randp) while (RANDP(randp))
        {
                char lowland = RANDBOOL ? RLEF : YLEF;
                int x = xlo + RANDI(5, CHUNKW - 5);
                int z = zlo + RANDI(5, CHUNKD - 5);
                for (int y = 10; y < TILESH-2; y++)
                {
                        if (TT_(x, y, z) == OPEN)
                                continue;

                        if (TT_(x, y, z) != GRAS && TT_(x, y, z) != DIRT && TT_(x, y, z) != MTGR)
                                break;

                        // spruce grows on the mountain grass, leafy trees below
                        char leaves = TT_(x, y, z) == MTGR ? SLEF : lowland;

                        int yy = y;
                        int trunklo = 5;
                        int trunkhi = (leaves == SLEF) ? 9 : 7;
                        int trunkh = RANDI(trunklo, trunkhi);
                        int leafbaselo = (leaves == SLEF) ? 1 : 2;
                        int leafbasehi = (leaves == SLEF) ? 2 : 4;
                        int tipheight = (leaves == SLEF) ? 18 : 3;
                        int radlo = (leaves == SLEF) ? 1.1f : 3.f;
                        int radhi = (leaves == SLEF) ? 4.f : 5.f;

                        float radius = RANDF(radlo, radhi);

                        for (; yy >= y - trunkh; yy--) // up to wood height
                                TT_(x, yy, z) = WOOD;

                        int ymax = yy + RANDI(leafbaselo, leafbasehi);

                        for (int i = x-3; i <= x+3; i++) for (int j = yy-tipheight; j <= ymax; j++) for (int k = z-3; k <= z+3; k++)
                        {
                                float hlax = (leaves == SLEF && j <= yy) ? .2f : 4.f;
                                float dist = (i-x) * (i-x) + (j-yy) * (j-yy) * hlax + (k-z) * (k-z);
                                if (TT_(i, j, k) == OPEN && dist < radius * radius)
                                        TT_(i, j, k) = leaves;
                        }

                        break;
                }
        }
        gen_pass(&t0, GEN_TREES);

        // edge corners read the neighbors' edge columns (pass 1 data)
        recalc_corner_lighting(xlo, MIN(xhi+1, TILESW-1), zlo, MIN(zhi+1, TILESD-1));
        gen_pass(&t0, GEN_CORNERS);
}

// update terrain worker thread(s) copies of scoot vars
void terrain_apply_scoot()
{
        #pragma omp critical
        {
                tscootx = future_scootx * CHUNKW;
                tscootz = future_scootz * CHUNKD;
                tchunk_scootx = future_scootx;
                tchunk_scootz = future_scootz;
        }
}

// ring slot of a window chunk coord - absolute-derived, so every builder
// thread agrees on it no matter which scoot mapping it is running under
#define TSLOTI(z) ((z - tchunk_scootz) & (VAOD-1))
#define TSLOTJ(x) ((x - tchunk_scootx) & (VAOW-1))

// on its own thread(s), loops forever building chunks when needed
void chunk_builder()
{ for(;;) {
        terrain_apply_scoot();

        int best_x = 0, best_z = 0;
        int px = (player[0].pos.x / BS + CHUNKW2) / CHUNKW;
        int pz = (player[0].pos.z / BS + CHUNKD2) / CHUNKD;
        px = CLAMP(px, 0, VAOW-1);
        pz = CLAMP(pz, 0, VAOD-1);

        int found = 0;
        int need[9][2], nr_need = 0; // pass 1 units this thread claimed

        #pragma omp critical (chunks)
        {
                // find nearest ungenerated chunk nobody is working on
                int best_dist = 99999999;
                for (int x = 0; x < VAOW; x++) for (int z = 0; z < VAOD; z++)
                {
                        if (TAGEN_(x, z) || chunk_claim2[TSLOTI(z)][TSLOTJ(x)])
                                continue;

                        int dist_sq = (x - px) * (x - px) + (z - pz) * (z - pz);
                        if (dist_sq < best_dist)
                        {
                                best_dist = dist_sq;
                                best_x = x;
                                best_z = z;
                        }
                }

                if (best_dist != 99999999)
                {
                        found = 1;
                        chunk_claim2[TSLOTI(best_z)][TSLOTJ(best_x)] = 1;
                        // claim the missing edges (pass 1) around it
                        for (int dz = -1; dz <= 1; dz++) for (int dx = -1; dx <= 1; dx++)
                        {
                                int cx = best_x + dx, cz = best_z + dz;
                                if (cx < 0 || cx >= VAOW || cz < 0 || cz >= VAOD) continue;
                                if (TEDGE_(cx, cz) || chunk_claim1[TSLOTI(cz)][TSLOTJ(cx)]) continue;
                                chunk_claim1[TSLOTI(cz)][TSLOTJ(cx)] = 1;
                                need[nr_need][0] = cx;
                                need[nr_need][1] = cz;
                                nr_need++;
                        }
                }
        }

        if (!found)
        {
                if (!TERRAIN_THREAD)
                        return;
                SDL_Delay(1);
                continue;
        }

        int ticks_before = SDL_GetTicks();

        for (int i = 0; i < nr_need; i++)
        {
                int cx = need[i][0], cz = need[i][1];
                gen_chunk_pass1(cx, cz);
                #pragma omp critical (chunks)
                {
                        TEDGE_SLOT(cx, cz).ax = cx - tchunk_scootx;
                        TEDGE_SLOT(cx, cz).az = cz - tchunk_scootz;
                        chunk_claim1[TSLOTI(cz)][TSLOTJ(cx)] = 0;
                }
        }

        // another builder may still be generating a neighbor's edges
        for (;;)
        {
                int ready = 1;
                #pragma omp critical (chunks)
                {
                        for (int dz = -1; dz <= 1; dz++) for (int dx = -1; dx <= 1; dx++)
                        {
                                int cx = best_x + dx, cz = best_z + dz;
                                if (cx < 0 || cx >= VAOW || cz < 0 || cz >= VAOD) continue;
                                if (!TEDGE_(cx, cz)) ready = 0;
                        }
                }
                if (ready) break;
                SDL_Delay(1);
        }

        gen_chunk_pass2(best_x, best_z);

        #pragma omp critical (chunks)
        {
                nr_chunks_generated++;
                chunk_gen_ticks += SDL_GetTicks() - ticks_before;
                TAGEN_SLOT(best_x, best_z).ax = best_x - tchunk_scootx;
                TAGEN_SLOT(best_x, best_z).az = best_z - tchunk_scootz;
                chunk_claim2[TSLOTI(best_z)][TSLOTJ(best_x)] = 0;
        }

        #pragma omp critical
        {
                just_generated[just_gen_len].x = best_x - tchunk_scootx; // absolute, in
                just_generated[just_gen_len].z = best_z - tchunk_scootz; // case of scoots
                just_gen_len++;
        }

        if (!TERRAIN_THREAD)
                return;
} }

#endif // BLOCKO_CHUNKER_C_INCLUDED

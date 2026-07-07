#include "blocko.c"
#ifndef BLOCKO_CHUNKER_C_INCLUDED
#define BLOCKO_CHUNKER_C_INCLUDED

#include "../common/tinyc.games/taylor_noise.c"
#include "../common/tinyc.games/terrain.c"

void gen_chunk(int xlo, int xhi, int zlo, int zhi)
{
        xlo = CLAMP(xlo, 0, TILESW-1);
        xhi = CLAMP(xhi, 0, TILESW-1);
        zlo = CLAMP(zlo, 0, TILESD-1);
        zhi = CLAMP(zhi, 0, TILESD-1);

        int x;
        unsigned gen_t0 = SDL_GetTicks();
        #define GEN_PASS(which) { unsigned t = SDL_GetTicks(); \
                gen_pass_ms[which] += t - gen_t0; gen_t0 = t; }

        // heights for every column plus a 1-column border - the steepness
        // test reads each neighbor's height, so compute each column once
        // here instead of 5x in the soil loop below
        static _Thread_local int hmap[CHUNKD+4][CHUNKW+4];
        #define HMAP(xx, zz) hmap[(zz)-(zlo-1)][(xx)-(xlo-1)]
        for (int z = zlo-1; z <= zhi; z++) for (int xx = xlo-1; xx <= xhi; xx++)
                HMAP(xx, z) = TILESH
                        - get_filtered_height(xx - tscootx, z - tscootz) * TERRAIN_VSCALE;

        //#pragma omp parallel for
        for (x = xlo; x < xhi; x++) for (int z = zlo; z < zhi; z++)
        {
                if (x == xlo && z == zlo)
                {
                        omp_threads = omp_get_num_threads();
                        //printf("gen_chunk threads: %d\n", omp_threads);
                }

                int ax = x - tscootx; // absolute world coords: feed these to all
                int az = z - tscootz; // noise/seeds so terrain survives scooting
                int sx = ax & (TILESW-1);
                int sz = az & (TILESD-1);

                if (col_stamp_x[sx][sz] == ax && col_stamp_z[sx][sz] == az)
                        continue;
                col_stamp_x[sx][sz] = ax;
                col_stamp_z[sx][sz] = az;

                int solid_depth = 0;
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

                #define topsoil() ((noise(ax, az, 690, 12345, 1) > .5f) ? GRAS : DIRT)

                int flo[16], fhi[16];
                int fn = form_spans(ax, az, flo, fhi, 16);
                int fdepth = 0; // consecutive formation blocks from pile top

                for (int y = 0; y < TILESH; y++)
                {
                        if (y == TILESH - 1) {
                                TT_(x, y, z) = HARD;
                                continue;
                        }

                        if (y < hmaph)
                        {
                                int fs = 0;
                                for (int i = 0; i < fn; i++)
                                        if (y >= flo[i] && y <= fhi[i]) { fs = 1; break; }
                                if (fs)
                                {
                                        // cap exposed formation tops with the
                                        // same soil bands as the surrounding
                                        // land; the core below stays stone
                                        fdepth++;
                                        if      (y < lev1 || fdepth > 3) TT_(x, y, z) = STON;
                                        else if (y < lev2) TT_(x, y, z) = DIRT;
                                        else if (y < lev3) TT_(x, y, z) = fdepth == 1 ?
                                                (y > SEA_LEVEL ? SAND : GRAS) : DIRT;
                                        else if (y < lev4) TT_(x, y, z) = SAND;
                                        else               TT_(x, y, z) = STON;
                                }
                                else
                                {
                                        fdepth = 0;
                                        TT_(x, y, z) = y > SEA_LEVEL ? WATR : OPEN;
                                }
                                continue;
                        }

                        solid_depth++;

                        if (steep)         TT_(x, y, z) = STON;
                        else if (y < lev1) TT_(x, y, z) = STON;
                        else if (y < lev2) TT_(x, y, z) = DIRT;
                        else if (y < lev3) TT_(x, y, z) = solid_depth == 1 ? (hmaph > SEA_LEVEL + 1 ? SAND : GRAS) : DIRT;
                        else if (y < lev4) TT_(x, y, z) = solid_depth <= 4 ? SAND : STON;
                        else               TT_(x, y, z) = HARD;

                        //if (TT_(x, y, z) != HARD) TT_(x, y, z) = OPEN;
                }
        }
        GEN_PASS(GEN_SOIL);

        // find nearby bezier curvy caves - curves live in absolute coords
        #define REGW (CHUNKW*16)
        #define REGD (CHUNKD*16)
        // find region          ,-- have to add 1 bc we're overdrawing chunks
        // lower bound         /   (& with power-of-2 floors correctly when negative)
        int rxlo = (xlo - tscootx + 1) & ~(REGW-1);
        int rzlo = (zlo - tscootz + 1) & ~(REGD-1);
        unsigned seed = SEED2(rxlo, rzlo);
        // find region center
        int rxcenter = rxlo + REGW/2;
        int rzcenter = rzlo + REGD/2;
        struct point PC = (struct point){rxcenter, TILESH - RANDI(1, 25), rzcenter};
        struct point P0;
        struct point P1;
        struct point P2;
        struct point P3 = PC;
        int nr_caves = cave_enable ? RANDI(0, 100) : 0;

        // cave system stretchiness
        int sx = RANDI(10, 60);
        int sy = RANDI(10, 60);
        int sz = RANDI(10, 60);

        #define MAX_CAVE_POINTS 10000
        #define QCAVE(x,y,z,radius_sq) ((struct qcave){x, y, z, radius_sq})
        struct qcave cave_points[MAX_CAVE_POINTS];
        int cave_p_len = 0;

        for (int i = 0; i < nr_caves; i++)
        {
                // random walk from center of region, or end of last curve
                P0 = RANDP(33) ? PC : P3;
                P1 = (struct point){P0.x + RANDI(-sx, sx), P0.y + RANDI(-sy, sy), P0.z + RANDI(-sz, sz)};
                P2 = (struct point){P1.x + RANDI(-sx, sx), P1.y + RANDI(-sy, sy), P1.z + RANDI(-sz, sz)};
                P3 = (struct point){P2.x + RANDI(-sx, sx), P2.y + RANDI(-sy, sy), P2.z + RANDI(-sz, sz)};

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
                        // TODO: don't store duplicate cave points?
                        int lx = x + tscootx; // store points in window coords
                        int lz = z + tscootz; // for the carve pass
                        if (lx >= xlo && lx <= xhi && y >= 0 && y <= TILESH - 1 && lz >= zlo && lz <= zhi)
                                cave_points[cave_p_len++] = QCAVE(lx, y, lz, radius_sq);
                }
        }

        // carve caves
        #pragma omp parallel for
        for (x = xlo; x < xhi; x++) for (int z = zlo; z < zhi; z++) for (int y = 0; y < TILESH-2; y++)
                for (int i = 0; i < cave_p_len; i++)
                {
                        int dist_sq = DIST_SQ(cave_points[i].x - x, cave_points[i].y - y, cave_points[i].z - z);
                        if (dist_sq <= cave_points[i].radius_sq)
                        {
                                TT_(x, y, z) = OPEN;
                                break;
                        }
                }
        GEN_PASS(GEN_CAVES);

        // correcting pass over middle, contain floating water
        //#pragma omp parallel for
        for (x = xlo+1; x < xhi-1; x++) for (int z = zlo+1; z < zhi-1; z++) for (int y = SEA_LEVEL + 20; y < TILESH-2; y++)
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
        GEN_PASS(GEN_WATER);

        // trees?
        // reseed per-chunk: the cave seed above is per-REGION, and cave gen
        // consumes an identical sequence for every chunk in the region, so
        // without this every chunk gets the same tree layout
        seed = SEED2(xlo - tscootx, zlo - tscootz) ^ 0x5eed7ee5;
        float p191 = noise(zlo - tscootz, xlo - tscootx, 1300, 999, 1);
        int randp = p191 > 0.51f ? 96 : p191 > 0.45f ? 85 : 0;
        if (randp) while (RANDP(randp))
        {
                char leaves = RANDBOOL ? RLEF : YLEF;
                float radius = RANDF(3.f, 6.f);
                int x = xlo + RANDI(5, CHUNKW - 5);
                int z = zlo + RANDI(5, CHUNKD - 5);
                for (int y = 10; y < TILESH-2; y++)
                {
                        if (TT_(x, y, z) == OPEN)
                                continue;

                        if (TT_(x, y, z) != GRAS && TT_(x, y, z) != DIRT)
                                break;

                        int yy = y;
                        for (; yy >= y - (int)RANDI(5, 7); yy--) // height
                                TT_(x, yy, z) = WOOD;

                        int ymax = yy + RANDI(2, 4);

                        for (int i = x-3; i <= x+3; i++) for (int j = yy-3; j <= ymax; j++) for (int k = z-3; k <= z+3; k++)
                        {
                                float dist = (i-x) * (i-x) + (j-yy) * (j-yy) * 4.f + (k-z) * (k-z);
                                if (TT_(i, j, k) == OPEN && dist < radius * radius)
                                        TT_(i, j, k) = leaves;
                        }

                        break;
                }
        }

        GEN_PASS(GEN_TREES);

        // cleanup gndheight and set initial lighting
        //#pragma omp parallel for
        for (x = xlo+1; x < xhi-1; x++) for (int z = zlo+1; z < zhi-1; z++)
        {
                int above_ground = true;
                int light_level = 15;
                int wet = false;

                for (int y = 0; y < TILESH-1; y++)
                {
                        if (above_ground && TIS_OPAQUE(x, y, z))
                        {
                                TGNDH_(x, z) = y;
                                above_ground = false;
                                if (y)
                                {
                                        TSUN_(x, y-1, z) = 0;
                                        // sun_enqueue works in the main thread's
                                        // window mapping, which can differ mid-scoot
                                        int mx = x - tscootx + scootx;
                                        int mz = z - tscootz + scootz;
                                        if (mx >= 0 && mx < TILESW && mz >= 0 && mz < TILESD)
                                                sun_enqueue(mx, y-1, mz, 0, light_level);
                                }
                                light_level = 0;
                        }

                        if (wet && TT_(x, y, z) == OPEN)
                                TT_(x, y, z) = WATR;

                        if (wet && TIS_SOLID(x, y, z))
                                wet = false;

                        if (TT_(x, y, z) == WATR)
                        {
                                wet = true;
                                if (light_level) light_level--;
                                if (light_level) light_level--;
                        }

                        TSUN_(x, y, z) = light_level;
                }
        }
        GEN_PASS(GEN_LIGHT);

        recalc_corner_lighting(xlo, xhi, zlo, zhi);
        GEN_PASS(GEN_CORNERS);
        #undef GEN_PASS
        #undef HMAP
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

// on its own thread, loops forever building chunks when needed
void chunk_builder()
{ for(;;) {
        terrain_apply_scoot();

        int best_x = 0, best_z = 0;
        int px = (player[0].pos.x / BS + CHUNKW2) / CHUNKW;
        int pz = (player[0].pos.z / BS + CHUNKD2) / CHUNKD;
        px = CLAMP(px, 0, VAOW-1);
        pz = CLAMP(pz, 0, VAOD-1);

        // find nearest ungenerated chunk
        int best_dist = 99999999;
        for (int x = 0; x < VAOW; x++) for (int z = 0; z < VAOD; z++)
        {
                if (TAGEN_(x, z)) continue;

                int dist_sq = (x - px) * (x - px) + (z - pz) * (z - pz);
                if (dist_sq < best_dist)
                {
                        best_dist = dist_sq;
                        best_x = x;
                        best_z = z;
                }
        }

        if (best_dist == 99999999)
        {
                if (!TERRAIN_THREAD)
                        return;
                SDL_Delay(1);
                continue;
        }

        int xlo = best_x * CHUNKW;
        int zlo = best_z * CHUNKD;
        int xhi = xlo + CHUNKW;
        int zhi = zlo + CHUNKD;

        int ticks_before = SDL_GetTicks();
        gen_chunk(xlo-1, xhi+1, zlo-1, zhi+1);
        nr_chunks_generated++;
        chunk_gen_ticks += SDL_GetTicks() - ticks_before;

        TAGEN_SLOT(best_x, best_z).ax = best_x - tchunk_scootx;
        TAGEN_SLOT(best_x, best_z).az = best_z - tchunk_scootz;

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

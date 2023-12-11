#include "blocko.h"
#include "../common/tinyc.games/taylor_noise.c"
#include "../common/tinyc.games/terrain.c"

int tscootx, tscootz, tchunk_scootx, tchunk_scootz;

#define THMAP(x,z) (hmap2[((x)-tscootx)%TILESW][((z)-tscootz)%TILESD])

void gen_chunk(int xlo, int xhi, int zlo, int zhi)
{
        xlo = CLAMP(xlo, 0, TILESW-1);
        xhi = CLAMP(xhi, 0, TILESW-1);
        zlo = CLAMP(zlo, 0, TILESD-1);
        zhi = CLAMP(zhi, 0, TILESD-1);

        static char column_already_generated[TILESW][TILESD];
        int x;

        //#pragma omp parallel for
        for (x = xlo; x < xhi; x++) for (int z = zlo; z < zhi; z++)
        {
                if (x == xlo && z == zlo)
                {
                        omp_threads = omp_get_num_threads();
                        //printf("gen_chunk threads: %d\n", omp_threads);
                }

                if (column_already_generated[(x-tscootx) & (TILESW-1)][(z-tscootz) & (TILESD-1)])
                        continue;
                column_already_generated[(x-tscootx) & (TILESW-1)][(z-tscootz) & (TILESD-1)] = true;

                int solid_depth = 0;
                int hmaph = (1.f - get_filtered_height(x, z)) * TILESH;

                for (int y = 0; y < TILESH; y++)
                {
                        if (y == TILESH - 1) {
                                TT_(x, y, z) = HARD;
                                continue;
                        }

                        if (y < hmaph)
                        {
                                TT_(x, y, z) = y > 80 ? WATR : OPEN;
                                continue;
                        }

                        solid_depth++;

                             if (y < 50)   TT_(x, y, z) = GRAN;
                        else if (y < 64)   TT_(x, y, z) = STON;
                        else if (y < 77)   TT_(x, y, z) = solid_depth == 1 ? GRAS : DIRT;
                        else if (y < 115)  TT_(x, y, z) = solid_depth == 1 ? SAND : STON;
                        else               TT_(x, y, z) = HARD;
                }
        }

        // find nearby bezier curvy caves
        #define REGW (CHUNKW*16)
        #define REGD (CHUNKD*16)
        // find region          ,-- have to add 1 bc we're overdrawing chunks
        // lower bound         /
        int rxlo = (int)((xlo-tscootx+1) / REGW) * REGW;
        int rzlo = (int)((zlo-tscootz+1) / REGD) * REGD;
        unsigned seed = SEED2(rxlo, rzlo);
        rxlo = (int)((xlo+1) / REGW) * REGW; // now without scooting
        rzlo = (int)((zlo+1) / REGD) * REGD;
        // find region center
        int rxcenter = rxlo + REGW/2;
        int rzcenter = rzlo + REGD/2;
        struct point PC = (struct point){rxcenter, TILESH - RANDI(1, 25), rzcenter};
        struct point P0;
        struct point P1;
        struct point P2;
        struct point P3 = PC;
        int nr_caves = RANDI(0, 100);

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
                        if (x >= xlo && x <= xhi && y >= 0 && y <= TILESD - 1 && z >= zlo && z <= zhi)
                                cave_points[cave_p_len++] = QCAVE(x, y, z, radius_sq);
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

        // correcting pass over middle, contain floating water
        #pragma omp parallel for
        for (x = xlo+1; x < xhi-1; x++) for (int z = zlo+1; z < zhi-1; z++) for (int y = 100; y < TILESH-2; y++)
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

        // trees?
        float p191 = noise(zlo, xlo, 191, 999, 1);
        seed = SEED2(xlo - tscootx, zlo - tscootz);
        if (p191 > 0.6f) while (RANDP(95))
        {
                char leaves = RANDBOOL ? RLEF : YLEF;
                float radius = RANDF(1.f, 4.f);
                int x = xlo + CHUNKW/2 + RANDI(-5, 5);
                int z = zlo + CHUNKD/2 + RANDI(-5, 5);
                for (int y = 10; y < TILESH-2; y++)
                {
                        if (TT_(x, y, z) == OPEN)
                                continue;

                        if (TT_(x, y, z) != GRAS && TT_(x, y, z) != DIRT)
                                break;

                        int yy = y;
                        for (; yy >= y - (int)RANDI(3, 8); yy--)
                                TT_(x, yy, z) = WOOD;

                        int ymax = yy + RANDI(2, 4);

                        for (int i = x-3; i <= x+3; i++) for (int j = yy-3; j <= ymax; j++) for (int k = z-3; k <= z+3; k++)
                        {
                                float dist = (i-x) * (i-x) + (j-yy) * (j-yy) + (k-z) * (k-z);
                                if (TT_(i, j, k) == OPEN && dist < radius * radius)
                                        TT_(i, j, k) = leaves;
                        }

                        break;
                }
        }

        // cleanup gndheight and set initial lighting
        #pragma omp parallel for
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
                                        sun_enqueue(x, y-1, z, 0, light_level);
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

        recalc_corner_lighting(xlo, xhi, zlo, zhi);
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

        TAGEN_(best_x, best_z) = true;

        #pragma omp critical
        {
                just_generated[just_gen_len].x = best_x;
                just_generated[just_gen_len].z = best_z;
                just_gen_len++;
        }

        if (!TERRAIN_THREAD)
                return;
} }


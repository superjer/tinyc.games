#include "blocko.h"

float hmap[TILESW][TILESD];
float hmap2[TILESW][TILESD];

int tscootx, tscootz, tchunk_scootx, tchunk_scootz;

#define THMAP(x,z) (hmap2[((x)-tscootx)%TILESW][((z)-tscootz)%TILESD])

void gen_hmap(int x0, int x2, int z0, int z2)
{
        unsigned seed = SEED4(x0, x2, z0, z2);

        // pick corners if they aren't set
        if (hmap[x0][z0] == 0) hmap[x0][z0] = RANDI(64, 127);
        if (hmap[x0][z2] == 0) hmap[x0][z2] = RANDI(64, 127);
        if (hmap[x2][z0] == 0) hmap[x2][z0] = RANDI(64, 127);
        if (hmap[x2][z2] == 0) hmap[x2][z2] = RANDI(64, 127);

        int x1 = (x0 + x2) / 2;
        int z1 = (z0 + z2) / 2;
        int w = (x2 - x0) / 4;
        int d = (z2 - z0) / 4;
        w = w ? w : 1;
        d = d ? d : 1;
        float d2 = d / 2.f;
        float r = w > 2 ? 1.f : 0.f;

        // edges middles
        if (!hmap[x0][z1])
                hmap[x0][z1] = (hmap[x0][z0] + hmap[x0][z2]) / 2.f + r * RANDF(-d2, d2);
        if (!hmap[x2][z1])
                hmap[x2][z1] = (hmap[x2][z0] + hmap[x2][z2]) / 2.f + r * RANDF(-d2, d2);
        if (!hmap[x1][z0])
                hmap[x1][z0] = (hmap[x0][z0] + hmap[x2][z0]) / 2.f + r * RANDF(-d2, d2);
        if (!hmap[x1][z2])
                hmap[x1][z2] = (hmap[x0][z2] + hmap[x2][z2]) / 2.f + r * RANDF(-d2, d2);

        // middle middle
        hmap[x1][z1] = (hmap[x0][z1] + hmap[x2][z1] + hmap[x1][z0] + hmap[x1][z2]) / 4.f + r * RANDF(-d, d);

        // recurse if there are any unfilled spots
        if(x1 - x0 > 1 || x2 - x1 > 1 || z1 - z0 > 1 || z2 - z1 > 1)
        {
                gen_hmap(x0, x1, z0, z1);
                gen_hmap(x0, x1, z1, z2);
                gen_hmap(x1, x2, z0, z1);
                gen_hmap(x1, x2, z1, z2);
        }
}

void smooth_hmap()
{
        for (int x = 0; x < TILESW; x++) for (int z = 0; z < TILESD; z++)
        {
                float p365 = noise(x, 0, -z, 365);
                int radius = p365 < 0.0f ? 3 :
                             p365 < 0.2f ? 2 : 1;
                int x0 = x - radius;
                int x1 = x + radius + 1;
                int z0 = z - radius;
                int z1 = z + radius + 1;
                CLAMP(x0, 0, TILESW-1);
                CLAMP(x1, 0, TILESW-1);
                CLAMP(z0, 0, TILESD-1);
                CLAMP(z1, 0, TILESD-1);
                int sum = 0, n = 0;
                for (int i = x0; i < x1; i++) for (int j = z0; j < z1; j++)
                {
                        sum += hmap[i][j];
                        n++;
                }
                int res = sum / n;

                float p800 = noise(x, 0, z, 800);
                float p777 = noise(z, 0, x, 777);
                float p301 = noise(x, 0, z, 301);
                float p204 = noise(x, 0, z, 204);
                float p33 = noise(x, 0, z, 32 * (1.1 + p301));
                float swoosh = p33 > 0.3 ? (10 - 30 * (p33 - 0.3)) : 0;

                float times = (p204 * 20.f) + 30.f;
                float plus = (-p204 * 40.f) + 60.f;
                CLAMP(times, 20.f, 40.f);
                CLAMP(plus, 40.f, 80.f);
                int beach_ht = (1.f - p777) * times + plus;
                CLAMP(beach_ht, 90, 100);

                if (res > beach_ht) // beaches
                {
                        if (res > beach_ht + 21) res -= 18;
                        else res = ((res - beach_ht) / 7) + beach_ht;
                }

                float s = (1 + p204) * 0.2;
                if (p800 > 0.0 + s)
                {
                        float t = (p800 - 0.0 - s) * 10;
                        CLAMP(t, 0.f, 1.f);
                        res = lerp(t, res, 102);
                        if (res == 102 && swoosh) res = 101;
                }

                hmap2[x][z] = res < TILESH - 1 ? res : TILESH - 1;
        }
}

void create_hmap()
{
        // generate in pieces
        for (int i = 0; i < 8; i++) for (int j = 0; j < 8; j++)
        {
                int x0 = (i  ) * TILESW / 8;
                int x1 = (i+1) * TILESW / 8;
                int z0 = (j  ) * TILESD / 8;
                int z1 = (j+1) * TILESD / 8;
                CLAMP(x1, 0, TILESW-1);
                CLAMP(z1, 0, TILESD-1);
                gen_hmap(x0, x1, z0 , z1);
        }

        smooth_hmap();
}

void gen_chunk(int xlo, int xhi, int zlo, int zhi)
{
        CLAMP(xlo, 0, TILESW-1);
        CLAMP(xhi, 0, TILESW-1);
        CLAMP(zlo, 0, TILESD-1);
        CLAMP(zhi, 0, TILESD-1);

        static char column_already_generated[TILESW][TILESD];
        int x;

        #pragma omp parallel for
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

                float p1080 = noise(x, 0, -z, 1080);
                float p530 = noise(z, 0, x, 530);
                float p630 = noise(-z, 0, x, 629);
                float p200 = noise(x, 0, z, 200);
                float p80 = noise(x, 0, z, 80);
                float p15 = noise(z, 0, -x, 15);
                //float p5 = noise(-x, 0, z, 5);

                if (p200 > 0.2f)
                {
                        float flatten = (p200 - 0.2f) * 80;
                        CLAMP(flatten, 1, 12);
                        THMAP(x,z) -= 100;
                        THMAP(x,z) /= flatten;
                        THMAP(x,z) += 100;
                }

                int solid_depth = 0;
                int slicey_bit = false;
                int plateau_bit = false;
                int mode = p1080 > 0 ? 1 : 10;

                for (int y = 0; y < TILESH; y++)
                {
                        if (y == TILESH - 1) { TT_(x, y, z) = HARD; continue; }

                        float p300 = noise(x, y, z, 300);
                        float p32 = noise(x, y*mode, z, 16 + 16 * (1.1 + p300));
                        float plat = p32 > 0.3 ? (10 - 30 * (p32 * p32 * p32 - 0.3)) : 0;

                        float p90 = noise(x, y, z, 90);
                        float p91 = noise(x+1000, y+1000, z+1000, 91);
                        float p42 = noise(x, y*(p300 + 1), z, 42);
                        float p9  = noise(x, y*0.05, z, 9);
                        float p2  = noise(-z, y, x, 2);

                        if (p300 + fabsf(p80) * 0.25 + p15 * 0.125 < -0.5) { plat = -plat; }
                        else if (p300 < 0.5) { plat = 0; }

                        int cave = (p90 < -0.24 || p91 < -0.24) && (p42 > 0.5 && p9 < 0.4);

                        if (y > THMAP(x,z) - ((p80 + 1) * 20) && p90 > 0.4 && p91 > 0.4 && p42 > 0.01 && p42 < 0.09 && p300 > 0.3)
                                slicey_bit = true;

                        int platted = y < THMAP(x,z) + plat * (mode * 0.125f + 0.875f);

                        if ((cave || platted) && !plateau_bit)
                        {
                                unsigned seed = SEED2(x - tscootx, z - tscootz);
                                if (!slicey_bit || RANDP(5))
                                {
                                        int type = (y > 100 && THMAP(x,z) > 99) ? WATR : OPEN; //only allow water below low heightmap
                                        TT_(x, y, z) = type;
                                        solid_depth = 0;
                                        slicey_bit = false;
                                        goto out;
                                }
                        }
                        else
                        {
                                if (mode == 10 && plat && !cave && y < THMAP(x,z))
                                        plateau_bit = true;
                                slicey_bit = false;
                        }

                        solid_depth++;
                        float p16 = noise(x, y, z, 16);
                        int slv = 76 + p530 * 20;
                        int dlv = 86 + p630 * 20;
                        int ore  =  p2 > 0.4f ? ORE : OREH;
                        int ston = p42 > 0.4f && p9 < -0.3f ? ore : STON;

                        if      (slicey_bit)          TT_(x, y, z) = p9 > 0.4f ? HARD : SAND;
                        else if (solid_depth > 14 + 5 * p9) TT_(x, y, z) = GRAN;
                        else if (y < slv - 5 * p16)   TT_(x, y, z) = ston;
                        else if (y < dlv - 5 * p16)   TT_(x, y, z) = p80 > (-solid_depth * 0.1f) ? DIRT : OPEN; // erosion
                        else if (y < 100 - 5 * p16)   TT_(x, y, z) = solid_depth == 1 ? GRAS : DIRT;
                        else if (y < 120          )   TT_(x, y, z) = solid_depth < 4 + 5 * p9 ? SAND : ston;
                        else                          TT_(x, y, z) = HARD;

                        out: ;
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
                        CLAMP(radius_sq, 1.f, 50.f);

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
        float p191 = noise(zlo, 0, xlo, 191);
        seed = SEED2(xlo - tscootx, zlo - tscootz);
        if (p191 > 0.2f) while (RANDP(95))
        {
                char leaves = RANDBOOL ? RLEF : YLEF;
                float radius = RANDF(3.f, 6.f);
                int x = xlo + CHUNKW/2 + RANDI(-5, 5);
                int z = zlo + CHUNKD/2 + RANDI(-5, 5);
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
        CLAMP(px, 0, VAOW-1);
        CLAMP(pz, 0, VAOD-1);

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


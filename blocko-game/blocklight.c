#include "blocko.c"
#ifndef BLOCKO_BLOCKLIGHT_C_INCLUDED
#define BLOCKO_BLOCKLIGHT_C_INCLUDED

void sun_enqueue(int x, int y, int z, int base, unsigned char incoming_light)
{
        if (incoming_light == 0)
                return;

        if (T_(x, y, z) == WATR)
                incoming_light--; // water blocks more light

        if (T_(x, y, z) == RLEF || T_(x, y, z) == YLEF || T_(x, y, z) == SLEF)
        {
                incoming_light--; // leaves block more light
                if (incoming_light) incoming_light--;
        }

        if (SUN_(x, y, z) >= incoming_light)
                return; // already brighter

        if (T_(x, y, z) < OPEN)
                return; // no lighting for solid blocks

        set_sunlight(x, y, z, incoming_light);

        // builder threads and the main thread all enqueue here
        #pragma omp critical (sunq)
        {
                if (sq_next_len >= SUNQLEN)
                {
                        sunq_outta_room++; // out of room in sun queue
                }
                else
                {
                        int queued = 0;
                        for (size_t i = base; i < sq_curr_len && !queued; i++)
                                if (sunq_curr[i].x == x && sunq_curr[i].y == y && sunq_curr[i].z == z)
                                        queued = 1;
                        for (size_t i = 0; i < sq_next_len && !queued; i++)
                                if (sunq_next[i].x == x && sunq_next[i].y == y && sunq_next[i].z == z)
                                        queued = 1;
                        if (!queued)
                        {
                                sunq_next[sq_next_len].x = x;
                                sunq_next[sq_next_len].y = y;
                                sunq_next[sq_next_len].z = z;
                                sq_next_len++;
                        }
                }
        }
}

// gen-time variant: same light rules, but skips the duplicate scan - the
// chunk builders enqueue each column once, and a stray dupe just re-floods
// one block. keeps the lock hold tiny so builder threads don't fight
void sun_enqueue_raw(int x, int y, int z, unsigned char incoming_light)
{
        if (incoming_light == 0)
                return;

        if (T_(x, y, z) == WATR)
                incoming_light--; // water blocks more light

        if (T_(x, y, z) == RLEF || T_(x, y, z) == YLEF || T_(x, y, z) == SLEF)
        {
                incoming_light--; // leaves block more light
                if (incoming_light) incoming_light--;
        }

        if (SUN_(x, y, z) >= incoming_light)
                return; // already brighter

        if (T_(x, y, z) < OPEN)
                return; // no lighting for solid blocks

        set_sunlight(x, y, z, incoming_light);

        #pragma omp critical (sunq)
        {
                if (sq_next_len >= SUNQLEN)
                        sunq_outta_room++;
                else
                {
                        sunq_next[sq_next_len].x = x;
                        sunq_next[sq_next_len].y = y;
                        sunq_next[sq_next_len].z = z;
                        sq_next_len++;
                }
        }
}

void glo_enqueue(int x, int y, int z, int base, unsigned char incoming_light)
{
        if (incoming_light == 0)
                return;

        if (T_(x, y, z) == WATR)
                incoming_light--; // water blocks more light

        if (T_(x, y, z) == RLEF || T_(x, y, z) == YLEF || T_(x, y, z) == SLEF)
        {
                incoming_light--; // leaves block more light
                if (incoming_light) incoming_light--;
        }

        if (GLO_(x, y, z) >= incoming_light)
                return; // already brighter

        if (T_(x, y, z) < OPEN)
                return; // no lighting for solid blocks

        set_glolight(x, y, z, incoming_light);

        if (gq_next_len >= GLOQLEN)
        {
                gloq_outta_room++;
                return; // out of room in glo queue
        }

        for (size_t i = base; i < gq_curr_len; i++)
                if (gloq_curr[i].x == x && gloq_curr[i].y == y && gloq_curr[i].z == z)
                        return; // already queued in current queue

        for (size_t i = 0; i < gq_next_len; i++)
                if (gloq_next[i].x == x && gloq_next[i].y == y && gloq_next[i].z == z)
                        return; // already queued in next queue

        gloq_next[gq_next_len].x = x;
        gloq_next[gq_next_len].y = y;
        gloq_next[gq_next_len].z = z;
        gq_next_len++;
}

int step_sunlight()
{
        // swap the queues
        #pragma omp critical (sunq)
        {
                sunq_curr = sunq_next;
                sq_curr_len = sq_next_len;
                sq_next_len = 0;
                sunq_next = (sunq_curr == sunq0_) ? sunq1_ : sunq0_;
        }

        for (size_t i = 0; i < sq_curr_len; i++)
        {
                int x = sunq_curr[i].x;
                int y = sunq_curr[i].y;
                int z = sunq_curr[i].z;
                char pass_on = SUN_(x, y, z);
                if (pass_on) pass_on--; else continue;
                if (x           ) sun_enqueue(x-1, y  , z  , i+1, pass_on);
                if (x < TILESW-1) sun_enqueue(x+1, y  , z  , i+1, pass_on);
                if (y           ) sun_enqueue(x  , y-1, z  , i+1, pass_on);
                if (y < TILESH-1) sun_enqueue(x  , y+1, z  , i+1, pass_on);
                if (z           ) sun_enqueue(x  , y  , z-1, i+1, pass_on);
                if (z < TILESD-1) sun_enqueue(x  , y  , z+1, i+1, pass_on);
        }

        return sq_curr_len;
}

int step_glolight()
{
        // swap the queues
        gloq_curr = gloq_next;
        gq_curr_len = gq_next_len;
        gq_next_len = 0;
        gloq_next = (gloq_curr == gloq0_) ? gloq1_ : gloq0_;

        for (size_t i = 0; i < gq_curr_len; i++)
        {
                int x = gloq_curr[i].x;
                int y = gloq_curr[i].y;
                int z = gloq_curr[i].z;
                char pass_on = GLO_(x, y, z);
                if (pass_on) pass_on--; else continue;
                if (x           ) glo_enqueue(x-1, y  , z  , i+1, pass_on);
                if (x < TILESW-1) glo_enqueue(x+1, y  , z  , i+1, pass_on);
                if (y           ) glo_enqueue(x  , y-1, z  , i+1, pass_on);
                if (y < TILESH-1) glo_enqueue(x  , y+1, z  , i+1, pass_on);
                if (z           ) glo_enqueue(x  , y  , z-1, i+1, pass_on);
                if (z < TILESD-1) glo_enqueue(x  , y  , z+1, i+1, pass_on);
        }

        return gq_curr_len;
}

// one corner's sun brightness from the 8 blocks that meet there. the flat
// sum darkens corners crowded by solid (unlit) blocks - free ambient
// occlusion. gen and edits must agree or edited terrain shows seams
static inline float sun_corner(int a, int b, int c, int d,
                               int a2, int b2, int c2, int d2)
{
        return 0.008f * (a + b + c + d + a2 + b2 + c2 + d2);
}

// runs on the terrain thread - coords are in the terrain thread's window
// mapping, so only the T-variant macros are safe here (a scoot can land
// mid-generation, making scootx and tscootx briefly disagree)
void recalc_corner_lighting(int xlo, int xhi, int zlo, int zhi)
{
        for (int x = xlo; x < xhi; x++) for (int z = zlo; z < zhi; z++)
        {
                int x_ = (x == 0) ? 0 : x - 1;
                int z_ = (z == 0) ? 0 : z - 1;

                // gen-time sunlight is zero below the ground line of all
                // four columns meeting at this corner: compute down to just
                // past the deepest one and zero the rest (the ring slot
                // holds the previous occupant's values). Light flooding
                // into caves later updates corners as it spreads.
                int g = TGNDH_(x, z), g2;
                g2 = TGNDH_(x_, z ); if (g2 > g) g = g2;
                g2 = TGNDH_(x , z_); if (g2 > g) g = g2;
                g2 = TGNDH_(x_, z_); if (g2 > g) g = g2;
                int ylim = g + 2;
                if (ylim > TILESH) ylim = TILESH;

                // hoist the ring mapping out of the y loop: every array
                // here stores columns contiguously in y
                float *corn = &TCORN_(x, 0, z);
                float *korn = &TKORN_(x, 0, z);
                unsigned char *sa = &TSUN_(x_, 0, z_), *sb = &TSUN_(x, 0, z_),
                              *sc = &TSUN_(x_, 0, z ), *sd = &TSUN_(x, 0, z );
                unsigned char *ga = &TGLO_(x_, 0, z_), *gb = &TGLO_(x, 0, z_),
                              *gc = &TGLO_(x_, 0, z ), *gd = &TGLO_(x, 0, z );

                int a = sa[0], b = sb[0], c = sc[0], d = sd[0];       // y-1 row
                int ka = ga[0], kb = gb[0], kc = gc[0], kd = gd[0];
                for (int y = 0; y < ylim; y++)
                {
                        int a2 = sa[y], b2 = sb[y], c2 = sc[y], d2 = sd[y];
                        corn[y] = sun_corner(a, b, c, d, a2, b2, c2, d2);
                        int ka2 = ga[y], kb2 = gb[y], kc2 = gc[y], kd2 = gd[y];
                        korn[y] = 0.008f * (ka + kb + kc + kd + ka2 + kb2 + kc2 + kd2);
                        a = a2; b = b2; c = c2; d = d2;
                        ka = ka2; kb = kb2; kc = kc2; kd = kd2;
                }
                if (ylim < TILESH)
                {
                        memset(corn + ylim, 0, (TILESH - ylim) * sizeof *corn);
                        memset(korn + ylim, 0, (TILESH - ylim) * sizeof *korn);
                }
        }
}

void set_sunlight(int xlo, int ylo, int zlo, int light)
{
        SUN_(xlo, ylo, zlo) = light;

        // Mark all chunks that could be affected by corner lighting update.
        // Soft mark: remeshing can wait until the light here stops changing.
        // The +1 corners past the window's high edge have no ring slot (the
        // mask would wrap them onto the opposite edge), so stop short there
        int xhi = MIN(xlo + 2, TILESW), zhi = MIN(zlo + 2, TILESD), yhi = MIN(ylo + 2, TILESH);
        DIRTY_LIGHT(B2C(xlo), B2C(zlo));
        DIRTY_LIGHT(B2C(xhi-1), B2C(zlo));
        DIRTY_LIGHT(B2C(xlo), B2C(zhi-1));
        DIRTY_LIGHT(B2C(xhi-1), B2C(zhi-1));

        for (int x = xlo; x < xhi; x++) for (int z = zlo; z < zhi; z++) for (int y = ylo; y < yhi; y++)
        {
                int x_ = (x == 0) ? 0 : x - 1;
                int y_ = (y == 0) ? 0 : y - 1;
                int z_ = (z == 0) ? 0 : z - 1;

                CORN_(x, y, z) = sun_corner(
                                SUN_(x_, y_, z_), SUN_(x , y_, z_), SUN_(x_, y , z_), SUN_(x , y , z_),
                                SUN_(x_, y_, z ), SUN_(x , y_, z ), SUN_(x_, y , z ), SUN_(x , y , z ));
        }
}

void set_glolight(int xlo, int ylo, int zlo, int light)
{
        GLO_(xlo, ylo, zlo) = light;

        // Mark all chunks that could be affected by corner lighting update.
        // Soft mark: remeshing can wait until the light here stops changing.
        // As in set_sunlight, skip the corners past the window's high edge
        int xhi = MIN(xlo + 2, TILESW), zhi = MIN(zlo + 2, TILESD), yhi = MIN(ylo + 2, TILESH);
        DIRTY_LIGHT(B2C(xlo), B2C(zlo));
        DIRTY_LIGHT(B2C(xhi-1), B2C(zlo));
        DIRTY_LIGHT(B2C(xlo), B2C(zhi-1));
        DIRTY_LIGHT(B2C(xhi-1), B2C(zhi-1));

        for (int x = xlo; x < xhi; x++) for (int z = zlo; z < zhi; z++) for (int y = ylo; y < yhi; y++)
        {
                int x_ = (x == 0) ? 0 : x - 1;
                int y_ = (y == 0) ? 0 : y - 1;
                int z_ = (z == 0) ? 0 : z - 1;

                KORN_(x, y, z) = 0.008f * (
                                GLO_(x_, y_, z_) + GLO_(x , y_, z_) + GLO_(x_, y , z_) + GLO_(x , y , z_) +
                                GLO_(x_, y_, z ) + GLO_(x , y_, z ) + GLO_(x_, y , z ) + GLO_(x , y , z ));
        }
}

// append to the flood queue without the brighter-than check: light here is
// already correct, it just needs to spread into freshly darkened cells
static void sun_requeue(int x, int y, int z)
{
        #pragma omp critical (sunq)
        {
                if (sq_next_len >= SUNQLEN)
                        sunq_outta_room++;
                else
                {
                        sunq_next[sq_next_len].x = x;
                        sunq_next[sq_next_len].y = y;
                        sunq_next[sq_next_len].z = z;
                        sq_next_len++;
                }
        }
}

static void glo_requeue(int x, int y, int z)
{
        if (gq_next_len >= GLOQLEN)
        {
                gloq_outta_room++;
                return;
        }
        gloq_next[gq_next_len].x = x;
        gloq_next[gq_next_len].y = y;
        gloq_next[gq_next_len].z = z;
        gq_next_len++;
}

// unlight BFS queue, shared by sun and glo removal (main thread only).
// each entry remembers the light the cell had before it was darkened
struct unlit { int x, y, z, old; };
#define UNLQLEN 32768
static struct unlit unlq[UNLQLEN];

// remove direct or indirect sunlight - two-phase BFS: darken every cell
// whose light could have flowed through the changed block, and requeue the
// brighter frontier so the normal flood refills anything over-darkened.
// before calling, set opacity, but not light value
void remove_sunlight(int px, int py, int pz)
{
        // FIXME: remove this when gndheight is already correct
        for (int y = 0; y < TILESH-1; y++)
                if (IS_OPAQUE(px, y, pz))
                {
                        GNDH_(px, pz) = y;
                        break;
                }

        // i am in direct sunlight, no need to remove light
        if (ABOVE_GROUND(px, py, pz))
                return;

        if (SUN_(px, py, pz) < 1) return;

        unlq[0] = (struct unlit){px, py, pz, SUN_(px, py, pz)};
        set_sunlight(px, py, pz, 0);
        size_t head = 0, len = 1;

        while (head < len)
        {
                struct unlit u = unlq[head++];
                struct qitem nb[6];
                int nn = 0;
                if (u.x > 0       ) nb[nn++] = QITEM(u.x-1, u.y  , u.z  );
                if (u.x < TILESW-1) nb[nn++] = QITEM(u.x+1, u.y  , u.z  );
                if (u.y > 0       ) nb[nn++] = QITEM(u.x  , u.y-1, u.z  );
                if (u.y < TILESH-1) nb[nn++] = QITEM(u.x  , u.y+1, u.z  );
                if (u.z > 0       ) nb[nn++] = QITEM(u.x  , u.y  , u.z-1);
                if (u.z < TILESD-1) nb[nn++] = QITEM(u.x  , u.y  , u.z+1);

                for (int i = 0; i < nn; i++)
                {
                        int x = nb[i].x, y = nb[i].y, z = nb[i].z;
                        int L = SUN_(x, y, z);
                        if (L < 1 || IS_OPAQUE(x, y, z))
                                continue;

                        if (L < u.old && !ABOVE_GROUND(x, y, z))
                        {
                                // dimmer than the darkened cell was: its light
                                // may have flowed through there. take it down
                                if (len >= UNLQLEN) { sunq_outta_room++; continue; }
                                unlq[len++] = (struct unlit){x, y, z, L};
                                set_sunlight(x, y, z, 0);
                        }
                        else
                        {
                                // independently lit - respread from here
                                sun_requeue(x, y, z);
                        }
                }
        }
}

void remove_glolight(int px, int py, int pz)
{
        if (GLO_(px, py, pz) < 1) return;

        unlq[0] = (struct unlit){px, py, pz, GLO_(px, py, pz)};
        set_glolight(px, py, pz, 0);
        size_t head = 0, len = 1;

        while (head < len)
        {
                struct unlit u = unlq[head++];
                struct qitem nb[6];
                int nn = 0;
                if (u.x > 0       ) nb[nn++] = QITEM(u.x-1, u.y  , u.z  );
                if (u.x < TILESW-1) nb[nn++] = QITEM(u.x+1, u.y  , u.z  );
                if (u.y > 0       ) nb[nn++] = QITEM(u.x  , u.y-1, u.z  );
                if (u.y < TILESH-1) nb[nn++] = QITEM(u.x  , u.y+1, u.z  );
                if (u.z > 0       ) nb[nn++] = QITEM(u.x  , u.y  , u.z-1);
                if (u.z < TILESD-1) nb[nn++] = QITEM(u.x  , u.y  , u.z+1);

                for (int i = 0; i < nn; i++)
                {
                        int x = nb[i].x, y = nb[i].y, z = nb[i].z;
                        int L = GLO_(x, y, z);
                        if (L < 1 || IS_OPAQUE(x, y, z))
                                continue;

                        if (L < u.old)
                        {
                                if (len >= UNLQLEN) { gloq_outta_room++; continue; }
                                unlq[len++] = (struct unlit){x, y, z, L};
                                set_glolight(x, y, z, 0);
                        }
                        else
                        {
                                // independently lit - respread from here
                                glo_requeue(x, y, z);
                        }
                }
        }
}

#endif // BLOCKO_BLOCKLIGHT_C_INCLUDED

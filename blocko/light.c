#include "blocko.h"

void sun_enqueue(int x, int y, int z, int base, unsigned char incoming_light)
{
        if (incoming_light == 0)
                return;

        if (T_(x, y, z) == WATR)
                incoming_light--; // water blocks more light

        if (T_(x, y, z) == RLEF || T_(x, y, z) == YLEF)
        {
                incoming_light--; // leaves block more light
                if (incoming_light) incoming_light--;
        }

        if (SUN_(x, y, z) >= incoming_light)
                return; // already brighter

        if (T_(x, y, z) < OPEN)
                return; // no lighting for solid blocks

        SUN_(x, y, z) = incoming_light;

        if (sq_next_len >= SUNQLEN)
        {
                sunq_outta_room++;
                return; // out of room in sun queue
        }

        for (size_t i = base; i < sq_curr_len; i++)
                if (sunq_curr[i].x == x && sunq_curr[i].y == y && sunq_curr[i].z == z)
                        return; // already queued in current queue

        for (size_t i = 0; i < sq_next_len; i++)
                if (sunq_next[i].x == x && sunq_next[i].y == y && sunq_next[i].z == z)
                        return; // already queued in next queue

        sunq_next[sq_next_len].x = x;
        sunq_next[sq_next_len].y = y;
        sunq_next[sq_next_len].z = z;
        sq_next_len++;
}

void glo_enqueue(int x, int y, int z, int base, unsigned char incoming_light)
{
        if (incoming_light == 0)
                return;

        if (T_(x, y, z) == WATR)
                incoming_light--; // water blocks more light

        if (T_(x, y, z) == RLEF || T_(x, y, z) == YLEF)
        {
                incoming_light--; // leaves block more light
                if (incoming_light) incoming_light--;
        }

        if (GLO_(x, y, z) >= incoming_light)
                return; // already brighter

        if (T_(x, y, z) < OPEN)
                return; // no lighting for solid blocks

        GLO_(x, y, z) = incoming_light;

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
        sunq_curr = sunq_next;
        sq_curr_len = sq_next_len;
        sq_next_len = 0;
        sunq_next = (sunq_curr == sunq0_) ? sunq1_ : sunq0_;

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

void recalc_corner_lighting(int xlo, int xhi, int zlo, int zhi)
{
        for (int z = zlo; z < zhi; z++) for (int y = 0; y < TILESH; y++) for (int x = xlo; x < xhi; x++)
        {
                int x_ = (x == 0) ? 0 : x - 1;
                int y_ = (y == 0) ? 0 : y - 1;
                int z_ = (z == 0) ? 0 : z - 1;

                CORN_(x, y, z) = 0.008f * (
                                SUN_(x_, y_, z_) + SUN_(x , y_, z_) + SUN_(x_, y , z_) + SUN_(x , y , z_) +
                                SUN_(x_, y_, z ) + SUN_(x , y_, z ) + SUN_(x_, y , z ) + SUN_(x , y , z ));
                KORN_(x, y, z) = 0.008f * (
                                GLO_(x_, y_, z_) + GLO_(x , y_, z_) + GLO_(x_, y , z_) + GLO_(x , y , z_) +
                                GLO_(x_, y_, z ) + GLO_(x , y_, z ) + GLO_(x_, y , z ) + GLO_(x , y , z ));
        }
}

// remove direct or indirect sunlight
// before calling, set opacity, but not light value
void remove_sunlight(int px, int py, int pz)
{
        // FIXME: remove when confident
        static int recursions = 0;
        if (++recursions > 1000000)
        {
                fprintf(stderr, "1 million remove_sunlight() recursions\n");
                return;
        }

        int my_light = SUN_(px, py, pz);
        int im_opaque = IS_OPAQUE(px, py, pz);

        if (my_light < 1) return;

        struct qitem check_list[6];
        struct qitem recur_list[6];
        int check_len = 0;
        int recur_len = 0;

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

        int incoming_light = 0;
        int future_light = 0;

        // find valid neighbors to check
        if (px > 0       ) check_list[check_len++] = QITEM(px-1, py  , pz  );
        if (px < TILESW-1) check_list[check_len++] = QITEM(px+1, py  , pz  );
        if (py > 0       ) check_list[check_len++] = QITEM(px  , py-1, pz  );
        // never spread sunlight value 15 upward:
        if (py < TILESH-1 && SUN_(px,py+1,pz) != 15) check_list[check_len++] = QITEM(px  , py+1, pz  );
        if (pz > 0       ) check_list[check_len++] = QITEM(px  , py  , pz-1);
        if (pz < TILESD-1) check_list[check_len++] = QITEM(px  , py  , pz+1);

        for (int i = 0; i < check_len; i++)
        {
                int x = check_list[i].x;
                int y = check_list[i].y;
                int z = check_list[i].z;

                // no need to update my opaque neighbors
                if (IS_OPAQUE(x, y, z)) continue;

                // i am lit by a neighbor block as much or more than before
                // so... there is no more light to remove in this branch
                if (SUN_(x, y, z) > my_light && !im_opaque) return;

                // i am [now] being lit by this neighbor
                if (SUN_(x, y, z) == my_light && !im_opaque)
                        incoming_light = MAX(incoming_light, SUN_(x, y, z) - 1);

                // keep track of brightest neighboring light for queueing later
                if (SUN_(x, y, z) > future_light && !im_opaque)
                        future_light = SUN_(x, y, z);

                // i could be the light source for this neighbor, need to recurse
                if (SUN_(x, y, z) < my_light)
                        recur_list[recur_len++] = QITEM(x, y, z);
        }

        if (incoming_light >= my_light)
                fprintf(stderr, "INCOMING LIGHT > MY LIGHT when darkening\n");

        SUN_(px, py, pz) = incoming_light;

        // re-lighting may be needed here
        if (future_light)
                sun_enqueue(px, py, pz, 0, future_light - 1);

        // i had no light to give anyway
        if (my_light < 2) return;

        for (int i = 0; i < recur_len; i++)
                remove_sunlight(recur_list[i].x, recur_list[i].y, recur_list[i].z);
}

void remove_glolight(int px, int py, int pz)
{
        // FIXME: remove when confident
        static int recursions = 0;
        if (++recursions > 1000000)
        {
                fprintf(stderr, "1 million remove_glolight() recursions\n");
                return;
        }

        int my_light = GLO_(px, py, pz);
        int im_opaque = IS_OPAQUE(px, py, pz);

        if (my_light < 1) return;

        struct qitem check_list[6];
        struct qitem recur_list[6];
        int check_len = 0;
        int recur_len = 0;
        int incoming_light = 0;
        int future_light = 0;

        // find valid neighbors to check
        if (px > 0       ) check_list[check_len++] = QITEM(px-1, py  , pz  );
        if (px < TILESW-1) check_list[check_len++] = QITEM(px+1, py  , pz  );
        if (py > 0       ) check_list[check_len++] = QITEM(px  , py-1, pz  );
        if (py < TILESH-1) check_list[check_len++] = QITEM(px  , py+1, pz  );
        if (pz > 0       ) check_list[check_len++] = QITEM(px  , py  , pz-1);
        if (pz < TILESD-1) check_list[check_len++] = QITEM(px  , py  , pz+1);

        for (int i = 0; i < check_len; i++)
        {
                int x = check_list[i].x;
                int y = check_list[i].y;
                int z = check_list[i].z;

                // no need to update my opaque neighbors
                if (IS_OPAQUE(x, y, z)) continue;

                // i am lit by a neighbor block as much or more than before
                // so... there is no more light to remove in this branch
                if (GLO_(x, y, z) > my_light && !im_opaque) return;

                // i am [now] being lit by this neighbor
                if (GLO_(x, y, z) == my_light && !im_opaque)
                        incoming_light = MAX(incoming_light, GLO_(x, y, z) - 1);

                // keep track of brightest neighboring light for queueing later
                if (GLO_(x, y, z) > future_light && !im_opaque)
                        future_light = GLO_(x, y, z);

                // i could be the light source for this neighbor, need to recurse
                if (GLO_(x, y, z) < my_light)
                        recur_list[recur_len++] = QITEM(x, y, z);
        }

        if (incoming_light >= my_light)
                fprintf(stderr, "GLO: INCOMING LIGHT > MY LIGHT when darkening\n");

        GLO_(px, py, pz) = incoming_light;

        // re-lighting may be needed here
        if (future_light)
                glo_enqueue(px, py, pz, 0, future_light - 1);

        // i had no light to give anyway
        if (my_light < 2) return;

        for (int i = 0; i < recur_len; i++)
                remove_glolight(recur_list[i].x, recur_list[i].y, recur_list[i].z);
}


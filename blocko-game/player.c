#include "blocko.h"

int gravity[] = { -20, -17, -14, -12, -10, -8, -6, -5, -4, -3,
                   -2,  -2,  -1,  -1,   0,  1,  1,  2,  2,  3,
                    3,   4,   4,   5,   5,  6,  6,  7,  7,  8,
                    9,  10,  10,  11,  12, 12, 13, 14, 15, 16,
                   17,  18,  19,  20,  21, 22, 23, 24, 25, 26};

// for extrapolating the camera in > 60FPS
void lerp_camera(float t, struct player *a, struct player *b)
{
        lerped_pos.x = lerp(t, a->pos.x, b->pos.x);
        lerped_pos.y = lerp(t, a->pos.y, b->pos.y);
        lerped_pos.z = lerp(t, a->pos.z, b->pos.z);
}

//return 0 iff we couldn't actually move
int move_player(struct player *p, int velx, int vely, int velz)
{
        int last_was_x = false;
        int last_was_z = false;
        int already_stuck = false;
        int moved = false;

        if (!velx && !vely && !velz)
                return 1;

        if (world_collide(p->pos, 0))
                already_stuck = true;

        while (velx || vely || velz)
        {
                struct box testpos = p->pos;
                int amt;

                if ((!velx && !velz) || ((last_was_x || last_was_z) && vely))
                {
                        amt = vely > 0 ? 1 : -1;
                        testpos.y += amt;
                        vely -= amt;
                        last_was_x = false;
                        last_was_z = false;
                }
                else if (!velz || (last_was_z && velx))
                {
                        amt = velx > 0 ? 1 : -1;
                        testpos.x += amt;
                        velx -= amt;
                        last_was_z = false;
                        last_was_x = true;
                }
                else
                {
                        amt = velz > 0 ? 1 : -1;
                        testpos.z += amt;
                        velz -= amt;
                        last_was_x = false;
                        last_was_z = true;
                }

                int would_be_stuck = false;

                if (world_collide(testpos, 0))
                        would_be_stuck = true;
                else
                        already_stuck = false;

                if (would_be_stuck && !already_stuck)
                {
                        if (last_was_x)
                                velx = 0;
                        else if (last_was_z)
                                velz = 0;
                        else
                                vely = 0;
                        continue;
                }

                p->pos = testpos;
                moved = true;
        }

        return moved;
}


void update_player(struct player *p, int real)
{
        int zoomzip = false;
        if (!zooming && zoom_amt < 1.f)
                zoomzip = true;

        if (real && p->pos.y > TILESH*BS + 6000) // fell too far
        {
                player[0].pos.x = STARTPX;
                player[0].pos.z = STARTPZ;
                move_to_ground(&player[0].pos.y, STARTPX/BS, STARTPY/BS, STARTPZ/BS);
        }

        if (p->jumping && p->wet)
        {
                p->grav = GRAV_JUMP;
        }
        else if (p->jumping)
        {
                p->jumping--; // reduce buffer frames
                if (p->ground)
                {
                        p->grav = GRAV_JUMP;
                        p->jumping = false;
                }
        }

        if (p->cooldown) p->cooldown--;

        if (real && p->breaking && !p->cooldown && target_x >= 0)
        {
                int x = target_x;
                int y = target_y;
                int z = target_z;
                unsigned char max = 0;
                int broken = T_(x, y, z);
                T_(x, y, z) = OPEN;

                if (broken == LITE)
                {
                        remove_glolight(x, y, z);
                        goto out;
                }

                // gndheight needs to change if we broke the ground
                if (AT_GROUND(x, y, z))
                        recalc_gndheight(x, z);

                if (ABOVE_GROUND(x, y, z))
                {
                        while (y <= TILESH-1)
                        {
                                sun_enqueue(x, y, z, 0, 15);
                                y++;
                                if (!ABOVE_GROUND(x, y, z)) break;
                        }
                }
                else
                {
                        if (x > 0        && SUN_(x-1, y  , z  ) > max) max = SUN_(x-1, y  , z  );
                        if (x < TILESW-1 && SUN_(x+1, y  , z  ) > max) max = SUN_(x+1, y  , z  );
                        if (y > 0        && SUN_(x  , y-1, z  ) > max) max = SUN_(x  , y-1, z  );
                        if (y < TILESH-1 && SUN_(x  , y+1, z  ) > max) max = SUN_(x  , y+1, z  );
                        if (z > 0        && SUN_(x  , y  , z-1) > max) max = SUN_(x  , y  , z-1);
                        if (z < TILESD-1 && SUN_(x  , y  , z+1) > max) max = SUN_(x  , y  , z+1);
                        sun_enqueue(x, y, z, 0, max ? max - 1 : 0);
                }

                max = 0;
                if (x > 0        && GLO_(x-1, y  , z  ) > max) max = GLO_(x-1, y  , z  );
                if (x < TILESW-1 && GLO_(x+1, y  , z  ) > max) max = GLO_(x+1, y  , z  );
                if (y > 0        && GLO_(x  , y-1, z  ) > max) max = GLO_(x  , y-1, z  );
                if (y < TILESH-1 && GLO_(x  , y+1, z  ) > max) max = GLO_(x  , y+1, z  );
                if (z > 0        && GLO_(x  , y  , z-1) > max) max = GLO_(x  , y  , z-1);
                if (z < TILESD-1 && GLO_(x  , y  , z+1) > max) max = GLO_(x  , y  , z+1);
                glo_enqueue(x, y, z, 0, max ? max - 1 : 0);

                out:
                p->cooldown = 5;
        }

        if (real && p->building && !p->cooldown && place_x >= 0) {
                if (!collide(p->pos, (struct box){ place_x * BS, place_y * BS, place_z * BS, BS, BS, BS }))
                {
                        T_(place_x, place_y, place_z) = HARD;

                        if (ABOVE_GROUND(place_x, place_y, place_z))
                                GNDH_(place_x, place_z) = place_y;

                        int y = place_y;
                        remove_glolight(place_x, y, place_z);
                        do {
                                remove_sunlight(place_x, y, place_z);
                                y++;
                        } while (y < TILESH-1 && !IS_OPAQUE(place_x, y, place_z));
                }
                p->cooldown = 10;
        }

        if (real && p->lighting && !p->cooldown && place_x >= 0) {
                T_(place_x, place_y, place_z) = LITE;
                glo_enqueue(place_x, place_y, place_z, 0, 15);
                p->cooldown = 10;
        }

        // double tap forward to run
        if (p->cooldownf > 10) p->runningf = true;
        if (p->cooldownf > 0) p->cooldownf--;
        if (!p->goingf) p->runningf = false;

        if (p->goingf && !p->goingb) { p->fvel++; }
        else if (p->fvel > 0)        { p->fvel--; }

        if (p->goingb && !p->goingf) { p->fvel--; }
        else if (p->fvel < 0)        { p->fvel++; }

        if (p->goingr && !p->goingl) { p->rvel++; }
        else if (p->rvel > 0)        { p->rvel--; }

        if (p->goingl && !p->goingr) { p->rvel--; }
        else if (p->rvel < 0)        { p->rvel++; }

        //limit speed
        float totalvel = sqrt(p->fvel * p->fvel + p->rvel * p->rvel);
        float limit = (p->running || p->runningf)  ? PLYR_SPD_R :
                      p->sneaking                  ? PLYR_SPD_S :
                                                     PLYR_SPD;
        limit *= fast;
        if (totalvel > limit)
        {
                limit /= totalvel;
                if (p->fvel > 4 || p->fvel < -4) p->fvel *= limit;
                if (p->rvel > 4 || p->rvel < -4) p->rvel *= limit;
        }

        float fwdx = sin(p->yaw);
        float fwdy = sin(p->pitch);
        float fwdz = cos(p->yaw);

        p->vel.x = fwdx * p->fvel + fwdz * p->rvel;
        p->vel.z = fwdz * p->fvel - fwdx * p->rvel;

        if (!move_player(p, p->vel.x, p->vel.y, p->vel.z))
        {
                p->fvel = 0;
                p->rvel = 0;
        }

        //detect water
        int was_wet = p->wet;
        p->wet = world_collide(p->pos, 1);
        if (was_wet && !p->wet && p->grav < GRAV_FLOAT)
                p->grav = GRAV_FLOAT;

        //gravity
        if ((!p->ground || p->grav < GRAV_ZERO) && zoom_amt > .999f)
        {
                float fall_dist = gravity[p->grav] / (p->wet ? 3 : 1);
                if (!move_player(p, 0, fall_dist, 0))
                        p->grav = GRAV_ZERO;
                else if (p->grav < GRAV_MAX)
                        p->grav++;
        }

        //detect ground
        struct box foot = (struct box){
                p->pos.x, p->pos.y + PLYR_H, p->pos.z,
                PLYR_W, 1, PLYR_W};
        p->ground = world_collide(foot, 0);

        if (p->ground)
                p->grav = GRAV_ZERO;

        //zooming
        if (real)
        {
                if (zoomzip)
                {
                        move_player(p, fwdx * 300.f, fwdy * 300.f, fwdz * 300.f);
                }
                zoom_amt *= zooming ? 0.9f : 1.1f;
                CLAMP(zoom_amt, 0.1f, 1.0f);
        }
}


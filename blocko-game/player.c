#include "blocko.c"
#ifndef BLOCKO_PLAYER_C_INCLUDED
#define BLOCKO_PLAYER_C_INCLUDED

int gravity[] = { -333, -283, -233, -200, -167, -133, -100, -83, -67, -50,
                   -40,  -30,  -20,  -10,    0,    8,   16,  24,  32,  40,
                    48,   56,   64,   72,   80,   90,  100, 110, 120, 130,
                   140,  150,  160,  170,  180,  190,  200, 220, 240, 260,
                   280,  300,  320,  340,  360,  380,  400, 420, 440, 450 };

// for extrapolating the camera in > 60FPS
void lerp_camera(float t, struct player *a, struct player *b)
{
        lerped_pos.x = lerp(t, a->pos.x, b->pos.x);
        lerped_pos.y = lerp(t, a->pos.y, b->pos.y);
        lerped_pos.z = lerp(t, a->pos.z, b->pos.z);
}

//return 0 iff we couldn't actually move
//swept collision against the world, one unit at a time
int move_box(struct box *pos, int velx, int vely, int velz)
{
        int last_was_x = false;
        int last_was_z = false;
        int already_stuck = false;
        int moved = false;

        if (!velx && !vely && !velz)
                return 1;

        if (world_collide(*pos, 0))
                already_stuck = true;

        while (velx || vely || velz)
        {
                struct box testpos = *pos;
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

                *pos = testpos;
                moved = true;
        }

        return moved;
}

int move_player(struct player *p, int velx, int vely, int velz)
{
        return move_box(&p->pos, velx, vely, velz);
}


// forget the current dig. The block stays solid in tiles the whole time, so
// there's nothing to "put back" - dropping mine_frac/mine_hole retires the
// transient reject+patch (patch.c) and the untouched chunk buffer shows the
// block again next frame.
void mine_heal()
{
        mine_hole = 0;
        mine_x = mine_y = mine_z = -1;
        mine_tile = -1;
        mine_frac = 0.f;
        player[0].mine_progress = 0;
}

void update_player(struct player *p, int real)
{
        if (real && !p->noclip && p->pos.y > TILESH*BS + 6000) // fell too far
        {
                player[0].pos.x = STARTPX;
                player[0].pos.z = STARTPZ;
                move_to_ground(&player[0].pos.y, STARTPX/BS, STARTPY/BS, STARTPZ/BS);
        }

        if (p->jumping && p->ground)
        {
                p->grav = GRAV_JUMP; // normal jump, even off underwater ground
                p->jumping = false;
        }
        else if (p->jump_held && !p->ground && p->grav > GRAV_SWIM
                        && (p->submerged || (p->wet && p->grav < GRAV_ZERO)))
        {
                // swim: ease toward swim-up speed while submerged;
                // once rising, keep thrusting until fully out of the water
                p->grav -= 3;
                if (p->grav < GRAV_SWIM)
                        p->grav = GRAV_SWIM;
        }

        if (p->jumping)
                p->jumping--; // reduce buffer frames

        if (p->cooldown) p->cooldown--;

        // hold left click to mine: progress builds only while aimed at the
        // same block, so releasing (a quick click) never breaks anything.
        // The block shows as a hole instantly via the shader reject box + patch
        // mesh (patch.c), with a shaking stand-in drawn in its place (mine.c) -
        // no chunk rebuild, so nothing hitches while you dig.
        if (real && p->breaking && target_x >= 0)
        {
                if (target_x == mine_x && target_y == mine_y && target_z == mine_z)
                        p->mine_progress++;
                else
                {
                        // aim moved: the reject box just follows mine_x next frame,
                        // so the old block reappears with no rebuild
                        mine_x = target_x;
                        mine_y = target_y;
                        mine_z = target_z;
                        mine_tile = T_(mine_x, mine_y, mine_z);
                        mine_hole = 1;
                        p->mine_progress = 1;
                }
                mine_frac = (float)p->mine_progress / MINE_TIME;
        }
        else if (real)
        {
                mine_heal();
        }

        if (real && p->breaking && p->mine_progress >= MINE_TIME && target_x >= 0)
        {
                int x = target_x;
                int y = target_y;
                int z = target_z;
                unsigned char max = 0;
                int broken = T_(x, y, z);
                T_(x, y, z) = OPEN;
                // no immediate chunk rebuild: patch_edit (below) shows the hole
                // instantly and schedules a debounced rebuild to fold it in

                // tall grass grows on top of a block (the cell at y-1); with its
                // footing gone it has nothing to root in, so clear it too. same
                // chunk column, so patch_edit's debounced rebuild drops it.
                if (y > 0 && (T_(x, y-1, z) == TLGR || T_(x, y-1, z) == TMGR))
                        T_(x, y-1, z) = OPEN;

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
                        // flood sunlight down the newly-opened column, but with a
                        // scratch index - mutating y here corrupts the coords
                        // handed to patch_edit/glo below (the reject+patch box
                        // would land at the wrong height, leaving the broken
                        // block uncovered - a one-frame-late "ghost" on trunks)
                        for (int yy = y; yy <= TILESH-1; yy++)
                        {
                                sun_enqueue(x, yy, z, 0, 15);
                                if (!ABOVE_GROUND(x, yy+1, z)) break;
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
                // pop a little half-size copy of the block loose to tumble to the
                // ground (cosmetic; catches every break path, lights included)
                item_spawn(x, y, z, broken);
                // hand the dig off to a persistent edit patch: the block is now
                // OPEN in tiles, so schedule the debounced rebuild and let the
                // reject+patch cover the hole until it lands. mine_heal ends the dig.
                patch_edit(x, y, z);
                mine_heal();
                p->cooldown = 5;
                // this target is spent. A long frame can run another update tick
                // before the next rayshot (draw), and target still points at the
                // cell we just opened - now OPEN. Blank it so that stale tick can't
                // re-target it, cache mine_tile = OPEN, and draw the shaking
                // stand-in as the default STON tile for a frame. rayshot refills
                // target next frame, so mining the block behind resumes normally.
                target_x = target_y = target_z = -1;
        }

        if (real && p->building && !p->cooldown && place_x >= 0) {
                if (!collide(p->pos, (struct box){ place_x * BS, place_y * BS, place_z * BS, BS, BS, BS }))
                {
                        T_(place_x, place_y, place_z) = held_tile;
                        patch_edit(place_x, place_y, place_z); // instant, debounced rebuild
                        hand_swing_kick = 1;                   // swing the held block

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
                DIRTY_(B2C(place_x), B2C(place_z)) = 1;
                glo_enqueue(place_x, place_y, place_z, 0, 15);
                p->cooldown = 10;
        }

        // double tap forward to run
        if (p->cooldownf > 10) p->runningf = true;
        if (p->cooldownf > 0) p->cooldownf--;
        if (!p->goingf) p->runningf = false;

        if (p->goingf && !p->goingb) { p->fvel += PLYR_ACCEL; }
        else if (p->fvel > 0)        { p->fvel -= PLYR_ACCEL; if (p->fvel < 0) p->fvel = 0; }

        if (p->goingb && !p->goingf) { p->fvel -= PLYR_ACCEL; }
        else if (p->fvel < 0)        { p->fvel += PLYR_ACCEL; if (p->fvel > 0) p->fvel = 0; }

        if (p->goingr && !p->goingl) { p->rvel += PLYR_ACCEL; }
        else if (p->rvel > 0)        { p->rvel -= PLYR_ACCEL; if (p->rvel < 0) p->rvel = 0; }

        if (p->goingl && !p->goingr) { p->rvel -= PLYR_ACCEL; }
        else if (p->rvel < 0)        { p->rvel += PLYR_ACCEL; if (p->rvel > 0) p->rvel = 0; }

        //limit speed
        float totalvel = sqrt(p->fvel * p->fvel + p->rvel * p->rvel);
        float limit = (p->running || p->runningf)  ? PLYR_SPD_R :
                      p->sneaking                  ? PLYR_SPD_S :
                                                     PLYR_SPD;
        limit *= fast;
        if (p->wet) limit /= 2; // water drag
        if (totalvel > limit)
        {
                limit /= totalvel;
                if (p->fvel > 4 || p->fvel < -4) p->fvel *= limit;
                if (p->rvel > 4 || p->rvel < -4) p->rvel *= limit;
        }

        float fwdx = sin(p->yaw);
        float fwdz = cos(p->yaw);

        p->vel.x = fwdx * p->fvel + fwdz * p->rvel;
        p->vel.z = fwdz * p->fvel - fwdx * p->rvel;

        if (p->noclip)
        {
                // fly freely: move straight through the world with no collision
                // and no gravity. jump rises, sneak sinks (y grows downward).
                float vspeed = (p->running || p->runningf) ? PLYR_SPD_R :
                               p->sneaking                 ? PLYR_SPD_S :
                                                             PLYR_SPD;
                vspeed *= fast;
                p->pos.x += p->vel.x;
                p->pos.z += p->vel.z;
                if (p->jump_held) p->pos.y -= vspeed;
                if (p->sneaking)  p->pos.y += vspeed;
                p->vel.y = 0;
                p->grav = GRAV_ZERO;
                p->ground = 0;
                p->wet = 0;
                p->submerged = 0;
                if (real)
                {
                        zoom_amt *= zooming ? 0.9f : 1.2f;
                        zoom_amt = CLAMP(zoom_amt, 0.25f, 1.0f);
                }
                return;
        }

        if (!move_player(p, p->vel.x, p->vel.y, p->vel.z))
        {
                p->fvel = 0;
                p->rvel = 0;
        }

        //detect water
        int was_wet = p->wet;
        struct box torso = (struct box){
                p->pos.x, p->pos.y, p->pos.z,
                PLYR_W, PLYR_H/2, PLYR_W};
        p->wet = world_collide(p->pos, 1);
        p->submerged = world_collide(torso, 1);
        if (was_wet && !p->wet && p->grav < GRAV_FLOAT)
        {
                // breaking the surface: hop out if jumping, else don't launch
                p->grav = p->jump_held ? MIN(p->grav, GRAV_EXIT) : GRAV_FLOAT;
        }

        //gravity
        if (!p->ground || p->grav < GRAV_ZERO)
        {
                float fall_dist = gravity[p->grav] / (p->wet ? 3 : 1);
                if (!move_player(p, 0, fall_dist, 0))
                        p->grav = GRAV_ZERO;
                else if (p->grav < GRAV_MAX)
                        p->grav++;

                if (p->wet && p->grav > GRAV_WET_MAX) // water terminal velocity
                        p->grav = GRAV_WET_MAX;
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
                zoom_amt *= zooming ? 0.9f : 1.2f;
                zoom_amt = CLAMP(zoom_amt, 0.25f, 1.0f);
        }
}

#endif // BLOCKO_PLAYER_C_INCLUDED

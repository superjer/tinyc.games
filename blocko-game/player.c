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

// how high a horizontal move will auto-climb (raise-move-lower, see
// update_player). A bit over half a block: smooths ramp tops and low ledges
// while keeping full blocks as obstacles you still have to jump.
#define STEP_HEIGHT (BS * 9 / 16)

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

// horizontal move with auto-step: slide (velx,velz) along the ground, and if a
// low rise stops it short - the slope staircase (collision.c), a half-block
// ledge - climb over by raising up to STEP_HEIGHT, redoing the move, and
// settling back onto the step. Full blocks (taller than STEP_HEIGHT) still
// block. The caller vouches the box is on the ground. Returns whether it moved.
// The player runs its own inline version (below) because its move is one
// combined x/y/z sweep; this is the standalone form skating mobs use.
int move_box_step(struct box *pos, int velx, int velz)
{
        struct box pre = *pos;
        move_box(pos, velx, 0, velz);

        float want = fabsf((float)velx) + fabsf((float)velz);
        float got  = fabsf(pos->x - pre.x) + fabsf(pos->z - pre.z);
        if (got + 1.f >= want)
                return 1;                         // moved freely, nothing to climb

        struct box low = *pos;                    // the blocked result
        struct box lifted = pre;
        lifted.y -= STEP_HEIGHT;                   // y grows downward
        if (world_collide(lifted, 0))              // no headroom to rise
                return got >= 1.f;

        *pos = lifted;
        move_box(pos, velx, 0, velz);
        for (int s = 0; s < STEP_HEIGHT; s++)      // drop back onto the step
        {
                struct box down = *pos;
                down.y += 1;
                if (world_collide(down, 0)) break;
                *pos = down;
        }
        // no better than the blocked spot? keep the low result
        float climbed = fabsf(pos->x - pre.x) + fabsf(pos->z - pre.z);
        if (climbed <= got + 1.f)
                *pos = low;
        return fabsf(pos->x - pre.x) + fabsf(pos->z - pre.z) >= 1.f;
}

// which way a freshly placed slope should descend: away from the camera, so it
// rises the direction you're looking and you walk up it (facing = compass dir
// the surface heads downhill)
static int slope_facing(float yaw)
{
        float fx = sinf(yaw), fz = cosf(yaw);
        if (fabsf(fz) >= fabsf(fx))
                return fz >= 0 ? SLOPE_S : SLOPE_N; // looking north -> descend south
        else
                return fx >= 0 ? SLOPE_W : SLOPE_E; // looking east  -> descend west
}

// world-Y of the slope surface under the player's horizontal center, or a big
// sentinel if the player isn't standing over any slope cell (and the winning
// facing, for the uphill-speed check). Y grows downward, so a smaller value is
// a higher surface; when the footprint covers more than one slope the highest
// wins. Slopes are their own solid floor now (they collide as a staircase, see
// block_collide); this only reports which slope/facing you're on so the walk
// speed can ease off going uphill.
#define NO_RAMP 1e30f
float ramp_surface(struct box box, int *facing)
{
        float best = NO_RAMP;
        int bestf = 0;
        float cx = box.x + box.w * 0.5f;
        float cz = box.z + box.d * 0.5f;
        int bx = (int)floorf(cx / BS);
        int bz = (int)floorf(cz / BS);
        int ylo = (int)floorf(box.y / BS) - 1;
        int yhi = (int)floorf((box.y + box.h) / BS) + 1;

        for (int by = ylo; by <= yhi; by++)
        {
                if (!legit_tile(bx, by, bz) || T_(bx, by, bz) != GSLP)
                        continue;
                int f = TO_(bx, by, bz) & 3;
                float top = BS * by, bot = BS * (by + 1);
                float fz = (CLAMP(cz, BS*bz, BS*(bz+1)) - BS*bz) / BS; // 0 south..1 north
                float fx = (CLAMP(cx, BS*bx, BS*(bx+1)) - BS*bx) / BS; // 0 west ..1 east
                float surf = f == SLOPE_S ? bot - fz*BS :  // high side north
                             f == SLOPE_N ? top + fz*BS :  // high side south
                             f == SLOPE_W ? bot - fx*BS :  // high side east
                                            top + fx*BS ;  // high side west (SLOPE_E)
                if (surf < best) { best = surf; bestf = f; }
        }
        if (facing) *facing = bestf;
        return best;
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
        player[my_player].mine_progress = 0;
}

void update_player(struct player *p, int real)
{
        if (real && !p->noclip && p->pos.y > TILESH*BS + 6000) // fell too far
        {
                p->pos.x = STARTPX;
                p->pos.z = STARTPZ;
                move_to_ground(&p->pos.y, STARTPX/BS, STARTPY/BS, STARTPZ/BS);
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
                int broken = T_(x, y, z);

                // tall grass grows on top of a block (the cell at y-1); with its
                // footing gone it has nothing to root in, so clear it too - first,
                // so the main break's gndheight recalc doesn't stop at the grass
                if (y > 0 && (T_(x, y-1, z) == TLGR || T_(x, y-1, z) == TMGR))
                        set_tile(x, y-1, z, OPEN, 0);

                // set_tile (edit.c) records the edit and handles gndheight,
                // lighting, and the instant reject+patch - no chunk rebuild here
                set_tile(x, y, z, OPEN, 0);

                // pop a little half-size copy of the block loose to tumble to the
                // ground (cosmetic; catches every break path, lights included)
                item_spawn(x, y, z, broken);
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
                        // a slope faces so its surface descends the way you look, so
                        // you place it at your feet and walk up it
                        int orient = (held_tile == GSLP) ? slope_facing(p->yaw) : 0;
                        set_tile(place_x, place_y, place_z, held_tile, orient);
                        hand_swing_kick = 1; // swing the held block
                }
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
                // fly freely and fast: move straight through the world with no
                // collision and no gravity, 8x speed. forward/backward follow
                // the look direction exactly (pitch included); strafing stays
                // level. jump rises, sneak sinks (y grows downward).
                float c = cosf(p->pitch);
                float vspeed = 8.f * ((p->running || p->runningf) ? PLYR_SPD_R :
                                      p->sneaking                 ? PLYR_SPD_S :
                                                                    PLYR_SPD);
                p->pos.x += (fwdx * c * p->fvel + fwdz * p->rvel) * 8.f;
                p->pos.z += (fwdz * c * p->fvel - fwdx * p->rvel) * 8.f;
                p->pos.y += sinf(p->pitch) * p->fvel * 8.f;
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

        // slower going up a slope: full speed across or downhill, 70% straight
        // up, blended by how aligned the move is with the uphill direction
        if (p->ground)
        {
                int sf;
                if (ramp_surface(p->pos, &sf) < NO_RAMP)
                {
                        float ux = 0, uz = 0;
                        switch (sf) {
                        case SLOPE_S: uz =  1; break; // descends south -> uphill north (+z)
                        case SLOPE_N: uz = -1; break;
                        case SLOPE_W: ux =  1; break; // descends west -> uphill east (+x)
                        case SLOPE_E: ux = -1; break;
                        }
                        float vl = sqrtf(p->vel.x*p->vel.x + p->vel.z*p->vel.z);
                        if (vl > 0.001f)
                        {
                                float a = (p->vel.x*ux + p->vel.z*uz) / vl; // cos to uphill
                                if (a > 0) // has an uphill component: ease off, down to 70%
                                {
                                        float f = 1.0f - 0.3f * a;
                                        p->vel.x *= f;
                                        p->vel.z *= f;
                                }
                        }
                }
        }

        struct box pre = p->pos;
        move_player(p, p->vel.x, p->vel.y, p->vel.z);

        // auto-step: if a wall stopped the horizontal move short while we were on
        // the ground, try to climb it (raise up to STEP_HEIGHT, redo the move,
        // then settle back down onto the step). Makes single blocks, stairs, and
        // the slope's staircase (collision.c) walkable without jumping.
        float want = fabsf(p->vel.x) + fabsf(p->vel.z);
        float got  = fabsf(p->pos.x - pre.x) + fabsf(p->pos.z - pre.z);
        if (p->ground && p->vel.y == 0 && got + 1.f < want)
        {
                struct box low = p->pos;                 // the blocked result
                struct box lifted = pre;
                lifted.y -= STEP_HEIGHT;                  // y grows downward
                if (!world_collide(lifted, 0))           // headroom to rise?
                {
                        p->pos = lifted;
                        move_player(p, p->vel.x, 0, p->vel.z);
                        // drop back onto the step surface (never below the start)
                        for (int s = 0; s < STEP_HEIGHT; s++)
                        {
                                struct box down = p->pos;
                                down.y += 1;
                                if (world_collide(down, 0)) break;
                                p->pos = down;
                        }
                        // no better than the blocked spot? keep the low result
                        float climbed = fabsf(p->pos.x - pre.x) + fabsf(p->pos.z - pre.z);
                        if (climbed <= got + 1.f)
                                p->pos = low;
                }
        }

        got = fabsf(p->pos.x - pre.x) + fabsf(p->pos.z - pre.z);
        if (got < 1.f) // truly stuck: bleed off the speed we couldn't spend
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

        // slopes need no special floor handling: they collide as a solid
        // staircase (collision.c), so gravity settles the feet on a step and
        // the auto-step above walks up them like any other stairs.

        //zooming
        if (real)
        {
                zoom_amt *= zooming ? 0.9f : 1.2f;
                zoom_amt = CLAMP(zoom_amt, 0.25f, 1.0f);
        }
}

#endif // BLOCKO_PLAYER_C_INCLUDED

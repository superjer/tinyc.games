#include "blocko.c"
#ifndef BLOCKO_MOB_C_INCLUDED
#define BLOCKO_MOB_C_INCLUDED

// mob.c - slimes: hopping enemies that chase and bonk the player
//
// A slime is a single green cube drawn with the terrain pipeline - each one
// is a tiny one-cube "chunk" placed and scaled by push constants, plus two
// dark eye quads on whichever side it hopped toward. Slimes wander until the
// player gets close, then hop after them and knock them around on contact.
// Three punches (left click) pop one.

#define MOB_W 950                 // a full size-12 body cube nearly fills a block
#define MOB_H 950
#define MOB_PIX (MOB_W/4)         // eye quads live on a 4x4 grid on the face
#define MOB_HP 2                  // vitality, separate from size: start and cap.
                                  // shatter drops it, merges keep the best of
                                  // the pair; at 0 a slime can't merge and rots
#define MOB_HOP 130               // horizontal hop speed, units per frame
#define MOB_SKATE_SPD 80          // top glide speed for skating slimes
#define MOB_SKATE_ACCEL 5         // thrust per frame along the heading (low = more drift)
#define MOB_SKATE_DRIFT 0.98f     // momentum kept each frame (near 1 = very slippery)
#define MOB_BUOY_K 0.06f          // buoyancy spring stiffness toward the rest depth
#define MOB_BUOY_DAMP 0.85f       // damping on the bob spring
#define MOB_BOB_AMPL 0.12f        // bob height as a fraction of the body
#define MOB_BOB_RATE 0.09f        // bob speed (radians/frame; ~1s up-and-down)
#define MOB_AGGRO (16*BS)         // notice the player from this far
#define MOB_DESPAWN (100*BS)      // forget mobs that get this far away
#define MOB_REACH (4*BS)          // how far the player can punch
#define MOB_DEATH_FRAMES 15       // shrink-out animation length

#define TEX_SLIME_BODY 35         // texture-array layers for the slime skin
#define TEX_SLIME_EYES 36         // full-face eyes overlay, transparent around the eyes
#define MOB_EYE_OUT 0.02f         // push the eyes this far off the face (kills z-fighting)

// slimes carry a "size" 1..12 that drives everything about them
#define MOB_MAXSIZE 12            // biggest a slime can grow (also a full body)
#define MOB_DANGER 8              // size >= this is dangerous and hunts the player
#define MOB_FLEE 4                // size FLEE..DANGER-1 runs from the player
#define MOB_DECAY 6               // size < this slowly wastes away
#define MOB_POP_TARGET 6          // ambient spawns stop once this many are alive
#define MOB_MERGE_SIGHT (20*BS)   // how far a slime looks for a merge partner
#define MOB_DECAY_FRAMES 120      // rest between decays for a small slime
#define MOB_DOOMED_FRAMES (MOB_DECAY_FRAMES/10) // 10x faster: out of HP or in water

unsigned mob_seed = 60659;

// visual and physical scale for a slime of the given size: a size-1 shard is
// small, a size-12 slime fills the full MOB_W cube
static float mob_scale(int size)
{
        return 0.30f + 0.70f * (size - 1) / (float)(MOB_MAXSIZE - 1);
}

// resize a slime in place: grow/shrink about its horizontal center while
// keeping its feet planted, so merges and decays don't teleport it
static void mob_set_size(struct mob *m, int size)
{
        if (size < 1) size = 1;
        if (size > MOB_MAXSIZE) size = MOB_MAXSIZE;
        float oldw = m->pos.w, oldh = m->pos.h;
        float sc = mob_scale(size);
        float nw = MOB_W * sc, nh = MOB_H * sc;
        m->pos.x -= (nw - oldw) / 2;
        m->pos.z -= (nw - oldw) / 2;
        m->pos.y -= (nh - oldh);      // feet sit at y + h; keep them, raise the top
        m->pos.w = m->pos.d = nw;
        m->pos.h = nh;
        m->size = size;
}

// mobs live in window coords like the player, so they scoot too
void mob_scoot(int dx, int dz)
{
        for (int i = 0; i < NR_MOBS; i++)
        {
                mob[i].pos.x  += dx * BS;  mob[i].pos.z  += dz * BS;
                mob[i].prev.x += dx * BS;  mob[i].prev.z += dz * BS;
        }
}

// spawn a slime standing on the ground at (bx,bz), if there's room for one
int mob_spawn(int bx, int bz)
{
        struct mob *m = NULL;
        for (int i = 0; i < NR_MOBS; i++)
                if (!mob[i].alive) { m = &mob[i]; break; }
        if (!m) return 0;

        if (bx < 1 || bx >= TILESW-1 || bz < 1 || bz >= TILESD-1) return 0;
        if (!AGEN_(B2C(bx), B2C(bz))) return 0; // no terrain here yet

        int gnd = GNDH_(bx, bz);
        if (gnd < 2 || gnd >= TILESH) return 0;
        // never spawn in or over water: check the surface and the block the
        // body will occupy, and refuse anything at/below the waterline
        if (gnd >= SEA_LEVEL) return 0;
        if (T_(bx, gnd, bz) == WATR || T_(bx, gnd - 1, bz) == WATR) return 0;

        // slimes always arrive full-grown; the player carves them down
        int size = MOB_MAXSIZE;
        float sc = mob_scale(size);
        float w = MOB_W * sc, h = MOB_H * sc;

        *m = (struct mob){
                .pos = { bx * BS + (BS - w) / 2, gnd * BS - h - 1,
                         bz * BS + (BS - w) / 2, w, h, w },
                .alive = 1,
                .hp = MOB_HP,
                .size = size,
                .grav = GRAV_ZERO,
                .yaw = 0, .prev_yaw = 0, .target_yaw = 0, // yaw 0 faces -z (south)
                .hop_cooldown = 30,
                .decay_timer = MOB_DECAY_FRAMES,
        };

        if (world_collide(m->pos, 0)) { m->alive = 0; return 0; }
        m->prev = m->pos;
        return 1;
}

// drop a full-grown slime into block cell (bx,by,bz), standing on the cell
// floor. used by the "spawn where I'm pointing" shortcut - unlike mob_spawn it
// doesn't snap to the ground column, so it lands exactly where you aimed.
int mob_spawn_at(int bx, int by, int bz)
{
        struct mob *m = NULL;
        for (int i = 0; i < NR_MOBS; i++)
                if (!mob[i].alive) { m = &mob[i]; break; }
        if (!m) return 0;

        int size = MOB_MAXSIZE;
        float sc = mob_scale(size);
        float w = MOB_W * sc, h = MOB_H * sc;

        *m = (struct mob){
                .pos = { bx * BS + (BS - w) / 2, (by + 1) * BS - h - 1,
                         bz * BS + (BS - w) / 2, w, h, w },
                .alive = 1,
                .hp = MOB_HP,
                .size = size,
                .grav = GRAV_ZERO,
                .yaw = 0, .prev_yaw = 0, .target_yaw = 0,
                .hop_cooldown = 15,
                .decay_timer = MOB_DECAY_FRAMES,
        };

        if (world_collide(m->pos, 0)) { m->alive = 0; return 0; }
        m->prev = m->pos;
        return 1;
}

// spawn a lone size-1 shard centered on `from` with an outward pop; used when
// a slime is punched apart or ejects a bit of itself while decaying. The angle
// aims the pop, `k` staggers the rest/merge timers (and the pop height) so a
// burst of shards doesn't all fly and re-fuse in lockstep, and `hp` is the
// vitality the shard inherits (0 means it can never merge back up).
static int mob_spawn_shard(struct box from, float ang, int k, int hp)
{
        struct mob *m = NULL;
        for (int i = 0; i < NR_MOBS; i++)
                if (!mob[i].alive) { m = &mob[i]; break; }
        if (!m) return 0;

        float sc = mob_scale(1);
        float w = MOB_W * sc, h = MOB_H * sc;
        float cx = from.x + from.w / 2, cz = from.z + from.d / 2;
        // a shard with no HP left is doomed and melts fast. randomize the first
        // decay (1x..2x the base) so a burst of shards doesn't all melt at once
        int base_decay = hp <= 0 ? MOB_DOOMED_FRAMES : MOB_DECAY_FRAMES;
        unsigned seed = mob_seed + k * 0x9E3779B9u + 1;
        int decay0 = base_decay + RANDI(0, base_decay);

        *m = (struct mob){
                .pos = { cx - w / 2, from.y + from.h - h, cz - w / 2, w, h, w },
                .alive = 1,
                .hp = hp < 0 ? 0 : hp,
                .size = 1,
                .grav = GRAV_JUMP + (k % 7), // varied pop height out of the wreck
                .ground = 0,
                .hop_cooldown = 12 + (k % 4) * 6,
                .merge_cooldown = 30 + (k % 5) * 8, // brief rest before hunting to merge
                .decay_timer = decay0,
                .yaw = ang, .prev_yaw = ang, .target_yaw = ang,
        };
        m->vel.x = sinf(ang) * MOB_HOP * 1.6f;
        m->vel.z = cosf(ang) * MOB_HOP * 1.6f;
        m->prev = m->pos;
        return 1;
}

// a click aimed at a mob punches it instead of mining the block behind it
void mob_punch()
{
        struct player *p = &player[0];
        if (!p->breaking || p->cooldown) return;

        float eye0 = p->pos.x + PLYR_W / 2;
        float eye1 = p->pos.y + EYEDOWN;
        float eye2 = p->pos.z + PLYR_W / 2;
        float f0 = cosf(p->pitch) * sinf(p->yaw);
        float f1 = sinf(p->pitch);
        float f2 = cosf(p->pitch) * cosf(p->yaw);

        for (float d = 0; d < MOB_REACH; d += BS / 4.f)
        {
                float x = eye0 + f0 * d;
                float y = eye1 + f1 * d;
                float z = eye2 + f2 * d;
                int bx = x / BS, by = y / BS, bz = z / BS;
                if (legit_tile(bx, by, bz) && IS_SOLID(bx, by, bz))
                        return; // a block is in the way - mine it as usual

                for (int i = 0; i < NR_MOBS; i++)
                {
                        struct mob *m = &mob[i];
                        if (!m->alive || m->dying) continue;
                        if (!collide((struct box){x, y, z, 0, 0, 0}, m->pos))
                                continue;

                        // one punch shatters the slime into size-1 shards that
                        // scatter outward and, after a short rest, hunt each
                        // other down to rebuild a bigger slime
                        int n = m->size;
                        int shard_hp = m->hp - 1; // breaking apart costs a point
                        struct box from = m->pos;
                        m->alive = 0;
                        mob_kills++;
                        // shards spray out roughly along the punch, fanned into
                        // a wedge centered on the aim (not a full circle)
                        float aim = atan2f(f0, f2);
                        unsigned seed = mob_seed;
                        for (int k = 0; k < n; k++)
                        {
                                float a = aim + RANDF(-.9f, .9f);
                                mob_spawn_shard(from, a, k, shard_hp);
                        }
                        mob_seed = seed;
                        // can't mine through a mob: cancel any dig update_player
                        // just started on the block behind it
                        mine_heal();
                        p->cooldown = 10; // this click is spent
                        return;
                }
        }
}

// true if the player sits within the slime's 90-degree forward cone
static int mob_sees_ahead(struct mob *m, float dx, float dz)
{
        float hd = sqrtf(dx * dx + dz * dz);
        if (hd < 1) return 1;
        float fx = sinf(m->yaw), fz = -cosf(m->yaw); // forward from yaw
        float dot = (fx * dx + fz * dz) / hd;
        return dot > 0.7071f; // cos(45): 45 degrees either side of forward
}

// cheap line-of-sight: march a single ray from the slime to the player and
// bail if any solid block sits between them
static int mob_los_clear(struct mob *m, struct player *p)
{
        float ax = m->pos.x + MOB_W / 2, ay = m->pos.y + MOB_H / 4, az = m->pos.z + MOB_W / 2;
        float bx = p->pos.x + PLYR_W / 2, by = p->pos.y + EYEDOWN, bz = p->pos.z + PLYR_W / 2;
        float dx = bx - ax, dy = by - ay, dz = bz - az;
        float len = DIST(dx, dy, dz);
        if (len < 1) return 1;
        for (float d = BS / 2.f; d < len; d += BS / 2.f)
        {
                float t = d / len;
                int tx = (ax + dx * t) / BS, ty = (ay + dy * t) / BS, tz = (az + dz * t) / BS;
                if (legit_tile(tx, ty, tz) && IS_SOLID(tx, ty, tz))
                        return 0;
        }
        return 1;
}

// nearest slime this one could merge with: alive, not busy, not already full,
// within sight. Full (size-12) slimes are ignored - nobody chases them and
// they chase nobody. Returns an index into mob[], or -1 if there's no partner.
static int mob_merge_target(int mi, struct mob *m)
{
        if (m->merge_cooldown || m->size >= MOB_MAXSIZE || m->hp <= 0) return -1;
        int best = -1;
        float bestd = (float)MOB_MERGE_SIGHT * MOB_MERGE_SIGHT;
        for (int j = 0; j < NR_MOBS; j++)
        {
                if (j == mi) continue;
                struct mob *o = &mob[j];
                if (!o->alive || o->dying || o->size >= MOB_MAXSIZE || o->hp <= 0) continue;
                float dx = o->pos.x - m->pos.x;
                float dy = o->pos.y - m->pos.y;
                float dz = o->pos.z - m->pos.z;
                float d2 = DIST_SQ(dx, dy, dz);
                if (d2 < bestd) { bestd = d2; best = j; }
        }
        return best;
}

// world-space y of the water surface in this slime's column, or -1 if none is
// near. y grows downward, so the surface is the top face of the highest water
// block spanning the slime; we scan from just above its head down past its feet.
static float mob_water_surface(struct mob *m)
{
        int bx = (m->pos.x + m->pos.w / 2) / BS;
        int bz = (m->pos.z + m->pos.d / 2) / BS;
        int y0 = m->pos.y / BS - 1;
        int y1 = (m->pos.y + m->pos.h) / BS + 1;
        for (int by = y0; by <= y1; by++)
                if (legit_tile(bx, by, bz) && T_(bx, by, bz) == WATR)
                        return by * BS; // top face of the topmost water block
        return -1;
}

void update_mobs()
{
        struct player *p = &player[0];

        mob_punch();

        // trickle fresh slimes in around the player, but only up to the
        // ambient target - shatter bursts push the live count way past it and
        // we let those thin back out on their own (via merges and decay)
        static int spawn_timer;
        if (mob_enable && --spawn_timer <= 0)
        {
                spawn_timer = 120;
                int alive = 0;
                for (int i = 0; i < NR_MOBS; i++) if (mob[i].alive) alive++;
                if (alive < MOB_POP_TARGET)
                {
                        unsigned seed = mob_seed;
                        float ang = RANDF(0, TAU);
                        float dist = RANDF(30, 80);
                        mob_seed = seed;
                        mob_spawn((p->pos.x + sinf(ang) * dist * BS) / BS,
                                  (p->pos.z + cosf(ang) * dist * BS) / BS);
                }
        }

        for (int i = 0; i < NR_MOBS; i++)
        {
                struct mob *m = &mob[i];
                if (!m->alive) continue;
                m->prev = m->pos;
                m->prev_yaw = m->yaw;

                float dx = (p->pos.x + PLYR_W/2) - (m->pos.x + m->pos.w/2);
                float dy = p->pos.y - m->pos.y;
                float dz = (p->pos.z + PLYR_W/2) - (m->pos.z + m->pos.d/2);
                float dist = DIST(dx, dy, dz);

                if (dist > MOB_DESPAWN || m->pos.y > TILESH * BS + 6000)
                {
                        m->alive = 0;
                        continue;
                }

                if (m->hurt) m->hurt--;
                if (m->bonk_cooldown) m->bonk_cooldown--;
                if (m->merge_cooldown) m->merge_cooldown--;
                if (m->dying && --m->dying == 0)
                {
                        m->alive = 0;
                        continue;
                }

                // waste away: each tick of the timer they drop a size, and half
                // the time that lost bit pops off as a fresh size-1 shard (the
                // other half it just evaporates). Small slimes do this to keep a
                // shattered swarm from breeding without bound; out of HP (can't
                // merge, so doomed) or soaking in water melts them 10x faster.
                int melting = m->hp <= 0 || m->wet;
                if ((m->size < MOB_DECAY || melting) && !m->dying)
                {
                        int interval = melting ? MOB_DOOMED_FRAMES : MOB_DECAY_FRAMES;
                        // if a long pending timer would stall a now-melting slime,
                        // pull it into the fast range (staggered, not synced)
                        if (m->decay_timer > 2 * interval)
                                m->decay_timer = interval + (i * 37 + frame) % interval;
                        if (m->decay_timer > 0) m->decay_timer--;
                        else
                        {
                                unsigned seed = mob_seed + i * 7 + frame;
                                int eject = RANDBOOL;
                                float a = RANDF(0, TAU);
                                // randomize the next interval (1x..2x) so decays
                                // stay out of lockstep across the swarm
                                m->decay_timer = interval + RANDI(0, interval);
                                mob_seed = seed;
                                if (eject) mob_spawn_shard(m->pos, a, i, m->hp);
                                if (m->size <= 1)
                                        m->dying = MOB_DEATH_FRAMES; // last bit gone
                                else
                                        mob_set_size(m, m->size - 1);
                        }
                }

                m->wet = world_collide(m->pos, 1);

                // pick a heading + gait when the rest timer runs out. where it
                // goes depends entirely on size:
                //   size >= 8      dangerous - charges the player on sight
                //   size 4..7      skittish  - bolts away from the player
                //   size 1..3      oblivious - never reacts to the player
                // and whenever it isn't chasing or fleeing, any not-yet-full
                // slime drifts toward the nearest partner to merge back up.
                // how it travels also scales with size: little ones only skate
                // along the ground, big ones only hop, mid ones do a bit of both.
                // everything bigger than a single-size shard moves at half pace
                float spd = m->size > 1 ? 0.5f : 1.f;
                if ((m->ground || m->wet) && !m->dying)
                {
                        if (m->hop_cooldown) m->hop_cooldown--;
                        else
                        {
                                unsigned seed = mob_seed + i;
                                float ang;
                                int face_player = 1;
                                // the player registers only inside the forward
                                // cone with a clear line of sight
                                int sees = dist < MOB_AGGRO
                                        && mob_sees_ahead(m, dx, dz)
                                        && mob_los_clear(m, p);
                                if (m->size >= MOB_DANGER && sees)
                                {
                                        ang = atan2f(dx, dz) + RANDF(-.3f, .3f);
                                        m->hop_cooldown = RANDI(20, 40);
                                }
                                else if (m->size >= MOB_FLEE && sees)
                                {
                                        // flee: head the opposite way, but keep
                                        // eyeing the player so it doesn't blindly
                                        // back into them
                                        ang = atan2f(-dx, -dz) + RANDF(-.3f, .3f);
                                        m->hop_cooldown = RANDI(15, 30);
                                        m->target_yaw = atan2f(dx, -dz);
                                        face_player = 0;
                                }
                                else
                                {
                                        int tgt = mob_merge_target(i, m);
                                        if (tgt >= 0)
                                        {
                                                float mx = mob[tgt].pos.x - m->pos.x;
                                                float mz = mob[tgt].pos.z - m->pos.z;
                                                ang = atan2f(mx, mz) + RANDF(-.2f, .2f);
                                                m->hop_cooldown = RANDI(15, 35);
                                        }
                                        else
                                        {
                                                ang = RANDF(0, TAU);
                                                m->hop_cooldown = RANDI(40, 180);
                                        }
                                }
                                // hop vs skate by size: p(hop) ramps from 0 at
                                // size<=4 to 1 at size>=8 (5:.25 6:.5 7:.75)
                                int hop = RANDF(0, 1.f) < (m->size - 4) / 4.f;
                                mob_seed = seed;
                                m->move_yaw = ang;
                                m->skating = !hop;
                                if (hop)
                                {
                                        m->vel.x = sinf(ang) * MOB_HOP * spd;
                                        m->vel.z = cosf(ang) * MOB_HOP * spd;
                                        m->grav = GRAV_JUMP + 4; // little hop
                                }
                                // (a skater just refreshes its heading here and
                                //  keeps gliding via the steering step below)
                                // face the way it moved, unless fleeing (then it
                                // stays turned toward the player, set above)
                                if (face_player)
                                        m->target_yaw = atan2f(sinf(ang), -cosf(ang));
                        }
                }

                // skaters glide like they're on ice: thrust along the heading
                // but keep the momentum already built up, capped at cruise speed.
                // turning doesn't snap the velocity around - the slime keeps
                // drifting the old way and has to skate against it to come about
                if (m->skating && (m->ground || m->wet) && !m->dying)
                {
                        m->vel.x = m->vel.x * MOB_SKATE_DRIFT + sinf(m->move_yaw) * MOB_SKATE_ACCEL * spd;
                        m->vel.z = m->vel.z * MOB_SKATE_DRIFT + cosf(m->move_yaw) * MOB_SKATE_ACCEL * spd;
                        float sp = sqrtf(m->vel.x * m->vel.x + m->vel.z * m->vel.z);
                        float cap = MOB_SKATE_SPD * spd;
                        if (sp > cap)
                        {
                                m->vel.x *= cap / sp;
                                m->vel.z *= cap / sp;
                        }
                }

                // hops travel only mid-air; skaters glide along the ground/water
                if (!m->ground || m->skating)
                        move_box(&m->pos, m->vel.x, 0, m->vel.z);

                // vertical motion: bob in open water, else plain gravity.
                float surf;
                if (m->wet && !m->ground && (surf = mob_water_surface(m)) >= 0)
                {
                        // ride the surface at about half-submerged, but let the
                        // rest depth gently rise and fall so the slime keeps
                        // bobbing on the water (each slime phase-shifted by i)
                        float frac = (m->pos.y + m->pos.h - surf) / m->pos.h;
                        if (frac < 0) frac = 0;
                        if (frac > 1) frac = 1;
                        float target = 0.5f + MOB_BOB_AMPL * sinf(frame * MOB_BOB_RATE + i);
                        m->bob = (m->bob + (target - frac) * m->pos.h * MOB_BUOY_K) * MOB_BUOY_DAMP;
                        move_box(&m->pos, 0, m->bob, 0);
                        m->grav = GRAV_ZERO; // keep the fall system neutral while afloat
                }
                else
                {
                        m->bob = 0;
                        // gravity, straight from the player's rulebook
                        if (!m->ground || m->grav < GRAV_ZERO)
                        {
                                if (!move_box(&m->pos, 0, gravity[m->grav] / (m->wet ? 3 : 1), 0))
                                        m->grav = GRAV_ZERO;
                                else if (m->grav < GRAV_MAX)
                                        m->grav++;
                        }
                }

                struct box foot = (struct box){
                        m->pos.x, m->pos.y + m->pos.h, m->pos.z,
                        m->pos.w, 1, m->pos.d};
                m->ground = world_collide(foot, 0);
                if (m->ground)
                {
                        m->grav = GRAV_ZERO;
                        if (!m->skating)
                                m->vel.x = m->vel.z = 0; // a hop lands with a splat
                        // a skater keeps its steered velocity and glides on
                }

                // merge: when two not-yet-full slimes touch and both are rested
                // from their last split, the pair fuses. This one keeps its slot
                // and grows; the partner is consumed. Anything past size 12 is
                // forfeited (12 + 5 still lands on 12, not 17).
                if (!m->dying && !m->merge_cooldown && m->size < MOB_MAXSIZE && m->hp > 0)
                {
                        for (int j = 0; j < NR_MOBS; j++)
                        {
                                if (j == i) continue;
                                struct mob *o = &mob[j];
                                if (!o->alive || o->dying || o->merge_cooldown) continue;
                                if (o->size >= MOB_MAXSIZE || o->hp <= 0) continue;
                                if (!collide(m->pos, o->pos)) continue;
                                mob_set_size(m, m->size + o->size); // clamps at 12
                                m->hp = MAX(m->hp, o->hp);          // keep the best vitality
                                o->alive = 0;
                                m->merge_cooldown = 30; // settle before fusing again
                                m->hurt = 6;            // little flash to show it
                                break;
                        }
                }

                // bonk: shove the player away and pop them off their feet.
                // only the big dangerous slimes (size >= 8) can land a hit;
                // smaller ones are harmless and just pass on by
                if (!m->dying && !m->bonk_cooldown && m->size >= MOB_DANGER
                                && collide(p->pos, m->pos))
                {
                        float hd = sqrtf(dx * dx + dz * dz);
                        if (hd > 1)
                        {
                                // the knock direction in the player's
                                // forward/right terms, since that's how
                                // update_player integrates velocity
                                float nx = dx / hd, nz = dz / hd;
                                float fwdx = sinf(p->yaw), fwdz = cosf(p->yaw);
                                p->fvel = (nx * fwdx + nz * fwdz) * PLYR_SPD_R * 4;
                                p->rvel = (nx * fwdz - nz * fwdx) * PLYR_SPD_R * 4;
                                p->grav = GRAV_JUMP + 5;
                                m->bonk_cooldown = 45;

                                // and the slime rebounds away instead of
                                // lodging inside the player - it hops back a
                                // little and lands a short distance off, but
                                // keeps facing the player so it still sees them
                                m->vel.x = -nx * MOB_HOP * 1.5f;
                                m->vel.z = -nz * MOB_HOP * 1.5f;
                                m->grav = GRAV_JUMP + 5;
                                m->ground = 0; // launch: it only moves airborne
                                m->skating = 0; // the rebound is a hop, not a glide
                                m->hop_cooldown = 25;
                                m->target_yaw = atan2f(dx, -dz);
                        }
                }

                // ease the visible facing toward the target a bit each tick
                float turn = m->target_yaw - m->yaw;
                while (turn >  PI) turn -= TAU;
                while (turn < -PI) turn += TAU;
                if (turn >  0.35f) turn =  0.35f;
                if (turn < -0.35f) turn = -0.35f;
                m->yaw += turn;
                if (m->yaw >  PI) m->yaw -= TAU;
                if (m->yaw < -PI) m->yaw += TAU;
        }
}

// draw the slimes with the terrain pipeline, which must already be bound:
// body cube first, then the eyes at 1/4 scale from the same origin
// mob geometry is built once per frame (mob_build) then drawn into as many
// passes as needed (mob_render): the main scene plus the near shadow cascade
static struct allocation mob_alloc[MAX_FRAMES_IN_FLIGHT];
static struct vbufv mbuf[NR_MOBS * 8]; // 6 body faces + 1 eyes overlay each
// bs is the slime's full body width (from its size); scale is the extra
// death-shrink factor on top of that (1.0 when alive)
static struct { float x, y, z, bs, scale, yaw, cx, cz; int start; } mob_inst[NR_MOBS];
static int mob_count;

void mob_build()
{
        struct vbufv *b = mbuf;
        mob_count = 0;

        for (int i = 0; i < NR_MOBS; i++)
        {
                struct mob *m = &mob[i];
                if (!m->alive) continue;

                float px = lerp(mob_lerp_t, m->prev.x, m->pos.x);
                float py = lerp(mob_lerp_t, m->prev.y, m->pos.y);
                float pz = lerp(mob_lerp_t, m->prev.z, m->pos.z);

                // the body cube's size follows the slime's own size; dying
                // slimes then shrink that further into the ground where they stand
                float bw = m->pos.w, bh = m->pos.h;
                float s = m->dying ? (float)m->dying / MOB_DEATH_FRAMES : 1.f;
                mob_inst[mob_count].x = px + (bw - bw * s) / 2;
                mob_inst[mob_count].y = py + (bh - bh * s);
                mob_inst[mob_count].z = pz + (bw - bw * s) / 2;
                mob_inst[mob_count].bs = bw;
                mob_inst[mob_count].scale = s;

                // interpolate the facing along the shortest arc, and pin the
                // rotation to the body's vertical center axis
                float dyaw = m->yaw - m->prev_yaw;
                while (dyaw >  PI) dyaw -= TAU;
                while (dyaw < -PI) dyaw += TAU;
                mob_inst[mob_count].yaw = m->prev_yaw + dyaw * mob_lerp_t;
                mob_inst[mob_count].cx = px + bw / 2;
                mob_inst[mob_count].cz = pz + bw / 2;
                mob_inst[mob_count].start = b - mbuf;

                // light the whole body from the block the head is in
                float il = 0.4f, gl = 0.f;
                int bx = (px + bw/2) / BS, by = py / BS, bz = (pz + bw/2) / BS;
                if (legit_tile(bx, by, bz))
                {
                        il = CORN_(bx, by, bz);
                        gl = KORN_(bx, by, bz);
                }
                if (m->hurt || m->dying) gl = 1.5f; // white-hot hurt flash

                *b++ = (struct vbufv){ TEX_SLIME_BODY,    UP, 0, 0, 0, il,il,il,il, gl,gl,gl,gl, 1 };
                *b++ = (struct vbufv){ TEX_SLIME_BODY, SOUTH, 0, 0, 0, il,il,il,il, gl,gl,gl,gl, 1 };
                *b++ = (struct vbufv){ TEX_SLIME_BODY, NORTH, 0, 0, 0, il,il,il,il, gl,gl,gl,gl, 1 };
                *b++ = (struct vbufv){ TEX_SLIME_BODY,  WEST, 0, 0, 0, il,il,il,il, gl,gl,gl,gl, 1 };
                *b++ = (struct vbufv){ TEX_SLIME_BODY,  EAST, 0, 0, 0, il,il,il,il, gl,gl,gl,gl, 1 };
                *b++ = (struct vbufv){ TEX_SLIME_BODY,  DOWN, 0, 0, 0, il,il,il,il, gl,gl,gl,gl, 1 };

                // eyes: a full-face quad over the front (south) face, textured
                // with a transparent-bordered eyes layer (the frag shader
                // discards the clear part). Nudged out along -z so it doesn't
                // z-fight the body face; the shader spins it to face the heading.
                *b++ = (struct vbufv){ TEX_SLIME_EYES, SOUTH, 0, 0, -MOB_EYE_OUT, il,il,il,il, gl,gl,gl,gl, 1 };
                polys += 7;
                mob_count++;
        }

        if (!mob_count) return;

        int fr = vk.currentFrame;
        if (!mob_alloc[fr].buf)
                vulkan_allocate_vertex_buffer(sizeof mbuf, &mob_alloc[fr]);
        vulkan_populate_vertex_buffer(mbuf, (b - mbuf) * sizeof *mbuf, &mob_alloc[fr]);
}

// draw the built mobs with the given pipeline (already bound) and pv matrix
void mob_render(VkCommandBuffer cmdbuf, int pipe, float *pv)
{
        if (!mob_count) return;

        int fr = vk.currentFrame;
        struct { float pv[16]; float x, y, z, bs;
                 float yaw, cx, cz, shiny; } push;
        memcpy(push.pv, pv, sizeof push.pv);
        push.shiny = 1; // slimes are wet and glossy

        for (int i = 0; i < mob_count; i++)
        {
                push.x = mob_inst[i].x;
                push.y = mob_inst[i].y;
                push.z = mob_inst[i].z;
                push.yaw = mob_inst[i].yaw;
                push.cx = mob_inst[i].cx;
                push.cz = mob_inst[i].cz;

                VkDeviceSize off = mob_inst[i].start * sizeof(struct vbufv);
                push.bs = mob_inst[i].bs * mob_inst[i].scale;
                vkCmdPushConstants(cmdbuf, vk.pipelines[pipe].layout,
                        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof push, &push);
                vkCmdBindVertexBuffers(cmdbuf, 0, 1, &mob_alloc[fr].buf, &off);
                vkCmdDraw(cmdbuf, 4, 6, 0, 0);

                // eyes overlay: one full-face quad at body scale, just in front
                off += 6 * sizeof(struct vbufv);
                vkCmdBindVertexBuffers(cmdbuf, 0, 1, &mob_alloc[fr].buf, &off);
                vkCmdDraw(cmdbuf, 4, 1, 0, 0);
        }
}

#endif // BLOCKO_MOB_C_INCLUDED

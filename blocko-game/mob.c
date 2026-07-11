#include "blocko.c"
#ifndef BLOCKO_MOB_C_INCLUDED
#define BLOCKO_MOB_C_INCLUDED

// mob.c - slimes: hopping enemies that chase and bonk the player
//
// A slime is a single green cube drawn with the terrain pipeline - each one
// is a tiny one-cube "chunk" placed and scaled by push constants, plus two
// dark eye quads on whichever side it hopped toward. Slimes wander until the
// player gets close, then hop after them and knock them around on contact.
// One punch (left click) pops one.

#define MOB_W 950                 // the body cube nearly fills a block
#define MOB_H 950
#define MOB_HOP 130               // horizontal hop speed, units per frame
#define MOB_BUOY_K 0.06f          // buoyancy spring stiffness toward the rest depth
#define MOB_BUOY_DAMP 0.85f       // damping on the bob spring
#define MOB_BOB_AMPL 0.12f        // bob height as a fraction of the body
#define MOB_BOB_RATE 0.09f        // bob speed (radians/frame; ~1s up-and-down)
#define MOB_AGGRO (16*BS)         // notice the player from this far
#define MOB_DESPAWN (100*BS)      // forget mobs that get this far from the host
#define MOB_LEASH (64*BS)         // a remote player's sim area guarantees only
                                  // this much terrain around them; past it a
                                  // mob is off the map and despawns
#define MOB_REACH (4*BS)          // how far the player can punch
#define MOB_DEATH_FRAMES 15       // shrink-out animation length
#define MOB_POP_TARGET 6          // ambient spawns stop once this many are alive

#define TEX_SLIME_BODY 35         // texture-array layers for the slime skin
#define TEX_SLIME_EYES 36         // full-face eyes overlay, transparent around the eyes
#define MOB_EYE_OUT 0.02f         // push the eyes this far off the face (kills z-fighting)

unsigned mob_seed = 60659;

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

        int gnd = sim_gndh(bx, bz); // 0 where no terrain exists yet
        if (gnd < 2 || gnd >= TILESH) return 0;
        // never spawn in or over water: check the surface and the block the
        // body will occupy, and refuse anything at/below the waterline
        if (gnd >= SEA_LEVEL) return 0;
        if (sim_tile(bx, gnd, bz) == WATR || sim_tile(bx, gnd - 1, bz) == WATR) return 0;

        *m = (struct mob){
                .pos = { bx * BS + (BS - MOB_W) / 2, gnd * BS - MOB_H - 1,
                         bz * BS + (BS - MOB_W) / 2, MOB_W, MOB_H, MOB_W },
                .alive = 1,
                .grav = GRAV_ZERO,
                .yaw = 0, .prev_yaw = 0, .target_yaw = 0, // yaw 0 faces -z (south)
                .hop_cooldown = 30,
        };

        if (world_collide(m->pos, 0)) { m->alive = 0; return 0; }
        m->prev = m->pos;
        return 1;
}

// a click aimed at a mob punches it instead of mining the block behind it
void mob_punch()
{
        struct player *p = &player[my_player];
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

                        if (net_mode == NET_CLIENT)
                                net_send_punch(i); // the server resolves it
                        else
                                mob_kill(i);
                        // can't mine through a mob: cancel any dig update_player
                        // just started on the block behind it
                        mine_heal();
                        p->cooldown = 10; // this click is spent
                        return;
                }
        }
}

// one punch pops the slime: it shrinks out over a few frames. The
// authoritative kill: runs on the host (or single-player); a client's punch
// arrives here through MSG_PUNCH.
void mob_kill(int i)
{
        struct mob *m = &mob[i];
        if (!m->alive || m->dying) return;
        m->dying = MOB_DEATH_FRAMES;
        mob_kills++;
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
                if (sim_tile(tx, ty, tz) < LASTSOLID)
                        return 0;
        }
        return 1;
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
                if (sim_tile(bx, by, bz) == WATR)
                        return by * BS; // top face of the topmost water block
        return -1;
}

void update_mobs()
{
        mob_punch();

        // the server owns the mobs: clients only originate punches (above) and
        // receive positions through net.c's snapshots
        if (net_mode == NET_CLIENT)
                return;

        // trickle fresh slimes in around every simulated player - the host's
        // own body plus each connected player's ghost (a headless server's
        // player is disembodied and draws no slimes). Each anchor gets its
        // own share of the ambient target: a slime counts against whichever
        // anchor it's nearest. Shatter bursts push counts past the target
        // and thin back out on their own (via merges and decay)
        static int spawn_timer;
        if (mob_enable && --spawn_timer <= 0)
        {
                spawn_timer = 120;
                struct player *anch[NR_PLAYERS];
                int na = 0;
                if (!headless)
                        anch[na++] = &player[my_player];
                for (int j = 0; j < NR_PLAYERS; j++)
                        if (j != my_player && net_player_active(j))
                                anch[na++] = &player[j];

                int pop[NR_PLAYERS] = {0};
                for (int i = 0; na && i < NR_MOBS; i++)
                {
                        if (!mob[i].alive) continue;
                        int best = 0;
                        float bestd = 1e30f;
                        for (int a = 0; a < na; a++)
                        {
                                float ax = (anch[a]->pos.x + PLYR_W/2) - (mob[i].pos.x + mob[i].pos.w/2);
                                float az = (anch[a]->pos.z + PLYR_W/2) - (mob[i].pos.z + mob[i].pos.d/2);
                                float d2 = ax * ax + az * az;
                                if (d2 < bestd) { bestd = d2; best = a; }
                        }
                        pop[best]++;
                }

                for (int a = 0; a < na; a++)
                {
                        if (pop[a] >= MOB_POP_TARGET) continue;
                        unsigned seed = mob_seed;
                        float ang = RANDF(0, TAU);
                        // spawn ring: remote anchors keep it inside their sim
                        // area's guaranteed coverage (the leash), with margin
                        float dist = anch[a] == &player[my_player]
                                ? RANDF(30, 80) : RANDF(20, 56);
                        mob_seed = seed;
                        mob_spawn((anch[a]->pos.x + sinf(ang) * dist * BS) / BS,
                                  (anch[a]->pos.z + cosf(ang) * dist * BS) / BS);
                }
        }

        for (int i = 0; i < NR_MOBS; i++)
        {
                struct mob *m = &mob[i];
                if (!m->alive) continue;
                m->prev = m->pos;
                m->prev_yaw = m->yaw;

                // this slime minds the nearest player: the host's own body, or
                // a connected player's ghost (their position, synced at 20Hz)
                struct player *p = &player[my_player];
                int pi = my_player;
                float dx = (p->pos.x + PLYR_W/2) - (m->pos.x + m->pos.w/2);
                float dy = p->pos.y - m->pos.y;
                float dz = (p->pos.z + PLYR_W/2) - (m->pos.z + m->pos.d/2);
                float dist = DIST(dx, dy, dz);
                if (headless) dist = 1e30f; // disembodied body: no aggro, no leash
                // the leash: a slime lives only near someone simulating
                // terrain under it - within MOB_DESPAWN of the host's body
                // (the whole window is real ground) or within MOB_LEASH of a
                // connected player (their sim area guarantees only that much)
                int leashed = dist <= MOB_DESPAWN;
                for (int j = 0; j < NR_PLAYERS; j++)
                {
                        if (j == my_player || !net_player_active(j)) continue;
                        float jx = (player[j].pos.x + PLYR_W/2) - (m->pos.x + m->pos.w/2);
                        float jy = player[j].pos.y - m->pos.y;
                        float jz = (player[j].pos.z + PLYR_W/2) - (m->pos.z + m->pos.d/2);
                        float jd = DIST(jx, jy, jz);
                        // leash horizontally: the area covers the whole
                        // column, and a falling or mountaintop player
                        // shouldn't shed the slimes in the valley below
                        if (jx * jx + jz * jz <= (float)MOB_LEASH * MOB_LEASH)
                                leashed = 1;
                        if (jd < dist)
                        {
                                p = &player[j];
                                pi = j;
                                dx = jx; dy = jy; dz = jz; dist = jd;
                        }
                }

                if (!leashed || m->pos.y > TILESH * BS + 6000)
                {
                        m->alive = 0;
                        continue;
                }

                if (m->bonk_cooldown) m->bonk_cooldown--;
                if (m->dying && --m->dying == 0)
                {
                        m->alive = 0;
                        continue;
                }

                m->wet = world_collide(m->pos, 1);

                // pick a heading when the rest timer runs out: charge the
                // player on sight, otherwise wander
                if ((m->ground || m->wet) && !m->dying)
                {
                        if (m->hop_cooldown) m->hop_cooldown--;
                        else
                        {
                                unsigned seed = mob_seed + i;
                                float ang;
                                // the player registers only inside the forward
                                // cone with a clear line of sight
                                int sees = dist < MOB_AGGRO
                                        && mob_sees_ahead(m, dx, dz)
                                        && mob_los_clear(m, p);
                                if (sees)
                                {
                                        ang = atan2f(dx, dz) + RANDF(-.3f, .3f);
                                        m->hop_cooldown = RANDI(20, 40);
                                }
                                else
                                {
                                        ang = RANDF(0, TAU);
                                        m->hop_cooldown = RANDI(40, 180);
                                }
                                mob_seed = seed;
                                m->vel.x = sinf(ang) * MOB_HOP * 0.5f;
                                m->vel.z = cosf(ang) * MOB_HOP * 0.5f;
                                m->grav = GRAV_JUMP + 4; // little hop
                                m->target_yaw = atan2f(sinf(ang), -cosf(ang));
                        }
                }

                // hops travel only mid-air
                if (!m->ground)
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
                        m->vel.x = m->vel.z = 0; // a hop lands with a splat
                }

                // bonk: shove the player away and pop them off their feet
                if (!m->dying && !m->bonk_cooldown && collide(p->pos, m->pos))
                {
                        float hd = sqrtf(dx * dx + dz * dz);
                        if (hd > 1)
                        {
                                float nx = dx / hd, nz = dz / hd;
                                if (pi == my_player)
                                {
                                        // the knock direction in the player's
                                        // forward/right terms, since that's how
                                        // update_player integrates velocity
                                        float fwdx = sinf(p->yaw), fwdz = cosf(p->yaw);
                                        p->fvel = (nx * fwdx + nz * fwdz) * PLYR_SPD_R * 4;
                                        p->rvel = (nx * fwdz - nz * fwdx) * PLYR_SPD_R * 4;
                                        p->grav = GRAV_JUMP + 5;
                                }
                                else
                                        net_send_bonk(pi, nx, nz); // their machine applies it
                                m->bonk_cooldown = 45;

                                // and the slime rebounds away instead of
                                // lodging inside the player - it hops back a
                                // little and lands a short distance off, but
                                // keeps facing the player so it still sees them
                                m->vel.x = -nx * MOB_HOP * 1.5f;
                                m->vel.z = -nz * MOB_HOP * 1.5f;
                                m->grav = GRAV_JUMP + 5;
                                m->ground = 0; // launch: it only moves airborne
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
// slimes are 6 body faces + 1 eyes overlay each; remote players ride along as
// three cubes (head with eyes, torso, legs)
static struct vbufv mbuf[NR_MOBS * 8 + NR_PLAYERS * 20];
// bs is the slime's full body width (from its size); scale is the extra
// death-shrink factor on top of that (1.0 when alive)
static struct { float x, y, z, bs, scale, yaw, cx, cz; int start, eyes; }
        mob_inst[NR_MOBS + NR_PLAYERS * 3];
static int mob_count;

// one textured cube instance for a remote player's body part, lit like a mob
static struct vbufv *netplayer_cube(struct vbufv *b, float x, float y, float z,
                float size, float yaw, float cx, float cz, int tex, int eyes,
                float il)
{
        mob_inst[mob_count].x = x;
        mob_inst[mob_count].y = y;
        mob_inst[mob_count].z = z;
        mob_inst[mob_count].bs = size;
        mob_inst[mob_count].scale = 1.f;
        mob_inst[mob_count].yaw = yaw;
        mob_inst[mob_count].cx = cx;
        mob_inst[mob_count].cz = cz;
        mob_inst[mob_count].start = b - mbuf;
        mob_inst[mob_count].eyes = eyes;

        *b++ = (struct vbufv){ tex,    UP, 0, 0, 0, il,il,il,il, 1 };
        *b++ = (struct vbufv){ tex, SOUTH, 0, 0, 0, il,il,il,il, 1 };
        *b++ = (struct vbufv){ tex, NORTH, 0, 0, 0, il,il,il,il, 1 };
        *b++ = (struct vbufv){ tex,  WEST, 0, 0, 0, il,il,il,il, 1 };
        *b++ = (struct vbufv){ tex,  EAST, 0, 0, 0, il,il,il,il, 1 };
        *b++ = (struct vbufv){ tex,  DOWN, 0, 0, 0, il,il,il,il, 1 };
        if (eyes)
                *b++ = (struct vbufv){ TEX_SLIME_EYES, SOUTH, 0, 0, -MOB_EYE_OUT, il,il,il,il, 1 };
        mob_count++;
        return b;
}

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
                mob_inst[mob_count].eyes = 1;

                // light the whole body from the block the head is in
                float il = 0.4f;
                int bx = (px + bw/2) / BS, by = py / BS, bz = (pz + bw/2) / BS;
                if (legit_tile(bx, by, bz))
                        il = CORN_(bx, by, bz);
                if (m->dying) il = 1.5f; // white-hot death flash

                *b++ = (struct vbufv){ TEX_SLIME_BODY,    UP, 0, 0, 0, il,il,il,il, 1 };
                *b++ = (struct vbufv){ TEX_SLIME_BODY, SOUTH, 0, 0, 0, il,il,il,il, 1 };
                *b++ = (struct vbufv){ TEX_SLIME_BODY, NORTH, 0, 0, 0, il,il,il,il, 1 };
                *b++ = (struct vbufv){ TEX_SLIME_BODY,  WEST, 0, 0, 0, il,il,il,il, 1 };
                *b++ = (struct vbufv){ TEX_SLIME_BODY,  EAST, 0, 0, 0, il,il,il,il, 1 };
                *b++ = (struct vbufv){ TEX_SLIME_BODY,  DOWN, 0, 0, 0, il,il,il,il, 1 };

                // eyes: a full-face quad over the front (south) face, textured
                // with a transparent-bordered eyes layer (the frag shader
                // discards the clear part). Nudged out along -z so it doesn't
                // z-fight the body face; the shader spins it to face the heading.
                *b++ = (struct vbufv){ TEX_SLIME_EYES, SOUTH, 0, 0, -MOB_EYE_OUT, il,il,il,il, 1 };
                mob_count++;
        }

        // remote players (net.c): head (sand, with eyes), torso (wood), and
        // legs (stone) stacked down from pos.y, all spun about the body axis
        for (int i = 0; net_mode != NET_OFF && i < NR_PLAYERS; i++)
        {
                if (!net_player_active(i)) continue;
                struct player *r = &player[i];
                float x = r->pos.x, y = r->pos.y, z = r->pos.z;
                float cx = x + PLYR_W / 2, cz = z + PLYR_W / 2;
                // the mob shader's yaw spins the south (eyes) face to the
                // heading; a player's forward (sin yaw, cos yaw) maps to PI - yaw
                float yaw = PI - r->yaw;

                float il = 0.4f;
                int bx = cx / BS, by = y / BS, bz = cz / BS;
                if (legit_tile(bx, by, bz))
                        il = CORN_(bx, by, bz);

                b = netplayer_cube(b, x + 100, y,        z + 100, 500, yaw, cx, cz,  6, 1, il);
                b = netplayer_cube(b, x,       y + 500,  z,       700, yaw, cx, cz, 14, 0, il);
                b = netplayer_cube(b, x + 50,  y + 1200, z + 50,  600, yaw, cx, cz,  5, 0, il);
        }

        if (!mob_count) return;

        int fr = vk.currentFrame;
        if (!mob_alloc[fr].buf)
                vulkan_allocate_vertex_buffer(sizeof mbuf, &mob_alloc[fr]);
        vulkan_populate_vertex_buffer(mbuf, (b - mbuf) * sizeof *mbuf, &mob_alloc[fr]);
}

// draw the built mobs on the mob pipeline (mob.vert / mob_shadow.vert). Binds
// the pipeline itself; the caller must rebind its terrain pipeline afterward.
void mob_render(VkCommandBuffer cmdbuf, int pipe, float *pv)
{
        if (!mob_count) return;

        int fr = vk.currentFrame;
        vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipelines[pipe].pipeline);

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

                if (!mob_inst[i].eyes) continue; // player torso/legs have no face

                // eyes overlay: one full-face quad at body scale, just in front
                off += 6 * sizeof(struct vbufv);
                vkCmdBindVertexBuffers(cmdbuf, 0, 1, &mob_alloc[fr].buf, &off);
                vkCmdDraw(cmdbuf, 4, 1, 0, 0);
        }
}

#endif // BLOCKO_MOB_C_INCLUDED

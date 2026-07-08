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

#define MOB_W 800                 // the body cube is this big
#define MOB_H 800
#define MOB_PIX (MOB_W/4)         // eye quads live on a 4x4 grid on the face
#define MOB_HP 3
#define MOB_HOP 130               // horizontal hop speed, units per frame
#define MOB_AGGRO (16*BS)         // notice the player from this far
#define MOB_DESPAWN (48*BS)       // forget mobs that get this far away
#define MOB_REACH (4*BS)          // how far the player can punch
#define MOB_DEATH_FRAMES 15       // shrink-out animation length

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

        if (bx < 1 || bx >= TILESW-1 || bz < 1 || bz >= TILESD-1) return 0;
        if (!AGEN_(B2C(bx), B2C(bz))) return 0; // no terrain here yet

        int gnd = GNDH_(bx, bz);
        if (gnd < 2 || gnd >= TILESH) return 0;
        if (T_(bx, gnd, bz) == WATR) return 0; // don't spawn swimming

        *m = (struct mob){
                .pos = { bx * BS + (BS - MOB_W) / 2, gnd * BS - MOB_H - 1,
                         bz * BS + (BS - MOB_W) / 2, MOB_W, MOB_H, MOB_W },
                .alive = 1,
                .hp = MOB_HP,
                .grav = GRAV_ZERO,
                .facing = SOUTH,
                .hop_cooldown = 30,
        };

        if (world_collide(m->pos, 0)) { m->alive = 0; return 0; }
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

                        m->hurt = 12;
                        m->vel.x = f0 * MOB_HOP * 2; // knocked away...
                        m->vel.z = f2 * MOB_HOP * 2;
                        m->grav = GRAV_JUMP + 6;     // ...and popped up
                        m->hop_cooldown = 60;
                        if (--m->hp <= 0)
                        {
                                m->dying = MOB_DEATH_FRAMES;
                                mob_kills++;
                        }
                        // can't mine through a mob: cancel any dig update_player
                        // just started on the block behind it
                        mine_heal();
                        p->cooldown = 10; // this click is spent
                        return;
                }
        }
}

void update_mobs()
{
        struct player *p = &player[0];

        mob_punch();

        // keep trying to fill the mob slots with slimes around the player
        static int spawn_timer;
        if (mob_enable && --spawn_timer <= 0)
        {
                spawn_timer = 120;
                unsigned seed = mob_seed;
                float ang = RANDF(0, TAU);
                float dist = RANDF(18, 30);
                mob_seed = seed;
                mob_spawn((p->pos.x + sinf(ang) * dist * BS) / BS,
                          (p->pos.z + cosf(ang) * dist * BS) / BS);
        }

        for (int i = 0; i < NR_MOBS; i++)
        {
                struct mob *m = &mob[i];
                if (!m->alive) continue;
                m->prev = m->pos;

                float dx = (p->pos.x + PLYR_W/2) - (m->pos.x + MOB_W/2);
                float dy = p->pos.y - m->pos.y;
                float dz = (p->pos.z + PLYR_W/2) - (m->pos.z + MOB_W/2);
                float dist = DIST(dx, dy, dz);

                if (dist > MOB_DESPAWN || m->pos.y > TILESH * BS + 6000)
                {
                        m->alive = 0;
                        continue;
                }

                if (m->hurt) m->hurt--;
                if (m->bonk_cooldown) m->bonk_cooldown--;
                if (m->dying && --m->dying == 0)
                {
                        m->alive = 0;
                        continue;
                }

                m->wet = world_collide(m->pos, 1);

                // hop when rested: at the player if close, else wherever
                if ((m->ground || m->wet) && !m->dying)
                {
                        if (m->hop_cooldown) m->hop_cooldown--;
                        else
                        {
                                unsigned seed = mob_seed + i;
                                float ang;
                                if (dist < MOB_AGGRO)
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
                                m->vel.x = sinf(ang) * MOB_HOP;
                                m->vel.z = cosf(ang) * MOB_HOP;
                                m->grav = GRAV_JUMP + 4; // little jump
                                m->facing = fabsf(m->vel.x) > fabsf(m->vel.z) ?
                                                (m->vel.x > 0 ? EAST  : WEST ) :
                                                (m->vel.z > 0 ? NORTH : SOUTH);
                        }
                }

                // slimes only travel mid-hop, never slide along the ground
                if (!m->ground)
                        move_box(&m->pos, m->vel.x, 0, m->vel.z);

                // gravity, straight from the player's rulebook
                if (!m->ground || m->grav < GRAV_ZERO)
                {
                        if (!move_box(&m->pos, 0, gravity[m->grav] / (m->wet ? 3 : 1), 0))
                                m->grav = GRAV_ZERO;
                        else if (m->grav < GRAV_MAX)
                                m->grav++;
                }

                struct box foot = (struct box){
                        m->pos.x, m->pos.y + MOB_H, m->pos.z,
                        MOB_W, 1, MOB_W};
                m->ground = world_collide(foot, 0);
                if (m->ground)
                {
                        m->grav = GRAV_ZERO;
                        m->vel.x = m->vel.z = 0; // slimes land with a splat
                }

                // bonk: shove the player away and pop them off their feet
                if (!m->dying && !m->bonk_cooldown && collide(p->pos, m->pos))
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
                        }
                }
        }
}

// draw the slimes with the terrain pipeline, which must already be bound:
// body cube first, then the eyes at 1/4 scale from the same origin
// mob geometry is built once per frame (mob_build) then drawn into as many
// passes as needed (mob_render): the main scene plus the near shadow cascade
static struct allocation mob_alloc[MAX_FRAMES_IN_FLIGHT];
static struct vbufv mbuf[NR_MOBS * 8]; // 6 body faces + 2 eyes each
static struct { float x, y, z, scale; int start; } mob_inst[NR_MOBS];
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

                // dying slimes shrink into the ground where they stand
                float s = m->dying ? (float)m->dying / MOB_DEATH_FRAMES : 1.f;
                mob_inst[mob_count].x = px + (MOB_W - MOB_W * s) / 2;
                mob_inst[mob_count].y = py + (MOB_H - MOB_H * s);
                mob_inst[mob_count].z = pz + (MOB_W - MOB_W * s) / 2;
                mob_inst[mob_count].scale = s;
                mob_inst[mob_count].start = b - mbuf;

                // light the whole body from the block the head is in
                float il = 0.4f, gl = 0.f;
                int bx = (px + MOB_W/2) / BS, by = py / BS, bz = (pz + MOB_W/2) / BS;
                if (legit_tile(bx, by, bz))
                {
                        il = CORN_(bx, by, bz);
                        gl = KORN_(bx, by, bz);
                }
                if (m->hurt || m->dying) gl = 1.5f; // white-hot hurt flash

                *b++ = (struct vbufv){ 0,    UP, 0, 0, 0, il,il,il,il, gl,gl,gl,gl, 1 };
                *b++ = (struct vbufv){ 0, SOUTH, 0, 0, 0, il,il,il,il, gl,gl,gl,gl, 1 };
                *b++ = (struct vbufv){ 0, NORTH, 0, 0, 0, il,il,il,il, gl,gl,gl,gl, 1 };
                *b++ = (struct vbufv){ 0,  WEST, 0, 0, 0, il,il,il,il, gl,gl,gl,gl, 1 };
                *b++ = (struct vbufv){ 0,  EAST, 0, 0, 0, il,il,il,il, gl,gl,gl,gl, 1 };
                *b++ = (struct vbufv){ 0,  DOWN, 0, 0, 0, il,il,il,il, gl,gl,gl,gl, 1 };

                // two dark eyes sitting just off the side it faces
                float o = m->facing; // reuse the face's own orientation
                float ex0 = 0.7f, ez0 = -0.02f, ex1 = 2.3f, ez1 = -0.02f;
                switch (m->facing)
                {
                        case NORTH: ez0 = ez1 = 3.02f;                     break;
                        case EAST:  ex0 = ex1 = 3.02f; ez0 = .7f; ez1 = 2.3f; break;
                        case WEST:  ex0 = ex1 = -.02f; ez0 = .7f; ez1 = 2.3f; break;
                }
                *b++ = (struct vbufv){ 2, o, ex0, 0.9f, ez0, 0,0,0,0, gl,gl,gl,gl, 1 };
                *b++ = (struct vbufv){ 2, o, ex1, 0.9f, ez1, 0,0,0,0, gl,gl,gl,gl, 1 };
                polys += 8;
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
        struct { float pv[16]; float x, y, z, bs; } push;
        memcpy(push.pv, pv, sizeof push.pv);

        for (int i = 0; i < mob_count; i++)
        {
                push.x = mob_inst[i].x;
                push.y = mob_inst[i].y;
                push.z = mob_inst[i].z;

                VkDeviceSize off = mob_inst[i].start * sizeof(struct vbufv);
                push.bs = MOB_W * mob_inst[i].scale;
                vkCmdPushConstants(cmdbuf, vk.pipelines[pipe].layout,
                        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof push, &push);
                vkCmdBindVertexBuffers(cmdbuf, 0, 1, &mob_alloc[fr].buf, &off);
                vkCmdDraw(cmdbuf, 4, 6, 0, 0);

                off += 6 * sizeof(struct vbufv);
                push.bs = MOB_PIX * mob_inst[i].scale;
                vkCmdPushConstants(cmdbuf, vk.pipelines[pipe].layout,
                        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof push, &push);
                vkCmdBindVertexBuffers(cmdbuf, 0, 1, &mob_alloc[fr].buf, &off);
                vkCmdDraw(cmdbuf, 4, 2, 0, 0);
        }
}

#endif // BLOCKO_MOB_C_INCLUDED

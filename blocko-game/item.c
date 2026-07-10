#include "blocko.c"
#ifndef BLOCKO_ITEM_C_INCLUDED
#define BLOCKO_ITEM_C_INCLUDED

// item.c - the little block that pops loose when you mine a block: it springs up
// with a small random velocity and spin, tumbles to the ground, and slides to a
// halt as friction bleeds off its speed. Purely cosmetic - you can't pick it up,
// it despawns after about a minute, and the oldest ones are recycled once the
// ring buffer fills. Physics borrows the mob's gravity rulebook; rendering is a
// half-size cube on the mob pipeline (mob.vert spins it about the vertical axis),
// built once per frame (item_build) and drawn per pass (item_render) - main pass
// and near shadow cascade.

#define NR_ITEMS 64
#define ITEM_LIFE (60 * 60)          // ~1 minute at 60 ticks/sec
#define ITEM_SIZE BS2                // half a block on every axis
#define ITEM_DRIFT 30                // top horizontal pop speed, units/tick
#define ITEM_SPIN 0.13f              // top angular velocity, radians/tick
#define ITEM_FRICTION 0.8f           // per-tick speed/spin retention on the ground

struct item {
        struct box pos;          // world-space AABB (an ITEM_SIZE cube)
        struct box prev;         // last tick, for smooth drawing above 60 FPS
        struct point vel;        // horizontal drift (x,z); vertical is gravity[]
        float yaw, prev_yaw;     // facing/spin about the vertical axis
        float angvel;            // angular velocity, radians/tick
        int tile;                // the block type it represents
        int grav;                // index into gravity[] (fall accumulator)
        int ground;              // resting on solid?
        int life;                // ticks until it despawns
        int alive;
};

static struct item items[NR_ITEMS];
static int item_next;            // ring-buffer write cursor (oldest slot)

// pop an item out of a just-mined block at world-tile (x,y,z). Overwrites the
// oldest ring slot when the buffer is full - that's the "enough other items"
// despawn path the spec asks for.
void item_spawn(int x, int y, int z, int tile)
{
        struct item *it = &items[item_next];
        item_next = (item_next + 1) % NR_ITEMS;

        unsigned seed = mob_seed;
        float ang = RANDF(0, TAU);            // a random compass heading
        float speed = RANDF(ITEM_DRIFT / 3.f, ITEM_DRIFT);

        // centered in the mined cell so it visibly springs out of its socket
        it->pos = (struct box){
                x * BS + (BS - ITEM_SIZE) / 2,
                y * BS + (BS - ITEM_SIZE) / 2,
                z * BS + (BS - ITEM_SIZE) / 2,
                ITEM_SIZE, ITEM_SIZE, ITEM_SIZE };
        it->prev = it->pos;
        it->vel.x = sinf(ang) * speed;        // tiny drift along the heading
        it->vel.z = cosf(ang) * speed;
        it->vel.y = 0;
        it->yaw = it->prev_yaw = ang;
        it->angvel = RANDF(-ITEM_SPIN, ITEM_SPIN);
        it->tile = tile;
        it->grav = RANDI(10, 13);             // a gentle upward nudge (gravity[] < 0)
        it->ground = 0;
        it->life = ITEM_LIFE;
        it->alive = 1;
        mob_seed = seed;
}

// tick every live item: age it out, drift/fall, spin, and shed speed to friction
// once it settles. The fall is straight from the mob's rulebook (mob.c).
void update_items()
{
        for (int i = 0; i < NR_ITEMS; i++)
        {
                struct item *it = &items[i];
                if (!it->alive) continue;

                if (--it->life <= 0) { it->alive = 0; continue; }

                it->prev = it->pos;
                it->prev_yaw = it->yaw;

                // horizontal drift, then gravity (or the leftover upward pop)
                move_box(&it->pos, (int)it->vel.x, 0, (int)it->vel.z);
                if (!it->ground || it->grav < GRAV_ZERO)
                {
                        if (!move_box(&it->pos, 0, gravity[it->grav], 0))
                                it->grav = GRAV_ZERO;     // blocked -> reset the fall
                        else if (it->grav < GRAV_MAX)
                                it->grav++;               // accelerate the fall
                }

                it->yaw += it->angvel;

                // a thin probe just beneath the item: are we resting on solid?
                struct box foot = (struct box){
                        it->pos.x, it->pos.y + it->pos.h, it->pos.z,
                        it->pos.w, 1, it->pos.d };
                it->ground = world_collide(foot, 0);
                if (it->ground)
                {
                        it->grav = GRAV_ZERO;
                        // friction: bleed off drift and spin so it slides to a stop
                        it->vel.x *= ITEM_FRICTION;
                        it->vel.z *= ITEM_FRICTION;
                        it->angvel *= ITEM_FRICTION;
                }
        }
}

// items live in window coords like the player and mobs; slide them along when
// the world scrolls so they stay glued to their blocks
void item_scoot(int dx, int dz)
{
        for (int i = 0; i < NR_ITEMS; i++)
        {
                items[i].pos.x  += dx * BS;  items[i].pos.z  += dz * BS;
                items[i].prev.x += dx * BS;  items[i].prev.z += dz * BS;
        }
}

// item geometry is built once per frame (item_build) then drawn into as many
// passes as needed (item_render): the main scene plus the near shadow cascade
static struct allocation item_alloc[MAX_FRAMES_IN_FLIGHT];
static struct vbufv ibuf[NR_ITEMS * 6];   // 6 cube faces each
static struct { float x, y, z, bs, yaw, cx, cz; int start; } item_inst[NR_ITEMS];
static int item_count;

void item_build()
{
        struct vbufv *b = ibuf;
        item_count = 0;

        for (int i = 0; i < NR_ITEMS; i++)
        {
                struct item *it = &items[i];
                if (!it->alive) continue;

                float px = lerp(mob_lerp_t, it->prev.x, it->pos.x);
                float py = lerp(mob_lerp_t, it->prev.y, it->pos.y);
                float pz = lerp(mob_lerp_t, it->prev.z, it->pos.z);
                float bw = it->pos.w;

                item_inst[item_count].x = px;
                item_inst[item_count].y = py;
                item_inst[item_count].z = pz;
                item_inst[item_count].bs = bw;

                // interpolate the spin along the shortest arc; pivot on the
                // vertical center axis so it twirls in place
                float dyaw = it->yaw - it->prev_yaw;
                while (dyaw >  PI) dyaw -= TAU;
                while (dyaw < -PI) dyaw += TAU;
                item_inst[item_count].yaw = it->prev_yaw + dyaw * mob_lerp_t;
                item_inst[item_count].cx = px + bw / 2;
                item_inst[item_count].cz = pz + bw / 2;
                item_inst[item_count].start = b - ibuf;

                // light the whole item from the block it currently sits in, the
                // same flat-shading mob.c uses for its body
                float il = 0.4f, gl = 0.f;
                int bx = (px + bw/2) / BS, by = (py + bw/2) / BS, bz = (pz + bw/2) / BS;
                if (legit_tile(bx, by, bz))
                {
                        il = CORN_(bx, by, bz);
                        gl = KORN_(bx, by, bz);
                }

                int t = it->tile;
                *b++ = (struct vbufv){ tile_face_tex(t,UP),    UP,    0,0,0, il,il,il,il, gl,gl,gl,gl, 1 };
                *b++ = (struct vbufv){ tile_face_tex(t,SOUTH), SOUTH, 0,0,0, il,il,il,il, gl,gl,gl,gl, 1 };
                *b++ = (struct vbufv){ tile_face_tex(t,NORTH), NORTH, 0,0,0, il,il,il,il, gl,gl,gl,gl, 1 };
                *b++ = (struct vbufv){ tile_face_tex(t,WEST),  WEST,  0,0,0, il,il,il,il, gl,gl,gl,gl, 1 };
                *b++ = (struct vbufv){ tile_face_tex(t,EAST),  EAST,  0,0,0, il,il,il,il, gl,gl,gl,gl, 1 };
                *b++ = (struct vbufv){ tile_face_tex(t,DOWN),  DOWN,  0,0,0, il,il,il,il, gl,gl,gl,gl, 1 };
                item_count++;
        }

        if (!item_count) return;

        int fr = vk.currentFrame;
        if (!item_alloc[fr].buf)
                vulkan_allocate_vertex_buffer(sizeof ibuf, &item_alloc[fr]);
        vulkan_populate_vertex_buffer(ibuf, (b - ibuf) * sizeof *ibuf, &item_alloc[fr]);
}

// draw the built items on the mob pipeline (mob.vert / mob_shadow.vert - it spins
// each cube about the vertical axis by push.yaw). Binds the pipeline itself; the
// caller must rebind its terrain pipeline afterward.
void item_render(VkCommandBuffer cmdbuf, int pipe, float *pv)
{
        if (!item_count) return;

        int fr = vk.currentFrame;
        vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipelines[pipe].pipeline);

        struct { float pv[16]; float x, y, z, bs;
                 float yaw, cx, cz, shiny; } push;
        memcpy(push.pv, pv, sizeof push.pv);
        push.shiny = 0; // blocks are matte, not glossy like slimes

        for (int i = 0; i < item_count; i++)
        {
                push.x = item_inst[i].x;
                push.y = item_inst[i].y;
                push.z = item_inst[i].z;
                push.bs = item_inst[i].bs;
                push.yaw = item_inst[i].yaw;
                push.cx = item_inst[i].cx;
                push.cz = item_inst[i].cz;
                vkCmdPushConstants(cmdbuf, vk.pipelines[pipe].layout,
                        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof push, &push);
                VkDeviceSize off = item_inst[i].start * sizeof(struct vbufv);
                vkCmdBindVertexBuffers(cmdbuf, 0, 1, &item_alloc[fr].buf, &off);
                vkCmdDraw(cmdbuf, 4, 6, 0, 0);
        }
}

#endif // BLOCKO_ITEM_C_INCLUDED

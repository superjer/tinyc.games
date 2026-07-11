#ifndef BLOCKO_PMODEL_C_INCLUDED
#define BLOCKO_PMODEL_C_INCLUDED
#include "blocko.c"

// Player models: up to 12 rectangular prisms, each 1-16 px per axis, living in
// a 16x16x16 px local space. A piece parents to another piece or to the player
// center box (a 16^3 space centered on the physical hitbox); its origin point
// is pinned to the attach point in the parent's space and it rotates there.
// The whole definition - geometry, 256-color palette, and 16x16 face tiles in
// a fixed box unwrap - is one flat integer struct (defs.c) that travels the
// net as raw bytes: each instance randomizes its own model at startup and
// peers exchange them via MSG_PMODEL (net.c).
//
// Face tiles ride as extra layers of the terrain texture array (16x16 RGBA,
// expanded from the palette), one 72-layer range per player slot, so
// pmodel.vert can share main.frag: lighting, fog, near-shadow sampling and
// the a<0.5 discard all come free. A model arriving at runtime re-uploads
// just its slot's layer range (update_texture_layers).

// world units per model px, chosen so the default head center (28 px above the
// feet: 12 leg + 12 body + half of the 8 head) sits exactly at the camera eye
#define PM_SCALE ((PLYR_H - EYEDOWN) / 28.f)

// default model: head, body, 2 arms, 2 legs, Minecraft-ish proportions.
// local spaces are y-down like the world; the body carries everything and
// hangs from the center box such that the feet land exactly on the ground
// (the box anchor is snapped to the px grid in pm_resolve to keep this integer)
enum { PM_BODY, PM_HEAD, PM_ARM_L, PM_ARM_R, PM_LEG_L, PM_LEG_R };
static struct pmodel pm_default = {
        .nr_pieces = 6,
        .piece = {
        //        dims       corner    origin     attach    parent   axes
        [PM_BODY]  = { {8,12,4}, {4,2,6}, {8,8,8},  {8,5,8},   -1,      0 },
        [PM_HEAD]  = { {8,8,8},  {4,4,4}, {8,12,8}, {8,2,8},   PM_BODY, PM_PITCH|PM_YAW },
        [PM_ARM_L] = { {4,12,4}, {6,2,6}, {8,3,8},  {2,3,8},   PM_BODY, PM_PITCH|PM_YAW },
        [PM_ARM_R] = { {4,12,4}, {6,2,6}, {8,3,8},  {14,3,8},  PM_BODY, PM_PITCH|PM_YAW },
        [PM_LEG_L] = { {4,12,4}, {6,2,6}, {8,2,8},  {6,14,8},  PM_BODY, PM_PITCH },
        [PM_LEG_R] = { {4,12,4}, {6,2,6}, {8,2,8},  {10,14,8}, PM_BODY, PM_PITCH },
        },
};

// how many px each face spans in u and v, by orient code (1..6)
static void pm_face_extent(struct pm_piece *p, int orient, int *eu, int *ev)
{
        switch (orient) {
        case UP:   case DOWN:  *eu = p->dims[0]; *ev = p->dims[2]; break;
        case EAST: case WEST:  *eu = p->dims[2]; *ev = p->dims[1]; break;
        default:               *eu = p->dims[0]; *ev = p->dims[1]; break; // N/S
        }
}

#define PM_RGB(r, g, b) (0xff000000u | ((b) << 16) | ((g) << 8) | (r))

// paint a model's tiles: per-piece base/dark/accent colors and a random
// pattern per piece, with a darker border around the used region of each face
// so articulation and UV extents stay visible. Deterministic in the seed;
// the painted texels are what travels the net, not the seed.
static void pm_paint(struct pmodel *mo, unsigned seed)
{
        mo->palette[0] = 0; // transparent
        for (int i = 0; i < mo->nr_pieces; i++)
        {
                int base = 1 + i * 3, dark = base + 1, accent = base + 2;
                int r = 64 + dumb_rand(&seed) % 192;
                int g = 64 + dumb_rand(&seed) % 192;
                int b = 64 + dumb_rand(&seed) % 192;
                mo->palette[base]   = PM_RGB(r, g, b);
                mo->palette[dark]   = PM_RGB(r * 2 / 3, g * 2 / 3, b * 2 / 3);
                mo->palette[accent] = PM_RGB(64 + dumb_rand(&seed) % 192,
                                             64 + dumb_rand(&seed) % 192,
                                             64 + dumb_rand(&seed) % 192);
                int pattern = dumb_rand(&seed) % 4;

                for (int f = 0; f < PM_FACES; f++)
                {
                        int eu, ev;
                        pm_face_extent(&mo->piece[i], f + 1, &eu, &ev);
                        for (int v = 0; v < PM_TILE; v++)
                        for (int u = 0; u < PM_TILE; u++)
                        {
                                int c = base;
                                switch (pattern) {
                                case 1: c = ((u / 2 + v / 2) & 1) ? accent : base; break;
                                case 2: c = (v / 2 & 1) ? accent : base; break;
                                case 3: c = (dumb_rand(&seed) % 5 == 0) ? accent : base; break;
                                }
                                if (u < eu && v < ev &&
                                    (u == 0 || v == 0 || u == eu - 1 || v == ev - 1))
                                        c = dark;
                                mo->texel[i][f][v * PM_TILE + u] = c;
                        }
                }
        }
}

// an even random size in [lo, hi] so halved dims stay on the integer px grid
static int pm_even(unsigned *seed, int lo, int hi)
{
        return lo + 2 * (dumb_rand(seed) % ((hi - lo) / 2 + 1));
}

// random model: the default 6-piece skeleton with randomized prism sizes, the
// connection points re-derived so limbs stay attached and feet stay on the
// ground. (Head height varies, so only the default model's head sits exactly
// at the camera eye.) Then random colors and patterns on top.
static void pmodel_randomize(struct pmodel *mo, unsigned seed)
{
        *mo = pm_default;
        struct pm_piece *pc = mo->piece;

        int hw = pm_even(&seed, 6, 12), hh = pm_even(&seed, 6, 12), hd = pm_even(&seed, 6, 12);
        int bw = pm_even(&seed, 6, 10), bh = pm_even(&seed, 8, 14), bd = pm_even(&seed, 2, 6);
        int aw = pm_even(&seed, 2, 4),  ah = pm_even(&seed, 8, 14);
        int lw = pm_even(&seed, 2, 4),  lh = pm_even(&seed, 8, 14);

        // body hangs from the center box so the legs' feet land on the ground:
        // box px p is 8 + K - p px above the feet, K = the box anchor height
        int K = (int)roundf((PLYR_H / 2) / PM_SCALE);
        pc[PM_BODY] = (struct pm_piece){ {bw,bh,bd}, {8-bw/2, 8-bh/2, 8-bd/2},
                {8,8,8}, {8, 8 + K - lh - bh/2, 8}, -1, 0 };
        pc[PM_HEAD] = (struct pm_piece){ {hw,hh,hd}, {8-hw/2, 8-hh/2, 8-hd/2},
                {8, 8+hh/2, 8}, {8, 8-bh/2, 8}, PM_BODY, PM_PITCH|PM_YAW };
        pc[PM_ARM_L] = (struct pm_piece){ {aw,ah,aw}, {8-aw/2, 8-ah/2, 8-aw/2},
                {8, 8-ah/2+1, 8}, {8-bw/2-aw/2, 8-bh/2+1, 8}, PM_BODY, PM_PITCH|PM_YAW };
        pc[PM_ARM_R] = pc[PM_ARM_L];
        pc[PM_ARM_R].attach[0] = 8 + bw/2 + aw/2;
        pc[PM_LEG_L] = (struct pm_piece){ {lw,lh,lw}, {8-lw/2, 8-lh/2, 8-lw/2},
                {8, 8-lh/2, 8}, {8-lw/2, 8+bh/2, 8}, PM_BODY, PM_PITCH };
        pc[PM_LEG_R] = pc[PM_LEG_L];
        pc[PM_LEG_R].attach[0] = 8 + lw/2;

        pm_paint(mo, dumb_rand(&seed));
}

// ---- texture array layers: one 72-layer range per player slot --------------

static int pm_slot_layers() { return PM_MAX_PIECES * PM_FACES; }

static void pm_expand(struct pmodel *mo, unsigned char *rgba)
{
        unsigned *out = (unsigned *)rgba;
        for (int i = 0; i < PM_MAX_PIECES; i++)
                for (int f = 0; f < PM_FACES; f++)
                        for (int t = 0; t < PM_TILE * PM_TILE; t++)
                                *out++ = mo->palette[mo->texel[i][f][t]];
}

// all slots' face tiles for startup: everyone defaults except my own slot,
// which gets this instance's fresh random model
unsigned char *pmodel_make_tiles(int *nr_layers)
{
        static unsigned char rgba[NR_PLAYERS * PM_MAX_PIECES * PM_FACES * PM_TILE * PM_TILE * 4];
        pm_paint(&pm_default, 12345); // a fixed, recognizable default coat
        for (int i = 0; i < NR_PLAYERS; i++)
                pm_models[i] = pm_default;
        pmodel_randomize(&pm_models[my_player], (unsigned)SDL_GetPerformanceCounter());
        pmodel_have[my_player] = 1;

        for (int i = 0; i < NR_PLAYERS; i++)
                pm_expand(&pm_models[i], rgba + i * pm_slot_layers() * PM_TILE * PM_TILE * 4);
        *nr_layers = NR_PLAYERS * pm_slot_layers();
        return rgba;
}

// re-upload one slot's layer range after its model changed at runtime
static void pmodel_upload(int slot)
{
        static unsigned char rgba[PM_MAX_PIECES * PM_FACES * PM_TILE * PM_TILE * 4];
        pm_expand(&pm_models[slot], rgba);
        update_texture_layers(pmodel_tex_base + slot * pm_slot_layers(), pm_slot_layers(), rgba);
}

// a model arrived over the net: sanitize it (remote bytes!), store, upload
void pmodel_net_recv(int slot, const unsigned char *data, int len)
{
        if (slot < 0 || slot >= NR_PLAYERS || slot == my_player) return;
        if (len < (int)sizeof(struct pmodel)) return;
        struct pmodel *mo = &pm_models[slot];
        memcpy(mo, data, sizeof *mo);

        if (mo->nr_pieces > PM_MAX_PIECES) mo->nr_pieces = PM_MAX_PIECES;
        if (!mo->nr_pieces) { *mo = pm_default; }
        for (int i = 0; i < mo->nr_pieces; i++)
        {
                struct pm_piece *p = &mo->piece[i];
                for (int a = 0; a < 3; a++)
                {
                        p->dims[a] = ICLAMP(p->dims[a], 1, PM_TILE);
                        p->corner[a] = ICLAMP(p->corner[a], 0, PM_TILE - p->dims[a]);
                        p->origin[a] = ICLAMP(p->origin[a], 0, PM_TILE);
                        p->attach[a] = ICLAMP(p->attach[a], 0, PM_TILE);
                }
                if (p->parent >= i) p->parent = -1; // parents point backward only
        }

        pmodel_have[slot] = 1;
        pmodel_upload(slot);
        fprintf(stderr, "pmodel: got player %d's model\n", slot);
}

// the client's slot changed at WELCOME: carry the local model to its new home
void pmodel_local_moved(int old_slot)
{
        if (old_slot == my_player) return;
        pm_models[my_player] = pm_models[old_slot];
        pm_models[old_slot] = pm_default;
        pmodel_have[my_player] = 1;
        pmodel_have[old_slot] = 0;
        pmodel_upload(my_player);
}

// ---- transforms: column-major mat4s, storage as in vector.c ----

static void pm_mat_ident(float *m)
{
        memset(m, 0, 16 * sizeof *m);
        m[0] = m[5] = m[10] = m[15] = 1;
}

static void pm_mat_translate(float *m, float x, float y, float z)
{
        pm_mat_ident(m);
        m[12] = x; m[13] = y; m[14] = z;
}

// yaw about y, spinning x toward z like mob.vert's heading spin
static void pm_mat_yaw(float *m, float a)
{
        pm_mat_ident(m);
        float s = sinf(a), c = cosf(a);
        m[0] = c; m[2] = s; m[8] = -s; m[10] = c;
}

static void pm_mat_pitch(float *m, float a)
{
        pm_mat_ident(m);
        float s = sinf(a), c = cosf(a);
        m[5] = c; m[6] = s; m[9] = -s; m[10] = c;
}

// resolve the model's piece transforms (px in each piece's 16^3 space -> world)
// for a player whose hitbox top corner is at (x,y,z), facing model-yaw `yaw`.
// t drives the placeholder articulation until real animations exist.
static void pm_resolve(struct pmodel *mo, float x, float y, float z, float yaw,
                float t, float space[][16], float geom[][16])
{
        float tmp[16], tmp2[16], rot[16];

        // the center box is centered on the hitbox, but its anchor snaps to the
        // px grid counted from the feet so integer attach points can put the
        // feet exactly on the ground AND the head center exactly at eye height
        float feet = y + PLYR_H;
        float anchor = feet - roundf((PLYR_H / 2) / PM_SCALE) * PM_SCALE;
        float root[16] = {
                PM_SCALE, 0, 0, 0,
                0, PM_SCALE, 0, 0,
                0, 0, PM_SCALE, 0,
                -8 * PM_SCALE, -8 * PM_SCALE, -8 * PM_SCALE, 1, // box center px (8,8,8) -> anchor
        };
        pm_mat_yaw(tmp, yaw);
        mat4_multiply(tmp2, tmp, root);
        pm_mat_translate(tmp, x + PLYR_W / 2, anchor, z + PLYR_W / 2);
        mat4_multiply(root, tmp, tmp2);

        for (int i = 0; i < mo->nr_pieces; i++)
        {
                struct pm_piece *p = &mo->piece[i];

                // placeholder: wave every rotatable piece around so hierarchy,
                // pivots and axes are all visibly exercised
                float pitch = (p->axes & PM_PITCH) ? sinf(t * (1.1f + 0.2f * i) + i) * 0.9f : 0;
                float pyaw  = (p->axes & PM_YAW)   ? sinf(t * (0.7f + 0.3f * i) + 2 * i) * 0.9f : 0;
                pm_mat_yaw(tmp, pyaw);
                pm_mat_pitch(tmp2, pitch);
                mat4_multiply(rot, tmp, tmp2);

                // parent space -> this piece's space: pin origin to attach, rotate there
                float *par = p->parent < 0 ? root : space[(int)p->parent];
                pm_mat_translate(tmp, p->attach[0], p->attach[1], p->attach[2]);
                mat4_multiply(tmp2, par, tmp);
                mat4_multiply(tmp, tmp2, rot);
                pm_mat_translate(tmp2, -p->origin[0], -p->origin[1], -p->origin[2]);
                mat4_multiply(space[i], tmp, tmp2);

                // geometry runs 0..dims from the prism's min corner
                pm_mat_translate(tmp, p->corner[0], p->corner[1], p->corner[2]);
                mat4_multiply(geom[i], space[i], tmp);
        }
}

// ---- per-frame build + render, mob.c-style ----

static struct allocation pm_alloc[MAX_FRAMES_IN_FLIGHT];
static struct pmvert pmbuf[NR_PLAYERS * PM_MAX_PIECES * PM_FACES];
static int pm_count;  // total face instances this frame
static int pm_remote; // instances the main camera sees (the local player's own
                      // model is shadow-only until 2nd/3rd person cameras exist)

static struct pmvert *pm_emit(struct pmvert *b, int slot,
                float x, float y, float z, float yaw, float t)
{
        struct pmodel *mo = &pm_models[slot];
        float space[PM_MAX_PIECES][16], geom[PM_MAX_PIECES][16];
        pm_resolve(mo, x, y, z, yaw, t, space, geom);

        // light the whole body from the block the hitbox center is in
        float il = 0.4f, gl = 0.f;
        int bx = (x + PLYR_W / 2) / BS, by = (y + PLYR_H / 2) / BS, bz = (z + PLYR_W / 2) / BS;
        if (legit_tile(bx, by, bz))
        {
                il = CORN_(bx, by, bz);
                gl = KORN_(bx, by, bz);
        }

        for (int i = 0; i < mo->nr_pieces; i++)
        {
                float *m = geom[i];
                for (int f = 0; f < PM_FACES; f++)
                {
                        *b = (struct pmvert){
                                .r0 = { m[0], m[4], m[8],  m[12] },
                                .r1 = { m[1], m[5], m[9],  m[13] },
                                .r2 = { m[2], m[6], m[10], m[14] },
                                .dims = { mo->piece[i].dims[0], mo->piece[i].dims[1], mo->piece[i].dims[2] },
                                .orient = f + 1,
                                .tex = pmodel_tex_base + slot * pm_slot_layers() + i * PM_FACES + f,
                                .illum = il, .glow = gl,
                        };
                        b++;
                }
        }
        polys += mo->nr_pieces * PM_FACES;
        return b;
}

void pmodel_build()
{
        static float t;
        t += 0.05f; // placeholder articulation clock

        struct pmvert *b = pmbuf;

        // remote players first: the main pass draws only [0, pm_remote)
        for (int i = 0; net_mode != NET_OFF && i < NR_PLAYERS; i++)
        {
                if (!net_player_active(i)) continue;
                struct player *r = &player[i];
                // a player's forward (sin yaw, cos yaw) maps to model yaw PI - yaw
                b = pm_emit(b, i, r->pos.x, r->pos.y, r->pos.z, PI - r->yaw, t);
        }
        pm_remote = b - pmbuf;

        // the local player casts a shadow but is never drawn to the camera
        b = pm_emit(b, my_player, camplayer.pos.x, camplayer.pos.y, camplayer.pos.z,
                        PI - camplayer.yaw, t);
        pm_count = b - pmbuf;

        int fr = vk.currentFrame;
        if (!pm_alloc[fr].buf)
                vulkan_allocate_vertex_buffer(sizeof pmbuf, &pm_alloc[fr]);
        vulkan_populate_vertex_buffer(pmbuf, pm_count * sizeof *pmbuf, &pm_alloc[fr]);
}

void pmodel_render(VkCommandBuffer cmdbuf, int pipe, float *pv)
{
        int count = pipe == pmodel_pipe ? pm_remote : pm_count;
        if (!count) return;

        vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipelines[pipe].pipeline);

        struct { float pv[16]; } push;
        memcpy(push.pv, pv, sizeof push.pv);
        vkCmdPushConstants(cmdbuf, vk.pipelines[pipe].layout,
                        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                        0, sizeof push, &push);

        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(cmdbuf, 0, 1, &pm_alloc[vk.currentFrame].buf, &offset);
        vkCmdDraw(cmdbuf, 4, count, 0, 0);
}

#endif // BLOCKO_PMODEL_C_INCLUDED

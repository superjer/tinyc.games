#include "blocko.c"
#ifndef BLOCKO_MINE_C_INCLUDED
#define BLOCKO_MINE_C_INCLUDED

// mine.c - the shaking block stand-in shown while a block is being mined
//
// Hold left click to mine (see update_player in player.c): the target block is
// carved out of its chunk mesh (a hole, in mesh.c) and this file draws a shaking
// copy of that block sitting in the hole, so it reads as the block jostling
// loose. Built once per frame (mine_overlay_build) then drawn into as many
// passes as needed (mine_overlay_render): the main scene plus the near shadow
// cascade - the same build/render split as mob.c.

// tile_face_tex (the per-face texture layer) now lives in blockmodel.c, the
// shared home for off-grid block appearance; the mining stand-in still builds
// its own per-corner-lit cube below so it matches the block's world lighting.

// the block being mined, drawn as a shaking stand-in in the hole mesh.c
// carved for it - same texture and corner lighting as the real block, so it
// reads as the block itself jostling loose. Built once, drawn per pass.
static struct allocation mine_alloc[MAX_FRAMES_IN_FLIGHT];
static struct vbufv obuf[BLOCK_MODEL_MAX_FACES];
static int mine_faces;         // faces block_model_lit wrote this frame
static int mine_draw_on;
static float mine_px, mine_py, mine_pz, mine_bs;

void mine_overlay_build()
{
        mine_draw_on = 0;
        if (mine_frac <= 0.f || mine_x < 0) return;

        int x = mine_x, y = mine_y, z = mine_z, t = mine_tile;

        // the block's eight corner lights in block_model's canonical order
        // (upper sw,se,nw,ne then lower sw,se,nw,ne), exactly as mesh.c samples
        float sun[8] = {
                CORN_(x, y, z  ), CORN_(x+1, y, z  ), CORN_(x, y, z+1), CORN_(x+1, y, z+1),
                CORN_(x, y+1, z), CORN_(x+1, y+1, z), CORN_(x, y+1, z+1), CORN_(x+1, y+1, z+1),
        };
        float glo[8] = {
                KORN_(x, y, z  ), KORN_(x+1, y, z  ), KORN_(x, y, z+1), KORN_(x+1, y, z+1),
                KORN_(x, y+1, z), KORN_(x+1, y+1, z), KORN_(x, y+1, z+1), KORN_(x+1, y+1, z+1),
        };
        // a mined slope shakes loose as a wedge, keeping its facing; block_model
        // ignores facing for other blocks
        mine_faces = block_model_lit(obuf, t, TO_(x, y, z) & 3, sun, glo);

        int fr = vk.currentFrame;
        if (!mine_alloc[fr].buf)
                vulkan_allocate_vertex_buffer(sizeof obuf, &mine_alloc[fr]);
        vulkan_populate_vertex_buffer(obuf, mine_faces * sizeof *obuf, &mine_alloc[fr]);

        // rattle harder as it works loose; sit a touch inside the socket so
        // it never z-fights the walls of the hole
        float shake = 22.f * mine_frac;
        float jx = sinf(pframe * 1.9f) * shake;
        float jy = sinf(pframe * 2.7f) * shake * 0.5f;
        float jz = cosf(pframe * 2.3f) * shake;
        float inset = 15.f;

        mine_px = x * (float)BS + inset + jx;
        mine_py = y * (float)BS + inset + jy;
        mine_pz = z * (float)BS + inset + jz;
        mine_bs = BS - 2 * inset;
        mine_draw_on = 1;
        polys += mine_faces;
}

// draw the built mining stand-in with the given pipeline (already bound)
void mine_overlay_render(VkCommandBuffer cmdbuf, int pipe, float *pv)
{
        if (!mine_draw_on) return;

        int fr = vk.currentFrame;
        // reject_lo/hi mirror the terrain push layout (empty box) so the overlay,
        // drawn on the main pipeline, never inherits a stale reject box
        struct { float pv[16]; float x, y, z, bs;
                 float reject_lo[4], reject_hi[4]; } push;
        memcpy(push.pv, pv, sizeof push.pv);
        push.x = mine_px;
        push.y = mine_py;
        push.z = mine_pz;
        push.bs = mine_bs;
        push.reject_lo[0] = 1; push.reject_lo[1] = push.reject_lo[2] = push.reject_lo[3] = 0;
        push.reject_hi[0] = 0; push.reject_hi[1] = push.reject_hi[2] = push.reject_hi[3] = 0;

        vkCmdPushConstants(cmdbuf, vk.pipelines[pipe].layout,
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof push, &push);
        VkDeviceSize off = 0;
        vkCmdBindVertexBuffers(cmdbuf, 0, 1, &mine_alloc[fr].buf, &off);
        vkCmdDraw(cmdbuf, 4, mine_faces, 0, 0);
}

#endif // BLOCKO_MINE_C_INCLUDED

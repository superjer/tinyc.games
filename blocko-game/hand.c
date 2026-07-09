#include "blocko.c"
#ifndef BLOCKO_HAND_C_INCLUDED
#define BLOCKO_HAND_C_INCLUDED

// hand.c - the block you're about to place, shown floating in first person at
// the lower right of the screen. The mouse wheel cycles which block is held
// (held_cycle); right click places held_tile
//
// Rendering reuses the terrain pipeline exactly like the mining stand-in
// (mine.c): six textured faces of one cube, built once per frame (hand_build)
// and drawn in the main pass (hand_render). The cube is parked in world space a
// short distance in front of the eye - offset right and down - so it tracks the
// camera and reads as held. A squashed viewport depth range keeps it drawn on
// top of the world so it never clips into nearby walls.

// every block you can hold
static int placeable[] = {
        GRAS, MTGR, DIRT, SAND, STON, GRAN, WOOD,
        ORE, OREH, HARD, RLEF, YLEF, SLEF, WATR
};
#define NR_PLACEABLE ((int)(sizeof placeable / sizeof *placeable))

static int held_index = 9; // HARD, matching the old hard-coded place block

// spin the wheel: dir = +1 next block, -1 previous, wrapping around the list
void held_cycle(int dir)
{
        held_index = (held_index + dir + NR_PLACEABLE) % NR_PLACEABLE;
        held_tile = placeable[held_index];
}

// the held cube, built once per frame then drawn in the main pass. Positioned
// fresh each frame from the camera basis so it stays glued to the lower-right
// of the view.
static struct allocation hand_alloc[MAX_FRAMES_IN_FLIGHT];
static struct vbufv hbuf[6];
static int hand_draw_on;
static float hand_px, hand_py, hand_pz, hand_bs;

// how the held cube sits relative to the eye, in world units (BS = one block)
#define HAND_FWD   (BS * 0.90f)   // distance out in front of the eye
#define HAND_RIGHT (BS * 0.52f)   // shift toward the right edge of the screen
#define HAND_DOWN  (BS * 0.42f)   // shift down toward the bottom edge
#define HAND_SIZE  (BS * 0.42f)   // edge length of the held cube

// swing animation: a punch/mine (left click) or a place (right click) makes the
// block dip down and jab forward along a half-sine arc, then spring back. The
// main pipeline can't rotate the cube, so the swing is pure translation + a
// little scale pop. swing_a is the current arc height in [0,1] (0 = at rest);
// swing_a_prev is last tick's, so hand_build can interpolate above 60 FPS.
#define SWING_TICKS 9             // ticks for one full out-and-back swing
#define SWING_DIP   (BS * 0.45f)  // how far down the block swings at mid-arc
#define SWING_JAB   (BS * 0.28f)  // how far forward (into the scene) it jabs
static int swing_active;
static float swing;               // phase through the current swing, [0,1)
static float swing_a, swing_a_prev;

// advance the swing once per physics tick (from the main loop). A held left
// button (punch/mine) loops the swing; hand_swing_kick fires a single swing when
// a block is placed. Rest state is swing_a == 0.
void hand_animate(struct player *p)
{
        swing_a_prev = swing_a;

        int want = p->breaking || hand_swing_kick; // attacking, or a place just landed
        hand_swing_kick = 0;

        if (!swing_active && want)
        {
                swing_active = 1;
                swing = 0.f;
        }

        if (swing_active)
        {
                swing += 1.f / SWING_TICKS;
                if (swing >= 1.f)
                {
                        swing = 0.f;              // arc complete
                        if (!want) swing_active = 0; // stop unless still attacking
                }
        }

        swing_a = swing_active ? sinf(PI * swing) : 0.f;
}

void hand_build()
{
        hand_draw_on = 0;

        int t = held_tile;

        // camera basis, matching lookit() in vector.c: fwd is the look
        // direction, up is screen-up, right is screen-right (all orthonormal).
        float pitch = camplayer.pitch, yaw = camplayer.yaw;
        float fwd[3] = { cosf(pitch) * sinf(yaw), sinf(pitch), cosf(pitch) * cosf(yaw) };
        float wing[3] = { -cosf(yaw), 0, sinf(yaw) };
        float up[3] = {
                fwd[1]*wing[2] - fwd[2]*wing[1],
                fwd[2]*wing[0] - fwd[0]*wing[2],
                fwd[0]*wing[1] - fwd[1]*wing[0] };
        float um = sqrtf(up[0]*up[0] + up[1]*up[1] + up[2]*up[2]);
        up[0] /= um; up[1] /= um; up[2] /= um;
        float right[3] = {
                fwd[1]*up[2] - fwd[2]*up[1],
                fwd[2]*up[0] - fwd[0]*up[2],
                fwd[0]*up[1] - fwd[1]*up[0] };
        float rm = sqrtf(right[0]*right[0] + right[1]*right[1] + right[2]*right[2]);
        right[0] /= rm; right[1] /= rm; right[2] /= rm;

        // center of the held cube: out in front, then over and down. up points
        // screen-up, so subtract it to nudge the block downward.
        float cx = peye0 + fwd[0]*HAND_FWD + right[0]*HAND_RIGHT - up[0]*HAND_DOWN;
        float cy = peye1 + fwd[1]*HAND_FWD + right[1]*HAND_RIGHT - up[1]*HAND_DOWN;
        float cz = peye2 + fwd[2]*HAND_FWD + right[2]*HAND_RIGHT - up[2]*HAND_DOWN;

        // swing: dip the block down (-up) and jab it forward (+fwd) along the
        // arc, with a small scale pop for punch, interpolated for smoothness
        float a = lerp(mob_lerp_t, swing_a_prev, swing_a);
        cx += -up[0]*(a*SWING_DIP) + fwd[0]*(a*SWING_JAB);
        cy += -up[1]*(a*SWING_DIP) + fwd[1]*(a*SWING_JAB);
        cz += -up[2]*(a*SWING_DIP) + fwd[2]*(a*SWING_JAB);

        // the shader builds the cube from a corner, so shift back by half an edge
        hand_bs = HAND_SIZE * (1.f + a*0.10f);
        hand_px = cx - hand_bs / 2;
        hand_py = cy - hand_bs / 2;
        hand_pz = cz - hand_bs / 2;

        // lit flat and bright so the held block always reads clearly; main.vert
        // still darkens the side/bottom faces by its per-orient factor, which
        // gives the cube a little shape.
        float il = 0.9f, gl = 0.f;
        hbuf[0] = (struct vbufv){ tile_face_tex(t,UP),    UP,    0,0,0, il,il,il,il, gl,gl,gl,gl, 1 };
        hbuf[1] = (struct vbufv){ tile_face_tex(t,SOUTH), SOUTH, 0,0,0, il,il,il,il, gl,gl,gl,gl, 1 };
        hbuf[2] = (struct vbufv){ tile_face_tex(t,NORTH), NORTH, 0,0,0, il,il,il,il, gl,gl,gl,gl, 1 };
        hbuf[3] = (struct vbufv){ tile_face_tex(t,WEST),  WEST,  0,0,0, il,il,il,il, gl,gl,gl,gl, 1 };
        hbuf[4] = (struct vbufv){ tile_face_tex(t,EAST),  EAST,  0,0,0, il,il,il,il, gl,gl,gl,gl, 1 };
        hbuf[5] = (struct vbufv){ tile_face_tex(t,DOWN),  DOWN,  0,0,0, il,il,il,il, gl,gl,gl,gl, 1 };

        int fr = vk.currentFrame;
        if (!hand_alloc[fr].buf)
                vulkan_allocate_vertex_buffer(sizeof hbuf, &hand_alloc[fr]);
        vulkan_populate_vertex_buffer(hbuf, sizeof hbuf, &hand_alloc[fr]);
        hand_draw_on = 1;
        polys += 6;
}

// draw the held cube on the (already bound) main pipeline. A squashed viewport
// depth range parks it in the front sliver of the depth buffer so it draws over
// the world instead of clipping into it; the caller restores the viewport.
void hand_render(VkCommandBuffer cmdbuf, int pipe, float *pv)
{
        if (!hand_draw_on) return;

        int fr = vk.currentFrame;

        VkViewport near_vp = { 0, 0,
                vk.bestSwapchainExtent.width, vk.bestSwapchainExtent.height, 0.f, 0.1f };
        vkCmdSetViewport(cmdbuf, 0, 1, &near_vp);

        // same push layout as the terrain pipeline; empty reject box (lo > hi)
        struct { float pv[16]; float x, y, z, bs;
                 float reject_lo[4], reject_hi[4]; } push;
        memcpy(push.pv, pv, sizeof push.pv);
        push.x = hand_px;
        push.y = hand_py;
        push.z = hand_pz;
        push.bs = hand_bs;
        push.reject_lo[0] = 1; push.reject_lo[1] = push.reject_lo[2] = push.reject_lo[3] = 0;
        push.reject_hi[0] = 0; push.reject_hi[1] = push.reject_hi[2] = push.reject_hi[3] = 0;

        vkCmdPushConstants(cmdbuf, vk.pipelines[pipe].layout,
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof push, &push);
        VkDeviceSize off = 0;
        vkCmdBindVertexBuffers(cmdbuf, 0, 1, &hand_alloc[fr].buf, &off);
        vkCmdDraw(cmdbuf, 4, 6, 0, 0);

        // restore the full-depth viewport for anything drawn after us
        VkViewport full_vp = { 0, 0,
                vk.bestSwapchainExtent.width, vk.bestSwapchainExtent.height, 0.f, 1.f };
        vkCmdSetViewport(cmdbuf, 0, 1, &full_vp);
}

#endif // BLOCKO_HAND_C_INCLUDED

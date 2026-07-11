#include "blocko.c"
#ifndef BLOCKO_MINE_C_INCLUDED
#define BLOCKO_MINE_C_INCLUDED

// mine.c - the shaking block stand-in shown while a block is being mined
//
// Hold left click to mine (see update_player in player.c): the target block is
// carved out of its chunk mesh (a hole, in mesh.c) and this file draws a shaking
// copy of that block sitting in the hole, so it reads as the block jostling
// loose. When the block breaks, the same cube drops free for a few frames and
// vanishes (mine_fall). Built once per frame (mine_overlay_build) then drawn
// into as many passes as needed (mine_overlay_render): the main scene plus the
// near shadow cascade - the same build/render split as mob.c.

// the terrain texture index for one face of a block, matching mesh.c so the
// mining stand-in looks exactly like the block it replaces
static int tile_face_tex(int t, int orient)
{
        switch (t)
        {
                case GRAS: return orient == UP ? 0 : orient == DOWN ? 2 : 1;
                case MTGR: return orient == UP ? 37 : orient == DOWN ? 2 : 38;
                case DIRT: return 2;
                case STON: return 5;
                case SAND: return 6;
                case WATR: return 7;
                case ORE:  return 11;
                case OREH: return 12;
                case HARD: return 13;
                case WOOD: return 14;
                case GRAN: return 15;
                case RLEF: return 16;
                case YLEF: return 17;
                case SLEF: return 39;
                // BARR has no mesh.c case, so it stays invisible in the world grid,
                // but a stand-in / dropped item / hand copy is labelled so you can
                // tell what it is off the grid
                case BARR: return 43;
                default:   return 42; // debug: the labelled "open" tile, so a stray
                                      // OPEN (or any stray tile) is obvious instead
                                      // of masquerading as STON
        }
}

// the block being mined, drawn as a shaking stand-in in the hole mesh.c
// carved for it - same texture and corner lighting as the real block, so it
// reads as the block itself jostling loose. Built once, drawn per pass.
static struct allocation mine_alloc[MAX_FRAMES_IN_FLIGHT];
static struct vbufv obuf[6];
static int mine_draw_on;
static float mine_px, mine_py, mine_pz, mine_bs;

// the broken block's death drop: cell, tile, and the tick it broke on
static int fall_x = -1, fall_y, fall_z, fall_tile, fall_start;

// start the drop; called at break time, while mine_x/mine_tile still hold
// the block that just broke
void mine_fall()
{
        fall_x = mine_x; fall_y = mine_y; fall_z = mine_z;
        fall_tile = mine_tile;
        fall_start = pframe;
}

// the falling cube's cell is window coords, like everything drawn
void mine_scoot(int dx, int dz)
{
        if (fall_x >= 0) { fall_x += dx; fall_z += dz; }
}

void mine_overlay_build()
{
        mine_draw_on = 0;
        int mining = mine_frac > 0.f && mine_x >= 0;
        if (mining || pframe - fall_start > 10)
                fall_x = -1; // drop over - or the cube is needed for a new dig
        if (!mining && fall_x < 0) return;

        int x = mining ? mine_x : fall_x, y = mining ? mine_y : fall_y,
            z = mining ? mine_z : fall_z, t = mining ? mine_tile : fall_tile;

        // the block's eight corner lights, exactly as mesh.c samples them
        float usw = CORN_(x  , y  , z  ), use = CORN_(x+1, y  , z  );
        float unw = CORN_(x  , y  , z+1), une = CORN_(x+1, y  , z+1);
        float dsw = CORN_(x  , y+1, z  ), dse = CORN_(x+1, y+1, z  );
        float dnw = CORN_(x  , y+1, z+1), dne = CORN_(x+1, y+1, z+1);

        obuf[0] = (struct vbufv){ tile_face_tex(t,UP),    UP,    0,0,0, usw,use,unw,une, 1 };
        obuf[1] = (struct vbufv){ tile_face_tex(t,SOUTH), SOUTH, 0,0,0, use,usw,dse,dsw, 1 };
        obuf[2] = (struct vbufv){ tile_face_tex(t,NORTH), NORTH, 0,0,0, unw,une,dnw,dne, 1 };
        obuf[3] = (struct vbufv){ tile_face_tex(t,WEST),  WEST,  0,0,0, usw,unw,dsw,dnw, 1 };
        obuf[4] = (struct vbufv){ tile_face_tex(t,EAST),  EAST,  0,0,0, une,use,dne,dse, 1 };
        obuf[5] = (struct vbufv){ tile_face_tex(t,DOWN),  DOWN,  0,0,0, dse,dsw,dne,dnw, 1 };

        int fr = vk.currentFrame;
        if (!mine_alloc[fr].buf)
                vulkan_allocate_vertex_buffer(sizeof obuf, &mine_alloc[fr]);
        vulkan_populate_vertex_buffer(obuf, sizeof obuf, &mine_alloc[fr]);

        // rattle harder as it works loose; sit a touch inside the socket so
        // it never z-fights the walls of the hole. A broken block drops
        // instead: accelerating straight down (y grows down), ~2 blocks by
        // the time it vanishes
        float jx = 0, jy = 0, jz = 0;
        if (mining)
        {
                float shake = 22.f * mine_frac;
                jx = sinf(pframe * 1.9f) * shake;
                jy = sinf(pframe * 2.7f) * shake * 0.5f;
                jz = cosf(pframe * 2.3f) * shake;
        }
        else
        {
                int ft = pframe - fall_start;
                jy = 20.f * ft * ft;
        }
        float inset = 15.f;

        mine_px = x * (float)BS + inset + jx;
        mine_py = y * (float)BS + inset + jy;
        mine_pz = z * (float)BS + inset + jz;
        mine_bs = BS - 2 * inset;
        mine_draw_on = 1;
}

// draw the built mining stand-in with the given pipeline (already bound)
void mine_overlay_render(VkCommandBuffer cmdbuf, int pipe, float *pv)
{
        if (!mine_draw_on) return;

        int fr = vk.currentFrame;
        struct { float pv[16]; float x, y, z, bs; } push;
        memcpy(push.pv, pv, sizeof push.pv);
        push.x = mine_px;
        push.y = mine_py;
        push.z = mine_pz;
        push.bs = mine_bs;

        vkCmdPushConstants(cmdbuf, vk.pipelines[pipe].layout,
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof push, &push);
        VkDeviceSize off = 0;
        vkCmdBindVertexBuffers(cmdbuf, 0, 1, &mine_alloc[fr].buf, &off);
        vkCmdDraw(cmdbuf, 4, 6, 0, 0);
}

#endif // BLOCKO_MINE_C_INCLUDED

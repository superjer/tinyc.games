#include "blocko.c"
#ifndef BLOCKO_PATCH_C_INCLUDED
#define BLOCKO_PATCH_C_INCLUDED

// patch.c - instant, hitch-free block edits via shader-reject + a patch mesh
//
// A full-chunk mesh rebuild costs several ms, so editing a block by rebuilding
// its chunk on the spot stutters. Instead, when a block is broken or placed we:
//
//   1. apply the edit to the world immediately (so physics/rayshot see it),
//   2. REJECT the now-stale faces in a small box around the edit - the terrain
//      vertex shader collapses those quads to zero area (see main.vert),
//   3. PATCH: draw a tiny corrected mesh of that box (built with mesh_region,
//      pos_in origin 0,0 so it spans chunk seams), and
//   4. mark the box's chunks for a *debounced* rebuild; when that lands, the big
//      chunk buffer is authoritative again, so we drop the reject box + patch.
//
// The box is the 3x3x3 around the edited cell: a single-block edit only changes
// the faces of that cell and its 6 neighbours, all inside it. The patch is
// authoritative inside the box; everything outside is >=2 cells away and
// provably unaffected, so the untouched chunk faces stay correct. See
// reject-patch-plan.md.
//
// Phase 1: opaque faces, main pass only. Water and shadows still fold in on the
// debounced rebuild (one debounce window of slight staleness).

#define PATCH_MAX_VERTS 8192  // patch buffer capacity (a modest edit box)
#define PATCH_MAX_SPAN 24     // cap the union box's extent (tiles) on any axis

static struct allocation patch_alloc[MAX_FRAMES_IN_FLIGHT];       // opaque
static struct allocation patch_water_alloc[MAX_FRAMES_IN_FLIGHT];  // water/glow

// force the pending edit box's chunks to rebuild immediately (not debounced) and
// drop the box. Used when a new edit lands far from the pending box: rather than
// grow one giant patch (re-meshed every frame, liable to blow PATCH_MAX_VERTS
// into holes), bake what we have and start fresh. The old edits pop in within a
// frame or two via the forced rebuild - fine, since a far-apart burst (e.g.
// placing blocks while falling) is exactly what the instant path isn't for.
static void patch_flush()
{
        if (!patch_active) return;
        int wxlo = patch_lo[0] + scootx, wxhi = patch_hi[0] + scootx;
        int wzlo = patch_lo[2] + scootz, wzhi = patch_hi[2] + scootz;
        for (int cx = B2C(wxlo); cx <= B2C(wxhi); cx++)
        for (int cz = B2C(wzlo); cz <= B2C(wzhi); cz++)
                DIRTY_(cx, cz) = 1;
        patch_active = 0;
}

// grow the pending edit box to cover the 3x3x3 around a just-edited block
// (given in window tile coords) and flag every chunk it touches for a debounced
// rebuild. Called right after the world edit, from update_player.
void patch_edit(int wx, int wy, int wz)
{
        int ax = wx - scootx, az = wz - scootz; // window -> absolute tiles
        int lo[3] = { ax - 1, wy - 1, az - 1 };
        int hi[3] = { ax + 1, wy + 1, az + 1 };
        if (lo[1] < 0) lo[1] = 0;
        if (hi[1] > TILESH - 1) hi[1] = TILESH - 1;

        // if unioning this edit in would stretch the box past PATCH_MAX_SPAN on
        // any axis, flush the old box first and restart around just this edit
        if (patch_active) for (int i = 0; i < 3; i++)
        {
                int nlo = lo[i] < patch_lo[i] ? lo[i] : patch_lo[i];
                int nhi = hi[i] > patch_hi[i] ? hi[i] : patch_hi[i];
                if (nhi - nlo + 1 > PATCH_MAX_SPAN) { patch_flush(); break; }
        }

        if (!patch_active)
        {
                for (int i = 0; i < 3; i++) { patch_lo[i] = lo[i]; patch_hi[i] = hi[i]; }
                patch_active = 1;
        }
        else for (int i = 0; i < 3; i++) // accumulate: union with the pending box
        {
                if (lo[i] < patch_lo[i]) patch_lo[i] = lo[i];
                if (hi[i] > patch_hi[i]) patch_hi[i] = hi[i];
        }

        // debounced (not immediate) rebuild of every chunk the box touches - the
        // patch covers the view until the big buffer catches up
        int wxlo = patch_lo[0] + scootx, wxhi = patch_hi[0] + scootx;
        int wzlo = patch_lo[2] + scootz, wzhi = patch_hi[2] + scootz;
        for (int cx = B2C(wxlo); cx <= B2C(wxhi); cx++)
        for (int cz = B2C(wzlo); cz <= B2C(wzhi); cz++)
                DIRTY_LIGHT(cx, cz);
}

// fill a shader reject box (window tile coords) from this frame's effective box,
// or an empty box (lo.x > hi.x) when nothing is pending. Called per frame in draw.c.
void patch_reject_box(float lo[4], float hi[4])
{
        if (patch_box_on)
        {
                lo[0] = patch_box_lo[0] + scootx; lo[1] = patch_box_lo[1]; lo[2] = patch_box_lo[2] + scootz;
                hi[0] = patch_box_hi[0] + scootx; hi[1] = patch_box_hi[1]; hi[2] = patch_box_hi[2] + scootz;
                lo[3] = hi[3] = 0;
        }
        else
        {
                lo[0] = 1; lo[1] = lo[2] = lo[3] = 0; // empty: lo.x > hi.x
                hi[0] = hi[1] = hi[2] = hi[3] = 0;
        }
}

// once per frame, after build_meshes: work out this frame's effective box (the
// pending edit box unioned with the block currently being mined) and rebuild the
// little patch mesh over it - or, if there's nothing to show, leave it off.
//
// The persistent edit box (break/place) retires the frame its chunks come back
// clean (the debounced rebuild has folded the edit into the big buffer). The
// mining box is transient: the block is still solid in tiles, so when mining
// stops (block broken -> handed to a persistent edit; or aborted) the box simply
// drops and the untouched chunk buffer shows the block again.
void patch_update()
{
        patch_box_on = 0;
        patch_vert_count = 0;

        // retire the persistent edit box once its chunks have rebuilt
        if (patch_active)
        {
                int wxlo = patch_lo[0] + scootx, wxhi = patch_hi[0] + scootx;
                int wzlo = patch_lo[2] + scootz, wzhi = patch_hi[2] + scootz;
                int clean = 1;
                for (int cx = B2C(wxlo); cx <= B2C(wxhi) && clean; cx++)
                for (int cz = B2C(wzlo); cz <= B2C(wzhi) && clean; cz++)
                        if (DIRTY_(cx, cz) || LIGHTDIRTY_(cx, cz))
                                clean = 0;
                if (clean) patch_active = 0;
        }

        int mining = (mine_frac > 0.f && mine_x >= 0);
        if (!patch_active && !mining) return;

        // effective box = union of the persistent edit box and the mining box
        int lo[3], hi[3], have = 0;
        if (patch_active)
        {
                for (int i = 0; i < 3; i++) { lo[i] = patch_lo[i]; hi[i] = patch_hi[i]; }
                have = 1;
        }
        if (mining)
        {
                int mx = mine_x - scootx, my = mine_y, mz = mine_z - scootz; // window -> absolute
                int mlo[3] = { mx - 1, my - 1, mz - 1 };
                int mhi[3] = { mx + 1, my + 1, mz + 1 };
                if (mlo[1] < 0) mlo[1] = 0;
                if (mhi[1] > TILESH - 1) mhi[1] = TILESH - 1;
                if (!have) { for (int i = 0; i < 3; i++) { lo[i] = mlo[i]; hi[i] = mhi[i]; } }
                else for (int i = 0; i < 3; i++)
                {
                        if (mlo[i] < lo[i]) lo[i] = mlo[i];
                        if (mhi[i] > hi[i]) hi[i] = mhi[i];
                }
        }
        for (int i = 0; i < 3; i++) { patch_box_lo[i] = lo[i]; patch_box_hi[i] = hi[i]; }
        patch_box_on = 1;

        // corrected mesh of the box, absolute pos_in (origin 0,0) so it draws with
        // a zero chunk origin even across chunk seams. patch_meshing lets mesh_region
        // carve the still-solid mining block to OPEN, so the patch shows the hole
        // walls behind the shaking overlay (mine.c).
        int wxlo = lo[0] + scootx, wxhi = hi[0] + scootx;
        int wzlo = lo[2] + scootz, wzhi = hi[2] + scootz;
        patch_meshing = 1;
        mesh_region(wxlo, wxhi + 1, lo[1], hi[1] + 1, wzlo, wzhi + 1, FACE_ALL, 0, 0);
        patch_meshing = 0;
        patch_vert_count = v - vbuf;  // opaque faces (main pass)
        patch_water_count = w - wbuf; // water + glow faces (transparent pass, Phase 2)
        if (patch_vert_count > PATCH_MAX_VERTS || patch_water_count > PATCH_MAX_VERTS)
        {
                // the corrected mesh overflows the patch buffer; a clamped (truncated)
                // patch would render holes. Bake the pending edit with an immediate
                // rebuild and skip the patch this frame. patch_flush retires the box, so
                // this can't re-fire every frame - a lone 3x3x3 edit/mining box never
                // overflows, only an accumulated multi-edit box does, so it warns once
                // per burst, not per frame.
                fprintf(stderr, "patch: edit box too big (%d opaque, %d water verts), baking\n",
                        patch_vert_count, patch_water_count);
                patch_flush();
                patch_box_on = 0;
                patch_vert_count = patch_water_count = 0;
                return;
        }

        int fr = vk.currentFrame;
        if (patch_vert_count > 0)
        {
                if (!patch_alloc[fr].buf)
                        vulkan_allocate_vertex_buffer(PATCH_MAX_VERTS * sizeof(struct vbufv), &patch_alloc[fr]);
                vulkan_populate_vertex_buffer(vbuf, patch_vert_count * sizeof(struct vbufv), &patch_alloc[fr]);
                polys += patch_vert_count;
        }
        if (patch_water_count > 0)
        {
                if (!patch_water_alloc[fr].buf)
                        vulkan_allocate_vertex_buffer(PATCH_MAX_VERTS * sizeof(struct vbufv), &patch_water_alloc[fr]);
                vulkan_populate_vertex_buffer(wbuf, patch_water_count * sizeof(struct vbufv), &patch_water_alloc[fr]);
                polys += patch_water_count;
        }
}

// draw the patch mesh with the given (already-bound) pipeline. Origin is the
// world origin (chunk_x/y/z = 0) since pos_in is absolute; its own reject box is
// empty because the patch is authoritative inside the edit box.
void patch_render(VkCommandBuffer cmdbuf, int pipe, float *pv)
{
        if (!patch_box_on || patch_vert_count <= 0) return;

        int fr = vk.currentFrame;
        struct { float pv[16]; float chunk_x, chunk_y, chunk_z, bs;
                 float reject_lo[4], reject_hi[4]; } push;
        memcpy(push.pv, pv, sizeof push.pv);
        push.chunk_x = push.chunk_y = push.chunk_z = 0;
        push.bs = BS;
        push.reject_lo[0] = 1; push.reject_hi[0] = 0; // empty box
        push.reject_lo[1] = push.reject_lo[2] = 0;
        push.reject_hi[1] = push.reject_hi[2] = push.reject_hi[3] = 0;
        push.reject_lo[3] = patch_tint ? 1.f : 0.f; // debug: tint the patch red

        vkCmdPushConstants(cmdbuf, vk.pipelines[pipe].layout,
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof push, &push);
        VkDeviceSize off = 0;
        vkCmdBindVertexBuffers(cmdbuf, 0, 1, &patch_alloc[fr].buf, &off);
        vkCmdDraw(cmdbuf, 4, patch_vert_count, 0, 0);
}

// draw the patch's water/glow faces (Phase 2). Same absolute-origin setup as
// patch_render, but the water/transparent verts from wbuf; call it in the
// transparent pass with water_pipe, after the chunk water is drawn.
void patch_render_water(VkCommandBuffer cmdbuf, int pipe, float *pv)
{
        if (!patch_box_on || patch_water_count <= 0) return;

        int fr = vk.currentFrame;
        struct { float pv[16]; float chunk_x, chunk_y, chunk_z, bs;
                 float reject_lo[4], reject_hi[4]; } push;
        memcpy(push.pv, pv, sizeof push.pv);
        push.chunk_x = push.chunk_y = push.chunk_z = 0;
        push.bs = BS;
        push.reject_lo[0] = 1; push.reject_hi[0] = 0; // empty box
        push.reject_lo[1] = push.reject_lo[2] = 0;
        push.reject_hi[1] = push.reject_hi[2] = push.reject_hi[3] = 0;
        push.reject_lo[3] = patch_tint ? 1.f : 0.f; // debug: tint the patch red

        vkCmdPushConstants(cmdbuf, vk.pipelines[pipe].layout,
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof push, &push);
        VkDeviceSize off = 0;
        vkCmdBindVertexBuffers(cmdbuf, 0, 1, &patch_water_alloc[fr].buf, &off);
        vkCmdDraw(cmdbuf, 4, patch_water_count, 0, 0);
}

#endif // BLOCKO_PATCH_C_INCLUDED

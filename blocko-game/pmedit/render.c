#include "../blocko.c"
#ifndef BLOCKO_PMEDIT_RENDER_C_INCLUDED
#define BLOCKO_PMEDIT_RENDER_C_INCLUDED

// pmedit/render.c - draw the preview over the finished world

// bind the blend pipe and draw the faint reference geometry - hidden and
// mode-set-aside pieces plus the ground quad and hitbox frame - then hand the
// solid pipe back. The ghost pipe writes no depth, so this reads see-through
// and never occludes the opaque model already laid down under it
static void pmedit_draw_faint(VkCommandBuffer cmdbuf, const void *push, unsigned pushsz)
{
        vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS,
                        vk.pipelines[pmodel_ghost_pipe].pipeline);
        vkCmdPushConstants(cmdbuf, vk.pipelines[pmodel_ghost_pipe].layout,
                        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                        0, pushsz, push);
        if (pm_edit_hide_count)
                vkCmdDraw(cmdbuf, 4, pm_edit_hide_count, 0, pm_edit_hide_start);
        if (pm_edit_floor_count)
                vkCmdDraw(cmdbuf, 4, pm_edit_floor_count, 0, pm_edit_floor_start);
        vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS,
                        vk.pipelines[pmodel_pipe].pipeline);
        vkCmdPushConstants(cmdbuf, vk.pipelines[pmodel_pipe].layout,
                        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                        0, pushsz, push);
}

// draw the preview over the finished world: clear depth so it always wins,
// the unselected pieces first, then (on cleared depth each) the white outline
// hull and the selected piece over it, so the selection always reads in full

void pmedit_render(VkCommandBuffer cmdbuf)
{
        if (!pmedit_on) return;
        if (pmedit_picker) { pmedit_pick_render(cmdbuf); return; }
        if (!(pm_edit_rest_count + pm_edit_sel_count + pm_edit_hide_count)) return;

        // the preview renders through the editor's leveled camera, not the
        // world's (which keeps whatever pitch the player froze it at)
        float f[3], pv[16], view[16];
        lookit(view, f, peye0, peye1, peye2, 0, camplayer.yaw);
        translate(view, -peye0, -peye1, -peye2);
        mat4_multiply(pv, pmedit_proj, view); // the editor's flat projection

        VkClearAttachment ca = { .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                .clearValue.depthStencil = {1.f, 0} };
        VkClearRect cr = { .rect = {{0, 0},
                {vk.bestSwapchainExtent.width, vk.bestSwapchainExtent.height}},
                .baseArrayLayer = 0, .layerCount = 1 };

        vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS,
                        vk.pipelines[pmodel_pipe].pipeline);
        struct { float pv[16]; } push;
        memcpy(push.pv, pv, sizeof push.pv);
        vkCmdPushConstants(cmdbuf, vk.pipelines[pmodel_pipe].layout,
                        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                        0, sizeof push, &push);
        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(cmdbuf, 0, 1, &pm_alloc[vk.currentFrame].buf, &offset);

        // JOINT/SOCKET/PARENT clear depth once and share it: pieces overlap
        // honestly and the highlights are inverted hulls (PARENT's views
        // don't overlap on screen anyway). Paint/TYPE modes clear per group
        // so the selection always reads in full, in front.
        int layers[3][2] = {
                { pm_edit_rest_start, pm_edit_rest_count },
                { pm_edit_hull_start, pm_edit_hull_count },
                { pm_edit_sel_start,  pm_edit_sel_count  },
        };
        int shared = pmedit_joint || pmedit_socket || pmedit_parent;
        int cleared = 0;
        for (int i = 0; i < 3; i++)
        {
                // paint/TYPE clears depth per group, so the faint geometry
                // (hidden pieces, ground quad, hitbox frame) has to ride the
                // unselected group's depth right here to occlude true: the
                // floor slices a sunken model, the frame hides behind pieces
                // in front. Shared gizmo modes clear once and draw it LAST
                // (below) so a faint piece layers over the active one rather
                // than hiding it
                if (!shared && i == 1 && (pm_edit_hide_count || pm_edit_floor_count))
                {
                        if (!cleared)
                        {
                                vkCmdClearAttachments(cmdbuf, 1, &ca, 1, &cr);
                                cleared = 1;
                        }
                        pmedit_draw_faint(cmdbuf, &push, sizeof push);
                }
                if (!layers[i][1]) continue;
                if (!shared || !cleared)
                {
                        vkCmdClearAttachments(cmdbuf, 1, &ca, 1, &cr);
                        cleared = 1;
                }
                vkCmdDraw(cmdbuf, 4, layers[i][1], 0, layers[i][0]);
        }
        // shared gizmo modes: the faint reference geometry draws last, after
        // the whole opaque model, so it never occludes the active piece - it
        // blends over it, honestly see-through (the ghost pipe writes no depth)
        if (shared && (pm_edit_hide_count || pm_edit_floor_count))
        {
                if (!cleared)
                {
                        vkCmdClearAttachments(cmdbuf, 1, &ca, 1, &cr);
                        cleared = 1;
                }
                pmedit_draw_faint(cmdbuf, &push, sizeof push);
        }
        // the gizmo shares the last layer's depth: where the cube and axis
        // lines pierce the piece's surface shows exactly, no x-ray. MOVE
        // PART's group is the see-through preview, and RESIZE's is the
        // picked face's see-through slab - swap in the blend pipe
        if (pm_edit_joint_count)
        {
                if (pmedit_socket || pmedit_resize)
                {
                        vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                        vk.pipelines[pmodel_ghost_pipe].pipeline);
                        vkCmdPushConstants(cmdbuf, vk.pipelines[pmodel_ghost_pipe].layout,
                                        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                                        0, sizeof push, &push);
                }
                vkCmdDraw(cmdbuf, 4, pm_edit_joint_count, 0, pm_edit_joint_start);
        }
}

#endif // BLOCKO_PMEDIT_RENDER_C_INCLUDED

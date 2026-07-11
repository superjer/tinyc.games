#include "blocko.c"
#ifndef BLOCKO_SHADOW_C_INCLUDED
#define BLOCKO_SHADOW_C_INCLUDED

// One shadow cascade: a +-40 block ortho volume centered on the player,
// rendered every frame. main.frag fades the shadow out over the outer 10%
// of the map so the bubble has no hard edge.

// Draw the shadow pass to the near cascade's framebuffer
void draw_shadow_pass(VkCommandBuffer cmdbuf, int cascade_idx, float bias_constant, float bias_slope, unsigned char cascade_bit)
{
    VkFramebuffer fb = shadow[cascade_idx].framebuffer;
    float *shadow_pv = shadow[cascade_idx].matrix;
        VkClearValue clearValue = {.depthStencil = {1.0f, 0}};
        VkRenderPassBeginInfo rpInfo = {
                .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                .renderPass = shadow_render_pass,
                .framebuffer = fb,
                .renderArea = {{0, 0}, {shadow_sz[cascade_idx], shadow_sz[cascade_idx]}},
                .clearValueCount = 1,
                .pClearValues = &clearValue,
        };
        vkCmdBeginRenderPass(cmdbuf, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

        // solid faces draw with an empty fragment stage (fast depth path, no
        // texture fetch); the terrain section is [solid | leaves) so the
        // leaves alpha-test in a second pass below.
        int terrain_pipe = shadow_solid_pipe;
        vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipelines[terrain_pipe].pipeline);

        VkViewport viewport = {0, 0, shadow_sz[cascade_idx], shadow_sz[cascade_idx], 0, 1};
        VkRect2D scissor = {{0, 0}, {shadow_sz[cascade_idx], shadow_sz[cascade_idx]}};
        vkCmdSetViewport(cmdbuf, 0, 1, &viewport);
        vkCmdSetScissor(cmdbuf, 0, 1, &scissor);
        vkCmdSetDepthBias(cmdbuf, bias_constant, 0.0f, bias_slope);

        // Bind descriptor set for texture access (alpha testing leaves)
        vkCmdBindDescriptorSets(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS,
                vk.pipelines[terrain_pipe].layout, 0, 1, &main_descriptor_set[vk.currentFrame], 0, NULL);

        struct { float pv[16]; float chunk_x, chunk_y, chunk_z, bs; } push;
        memcpy(push.pv, shadow_pv, sizeof push.pv);
        push.bs = BS;

        for (int k = 0; k < visible_chunk_count; k++) {
                if (!(visible_chunks[k].shadow_mask & cascade_bit)) continue;

                int i = visible_chunks[k].x;
                int j = visible_chunks[k].z;
                push.chunk_x = i * BS * CHUNKW;
                push.chunk_y = 0;
                push.chunk_z = j * BS * CHUNKD;
                size_t terrain_verts = LEAFSTART_(i, j);
                VkDeviceSize voffset = 0;
                vkCmdPushConstants(cmdbuf, vk.pipelines[terrain_pipe].layout,
                        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                        0, sizeof push, &push);
                vkCmdBindVertexBuffers(cmdbuf, 0, 1, &WBUF_(i, j), &voffset);
                vkCmdDraw(cmdbuf, 4, terrain_verts, 0, 0);
        }

        // leaves [LEAFSTART, WBOSTART) draw with the alpha-test pipeline so
        // close-up leaf shadows dapple
        vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipelines[shadow_pipe].pipeline);
        for (int k = 0; k < visible_chunk_count; k++) {
                if (!(visible_chunks[k].shadow_mask & cascade_bit)) continue;
                int i = visible_chunks[k].x;
                int j = visible_chunks[k].z;
                size_t leaf_verts = WBOSTART_(i, j) - LEAFSTART_(i, j);
                if (!leaf_verts) continue;
                push.chunk_x = i * BS * CHUNKW;
                push.chunk_y = 0;
                push.chunk_z = j * BS * CHUNKD;
                VkDeviceSize leaf_offset = LEAFSTART_(i, j) * sizeof(struct vbufv);
                vkCmdPushConstants(cmdbuf, vk.pipelines[shadow_pipe].layout,
                        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                        0, sizeof push, &push);
                vkCmdBindVertexBuffers(cmdbuf, 0, 1, &WBUF_(i, j), &leaf_offset);
                vkCmdDraw(cmdbuf, 4, leaf_verts, 0, 0);
        }

        // tall grass casts shadows too. the grass lives in the transparent
        // section [WBOSTART, VBOLEN) mixed with water; shadow.vert collapses
        // the water (alpha < 1) so only the grass billboards write depth here.
        for (int k = 0; k < visible_chunk_count; k++) {
                if (!(visible_chunks[k].shadow_mask & cascade_bit)) continue;
                int i = visible_chunks[k].x;
                int j = visible_chunks[k].z;
                size_t water_start = WBOSTART_(i, j);
                size_t water_verts = VBOLEN_(i, j) - water_start;
                if (!water_verts) continue;
                push.chunk_x = i * BS * CHUNKW;
                push.chunk_y = 0;
                push.chunk_z = j * BS * CHUNKD;
                VkDeviceSize grass_offset = water_start * sizeof(struct vbufv);
                vkCmdPushConstants(cmdbuf, vk.pipelines[shadow_pipe].layout,
                        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                        0, sizeof push, &push);
                vkCmdBindVertexBuffers(cmdbuf, 0, 1, &WBUF_(i, j), &grass_offset);
                vkCmdDraw(cmdbuf, 4, water_verts, 0, 0);
        }

        // mobs and the block being mined cast shadows as well
        mob_render(cmdbuf, mob_shadow_pipe, shadow_pv);
        item_render(cmdbuf, mob_shadow_pipe, shadow_pv);
        vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipelines[shadow_pipe].pipeline);
        mine_overlay_render(cmdbuf, shadow_pipe, shadow_pv);

        vkCmdEndRenderPass(cmdbuf);
}

void do_shadows()
{
        if (shadow_mapping)
        {
                float tgt[3] = { shadow_target[0], shadow_target[1], shadow_target[2] };
                float light_pos[3] = { sun_pos.x, sun_pos.y, sun_pos.z };

                // Build view matrix: look from light toward target
                float fwd[3] = {
                        tgt[0] - light_pos[0],
                        tgt[1] - light_pos[1],
                        tgt[2] - light_pos[2],
                };
                float fwd_len = sqrtf(fwd[0]*fwd[0] + fwd[1]*fwd[1] + fwd[2]*fwd[2]);
                fwd[0] /= fwd_len; fwd[1] /= fwd_len; fwd[2] /= fwd_len;

                // Right = normalize(cross(world_up, forward))
                float world_up[3] = {0, 1, 0};
                if (fabsf(fwd[1]) > 0.99f) {
                        // Sun near zenith, use Z as up
                        world_up[0] = 0; world_up[1] = 0; world_up[2] = 1;
                }
                float right[3] = {
                        world_up[1]*fwd[2] - world_up[2]*fwd[1],
                        world_up[2]*fwd[0] - world_up[0]*fwd[2],
                        world_up[0]*fwd[1] - world_up[1]*fwd[0],
                };
                float right_len = sqrtf(right[0]*right[0] + right[1]*right[1] + right[2]*right[2]);
                right[0] /= right_len; right[1] /= right_len; right[2] /= right_len;

                // Up = cross(forward, right)
                float up[3] = {
                        fwd[1]*right[2] - fwd[2]*right[1],
                        fwd[2]*right[0] - fwd[0]*right[2],
                        fwd[0]*right[1] - fwd[1]*right[0],
                };

                // View matrix (column-major)
                float view_mtrx[16] = {
                        right[0], up[0], -fwd[0], 0,
                        right[1], up[1], -fwd[1], 0,
                        right[2], up[2], -fwd[2], 0,
                        -(right[0]*light_pos[0] + right[1]*light_pos[1] + right[2]*light_pos[2]),
                        -(up[0]*light_pos[0] + up[1]*light_pos[1] + up[2]*light_pos[2]),
                        (fwd[0]*light_pos[0] + fwd[1]*light_pos[1] + fwd[2]*light_pos[2]),
                        1,
                };

                // Orthographic projection (Vulkan depth [0,1]), +-40 blocks
                float snear = 10.f;
                float sfar = dist2sun * 2;
                float mag = 40000.f;

                // snap the light-space translation to whole shadow-map texels:
                // the sun never rotates, so this pins the texel grid to the
                // world exactly and shadow edges never crawl as the window
                // follows the player
                float texel = 2.f * mag / shadow_sz[SHADOW_NEAR];
                view_mtrx[12] = roundf(view_mtrx[12] / texel) * texel;
                view_mtrx[13] = roundf(view_mtrx[13] / texel) * texel;

                float ortho_mtrx[] = {
                        1.f/mag, 0,       0,                        0,
                        0,       1.f/mag, 0,                        0,
                        0,       0,       -1.f/(sfar - snear),      0,
                        0,       0,       -snear/(sfar - snear),    1,
                };

                mat4_multiply(shadow[SHADOW_NEAR].matrix, ortho_mtrx, view_mtrx);
        }

        // Rebuild the biased sampling matrix from the stored shadow matrix
        // (kept world-aligned across scoots by apply_scoot). The bias matrix
        // maps NDC to texture coordinates: xy from [-1,1] to [0,1].
        if (shadow_mapping) {
                float bias_mtrx[] = {
                        0.5f, 0,    0, 0,
                        0,    0.5f, 0, 0,
                        0,    0,    1, 0,
                        0.5f, 0.5f, 0, 1,
                };
                mat4_multiply(main_ubo.shadow_space, bias_mtrx, shadow[SHADOW_NEAR].matrix);
        }
}

void shadow_render(VkCommandBuffer cmdbuf)
{
        if (shadow_mapping)
                draw_shadow_pass(cmdbuf, SHADOW_NEAR, 1.5f, 1.5f, 1);
}

#endif // BLOCKO_SHADOW_C_INCLUDED

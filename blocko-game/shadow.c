#include "blocko.c"
#ifndef BLOCKO_SHADOW_C_INCLUDED
#define BLOCKO_SHADOW_C_INCLUDED

// Draw shadow pass to a specific framebuffer
// cascade_idx: which shadow cascade (SHADOW_NEAR, SHADOW_MID, etc.)
// cascade_bit: which bit in shadow_mask to check (1=Near, 2=Mid, 4=Far, 8=Extreme)
void draw_shadow_pass(VkCommandBuffer cmdbuf, int cascade_idx, float bias_constant, float bias_slope, unsigned char cascade_bit)
{
    VkFramebuffer fb = shadow[cascade_idx].framebuffer;
    float *shadow_pv = shadow[cascade_idx].matrix;
        VkClearValue clearValue = {.depthStencil = {1.0f, 0}};
        VkRenderPassBeginInfo rpInfo = {
                .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                .renderPass = shadow_render_pass,
                .framebuffer = fb,
                .renderArea = {{0, 0}, {SHADOW_SZ, SHADOW_SZ}},
                .clearValueCount = 1,
                .pClearValues = &clearValue,
        };
        vkCmdBeginRenderPass(cmdbuf, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipelines[shadow_pipe].pipeline);

        VkViewport viewport = {0, 0, SHADOW_SZ, SHADOW_SZ, 0, 1};
        VkRect2D scissor = {{0, 0}, {SHADOW_SZ, SHADOW_SZ}};
        vkCmdSetViewport(cmdbuf, 0, 1, &viewport);
        vkCmdSetScissor(cmdbuf, 0, 1, &scissor);
        vkCmdSetDepthBias(cmdbuf, bias_constant, 0.0f, bias_slope);

        // Bind descriptor set for texture access (alpha testing leaves)
        vkCmdBindDescriptorSets(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS,
                vk.pipelines[shadow_pipe].layout, 0, 1, &main_descriptor_set[vk.currentFrame], 0, NULL);

        struct { float pv[16]; float chunk_x; float chunk_y; float chunk_z; float bs; } push;
        memcpy(push.pv, shadow_pv, sizeof push.pv);
        push.bs = BS;
        int cascade_x_draw_calls = 0;

        VkDeviceSize voffset = 0;
        for (int k = 0; k < visible_chunk_count; k++) {
                if (!(visible_chunks[k].shadow_mask & cascade_bit)) continue;

                int i = visible_chunks[k].x;
                int j = visible_chunks[k].z;
                push.chunk_x = i * BS * CHUNKW;
                push.chunk_y = 0;
                push.chunk_z = j * BS * CHUNKD;
                vkCmdPushConstants(cmdbuf, vk.pipelines[shadow_pipe].layout,
                        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                        0, sizeof push, &push);
                vkCmdBindVertexBuffers(cmdbuf, 0, 1, &world_buf[i * VAOD + j], &voffset);
                size_t terrain_verts = WBOSTART_(i, j);
                vkCmdDraw(cmdbuf, terrain_verts, 1, 0, 0);
                shadow_polys += terrain_verts;
                shadow[cascade_idx].polys += terrain_verts;
                cascade_x_draw_calls++;
        }

        vkCmdEndRenderPass(cmdbuf);
}

void do_shadows()
{
        // Quantization step for far/extreme cascades (radians)
        const float FAR_QUANT_STEP = 0.002f;
        const float EXTREME_QUANT_STEP = 0.002f;

        // Determine effective light pitch - use moon at night (sun below horizon)
        int is_night = (sun_pitch >= PI);
        float light_pitch = is_night ? moon_pitch : sun_pitch;
        if (light_pitch < 0) light_pitch += TAU;

        // Compute current slots for far/extreme cascades (A/B temporal blending)
        int far_slot = (int)floorf(light_pitch / FAR_QUANT_STEP);
        int extreme_slot = (int)floorf(light_pitch / EXTREME_QUANT_STEP);

        // Alternating A/B shadow maps based on slot parity:
        // - Even slot: A=slot (lo), B=slot+1 (hi), blend 0→1
        // - Odd slot:  B=slot (lo), A=slot+1 (hi), blend 1→0
        // This ensures when crossing a boundary, the shadow map that was "hi" becomes "lo"
        // without needing to copy matrices.

        int far_slot_is_even = (far_slot % 2) == 0;
        int extreme_slot_is_even = (extreme_slot % 2) == 0;

        // Determine which slot each shadow map should be at
        shadow[SHADOW_FAR_A].slot = far_slot_is_even ? far_slot : far_slot + 1;  // A = even slots
        shadow[SHADOW_FAR_B].slot = far_slot_is_even ? far_slot + 1 : far_slot;  // B = odd slots
        shadow[SHADOW_EXT_A].slot = extreme_slot_is_even ? extreme_slot : extreme_slot + 1;
        shadow[SHADOW_EXT_B].slot = extreme_slot_is_even ? extreme_slot + 1 : extreme_slot;

        // Blend factor: position within current slot (0→1)
        float far_blend_raw = (light_pitch - far_slot * FAR_QUANT_STEP) / FAR_QUANT_STEP;
        float extreme_blend_raw = (light_pitch - extreme_slot * EXTREME_QUANT_STEP) / EXTREME_QUANT_STEP;

        // When slot is even: blend from A(0) to B(1), so use raw blend
        // When slot is odd:  blend from B(0) to A(1), so use 1-raw blend
        main_ubo.shadow_far_blend = far_slot_is_even ? far_blend_raw : (1.0f - far_blend_raw);
        main_ubo.shadow_ext_blend = extreme_slot_is_even ? extreme_blend_raw : (1.0f - extreme_blend_raw);

        // Static frame counter for alternating cascade renders
        static int shadow_frame = 0;
        shadow_frame++;

        // Alternate between Far and Extreme cascades each frame (only render one)
        // Each cascade still alternates A/B within its own render schedule
        if ((shadow_frame % 2) == 1) {
                // Odd frames: render Far, skip Extreme
                shadow_far_render_ab = (shadow_frame / 2) % 2;  // 0=A, 1=B
                shadow_ext_render_ab = -1;
        } else {
                // Even frames: render Extreme, skip Far
                shadow_far_render_ab = -1;
                shadow_ext_render_ab = ((shadow_frame - 2) / 2) % 2;  // 0=A, 1=B
        }

        // Helper to compute light position from pitch
        #define COMPUTE_LIGHT_POS(pitch, lp) do { \
                lp[0] = shadow_target[0] + dist2sun * (cosf(pitch) * cosf(sun_yaw)); \
                lp[1] = shadow_target[1] + dist2sun * (cosf(pitch) * sinf(sun_yaw) * cosf(sun_roll) + sinf(pitch) * sinf(sun_roll)); \
                lp[2] = shadow_target[2] + dist2sun * (cosf(pitch) * sinf(sun_yaw) * sinf(sun_roll) - sinf(pitch) * cosf(sun_roll)); \
        } while(0)

        // compute shadow matrices and render shadow maps
        // s=0: Near, s=1: Mid, s=2: Far, s=3: Extreme
        TIMER(shadow_render);
        if (shadow_mapping) for(int s = 0; s < 4; s++)
        {
                // Skip cascades not being rendered this frame (keeps matrix frozen)
                if (s == 2 && shadow_far_render_ab < 0) continue;
                if (s == 3 && shadow_ext_render_ab < 0) continue;

                float light_pos[3];
                float render_pitch;

                if (s == 0 || s == 1) {
                        // Near and Mid cascades: use current light pitch (sun or moon)
                        render_pitch = light_pitch;
                } else if (s == 2) {
                        // Far cascade: render at the slot assigned to A or B
                        int far_idx = (shadow_far_render_ab == 0) ? SHADOW_FAR_A : SHADOW_FAR_B;
                        render_pitch = shadow[far_idx].slot * FAR_QUANT_STEP;
                } else {
                        // Extreme cascade: render at the slot assigned to A or B
                        int ext_idx = (shadow_ext_render_ab == 0) ? SHADOW_EXT_A : SHADOW_EXT_B;
                        render_pitch = shadow[ext_idx].slot * EXTREME_QUANT_STEP;
                }

                COMPUTE_LIGHT_POS(render_pitch, light_pos);

                // Build view matrix: look from light toward target
                // Forward = normalize(target - light_pos)
                float fwd[3] = {
                        shadow_target[0] - light_pos[0],
                        shadow_target[1] - light_pos[1],
                        shadow_target[2] - light_pos[2],
                };
                float fwd_len = sqrtf(fwd[0]*fwd[0] + fwd[1]*fwd[1] + fwd[2]*fwd[2]);
                fwd[0] /= fwd_len; fwd[1] /= fwd_len; fwd[2] /= fwd_len;

                // Right = normalize(cross(world_up, forward))
                // world_up = (0, 1, 0) unless forward is vertical
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

                // Orthographic projection (Vulkan depth [0,1])
                float snear = (s == 0 ? 10.f : s == 1 ? 40.f : s == 2 ? 80.f : 200.f);
                float sfar = dist2sun * 2;
                float mag = (s == 0 ? 5000.f : s == 1 ? 20000.f : s == 2 ? 40000.f : 150000.f);

                float ortho_mtrx[] = {
                        1.f/mag, 0,       0,                        0,
                        0,       1.f/mag, 0,                        0,
                        0,       0,       -1.f/(sfar - snear),      0,
                        0,       0,       -snear/(sfar - snear),    1,
                };

                memcpy(main_ubo.proj, ortho_mtrx, sizeof(ortho_mtrx));
                memcpy(main_ubo.view, view_mtrx, sizeof(view_mtrx));
                main_ubo.bs = BS;

                // Compute shadow projection*view matrix (used for both rendering and sampling)
                float shadow_pv_mtrx[16];
                mat4_multiply(shadow_pv_mtrx, ortho_mtrx, view_mtrx);

                // Bias matrix transforms from NDC to texture coordinates
                // For Vulkan: xy from [-1,1] to [0,1], z already in [0,1]
                float bias_mtrx[] = {
                        0.5f, 0,    0, 0,      // column 0
                        0,    0.5f, 0, 0,      // column 1
                        0,    0,    1, 0,      // column 2 (Z unchanged for Vulkan)
                        0.5f, 0.5f, 0, 1,      // column 3 (translation)
                };

                float biased_shadow_mtrx[16];
                mat4_multiply(biased_shadow_mtrx, bias_mtrx, shadow_pv_mtrx);

                // Store matrices in cascade struct and UBO
                if (s == 0) {
                        // Near cascade: render every frame
                        memcpy(shadow[SHADOW_NEAR].matrix, shadow_pv_mtrx, sizeof shadow_pv_mtrx);
                        memcpy(main_ubo.shadow_space[SHADOW_NEAR], biased_shadow_mtrx, sizeof biased_shadow_mtrx);
                } else if (s == 1) {
                        // Mid cascade: render every frame (no A/B blending)
                        memcpy(shadow[SHADOW_MID].matrix, shadow_pv_mtrx, sizeof shadow_pv_mtrx);
                        memcpy(main_ubo.shadow_space[SHADOW_MID], biased_shadow_mtrx, sizeof biased_shadow_mtrx);
                } else if (s == 2) {
                        // Far cascade: store in A or B based on which we're rendering
                        int far_idx = (shadow_far_render_ab == 0) ? SHADOW_FAR_A : SHADOW_FAR_B;
                        memcpy(shadow[far_idx].matrix, shadow_pv_mtrx, sizeof shadow_pv_mtrx);
                        mat4_multiply(main_ubo.shadow_space[SHADOW_FAR_A], bias_mtrx, shadow[SHADOW_FAR_A].matrix);
                        mat4_multiply(main_ubo.shadow_space[SHADOW_FAR_B], bias_mtrx, shadow[SHADOW_FAR_B].matrix);
                } else {
                        // Extreme cascade: store in A or B based on which we're rendering
                        int ext_idx = (shadow_ext_render_ab == 0) ? SHADOW_EXT_A : SHADOW_EXT_B;
                        memcpy(shadow[ext_idx].matrix, shadow_pv_mtrx, sizeof shadow_pv_mtrx);
                        mat4_multiply(main_ubo.shadow_space[SHADOW_EXT_A], bias_mtrx, shadow[SHADOW_EXT_A].matrix);
                        mat4_multiply(main_ubo.shadow_space[SHADOW_EXT_B], bias_mtrx, shadow[SHADOW_EXT_B].matrix);
                }
        }

        // Always compute biased matrices for Far/Extreme from stored matrices
        // (needed when cascade was skipped this frame but UBO still needs valid data)
        if (shadow_mapping) {
                float bias_mtrx[] = {
                        0.5f, 0,    0, 0,
                        0,    0.5f, 0, 0,
                        0,    0,    1, 0,
                        0.5f, 0.5f, 0, 1,
                };
                mat4_multiply(main_ubo.shadow_space[SHADOW_FAR_A], bias_mtrx, shadow[SHADOW_FAR_A].matrix);
                mat4_multiply(main_ubo.shadow_space[SHADOW_FAR_B], bias_mtrx, shadow[SHADOW_FAR_B].matrix);
                mat4_multiply(main_ubo.shadow_space[SHADOW_EXT_A], bias_mtrx, shadow[SHADOW_EXT_A].matrix);
                mat4_multiply(main_ubo.shadow_space[SHADOW_EXT_B], bias_mtrx, shadow[SHADOW_EXT_B].matrix);
        }
}

void shadow_render(VkCommandBuffer cmdbuf)
{
        if (shadow_mapping) {
                // Render shadow passes with per-cascade bias (using pre-built visible chunk list)
                // Near cascade: rendered every frame with PCF
                draw_shadow_pass(cmdbuf, SHADOW_NEAR, 1.5f, 1.5f, 1);
                if (gpu_timestamp_pool) vkCmdWriteTimestamp(cmdbuf, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, gpu_timestamp_pool, GPU_TS_SHADOW_N_END);

                // Mid cascade: rendered every frame, no PCF, no A/B blending
                draw_shadow_pass(cmdbuf, SHADOW_MID, 1.0f, 1.0f, 2);
                if (gpu_timestamp_pool) vkCmdWriteTimestamp(cmdbuf, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, gpu_timestamp_pool, GPU_TS_SHADOW_M_END);

                // Far cascade: render to A or B based on which needs updating
                if (shadow_far_render_ab >= 0) {
                        int far_cascade = (shadow_far_render_ab == 0) ? SHADOW_FAR_A : SHADOW_FAR_B;
                        draw_shadow_pass(cmdbuf, far_cascade, 0.5f, 0.5f, 4);
                }
                if (gpu_timestamp_pool) vkCmdWriteTimestamp(cmdbuf, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, gpu_timestamp_pool, GPU_TS_SHADOW_F_END);

                // Extreme cascade: render to A or B based on which needs updating
                if (shadow_ext_render_ab >= 0) {
                        int ext_cascade = (shadow_ext_render_ab == 0) ? SHADOW_EXT_A : SHADOW_EXT_B;
                        draw_shadow_pass(cmdbuf, ext_cascade, 0.5f, 0.5f, 8);
                }
                if (gpu_timestamp_pool) vkCmdWriteTimestamp(cmdbuf, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, gpu_timestamp_pool, GPU_TS_SHADOW_X_END);
        } else {
                // Write shadow timestamps even when disabled (so GPU timing display works)
                if (gpu_timestamp_pool) {
                        vkCmdWriteTimestamp(cmdbuf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, gpu_timestamp_pool, GPU_TS_SHADOW_N_END);
                        vkCmdWriteTimestamp(cmdbuf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, gpu_timestamp_pool, GPU_TS_SHADOW_M_END);
                        vkCmdWriteTimestamp(cmdbuf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, gpu_timestamp_pool, GPU_TS_SHADOW_F_END);
                        vkCmdWriteTimestamp(cmdbuf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, gpu_timestamp_pool, GPU_TS_SHADOW_X_END);
                }
        }
}

#endif // BLOCKO_SHADOW_C_INCLUDED

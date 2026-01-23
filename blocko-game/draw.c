#include "blocko.c"
#ifndef BLOCKO_DRAW_C_INCLUDED
#define BLOCKO_DRAW_C_INCLUDED

int sorter(const void * _a, const void * _b)
{
        const struct qitem *a = _a;
        const struct qitem *b = _b;
        return (a->y == b->y) ?  0 :
               (a->y <  b->y) ? -1 : 1;  // closest first
}

int chunk_in_frustum(float *matrix, int chunk_x, int chunk_z)
{
        int x_too_lo = 0;
        int x_too_hi = 0;
        int y_too_lo = 0;
        int y_too_hi = 0;
        int z_too_lo = 0;
        int z_too_hi = 0;
        int w_too_lo = 0;

        for (int x = 0; x <= 1; x++) for (int z = 0; z <= 1; z++) for (int y = 0; y <= 1; y++)
        {
                float v[4];
                mat4_f3_multiply(v, matrix,
                                chunk_x*BS*CHUNKW + x*BS*CHUNKW,
                                0 + y*BS*TILESH, // TODO: use highest gndheight?
                                chunk_z*BS*CHUNKD + z*BS*CHUNKD);
                if (v[0] < -v[3]) x_too_lo++;
                if (v[0] >  v[3]) x_too_hi++;
                if (v[1] < -v[3]) y_too_lo++;
                if (v[1] >  v[3]) y_too_hi++;
                if (v[2] < -v[3]) z_too_lo++;
                if (v[2] >  v[3]) z_too_hi++;
                if (v[3] <   0.f) w_too_lo++;
        }

        return x_too_lo != 8 && x_too_hi != 8 &&
               y_too_lo != 8 && y_too_hi != 8 &&
               z_too_lo != 8 && z_too_hi != 8 &&
               w_too_lo != 8;
}

int chunk_in_range(int chunk_x, int chunk_z)
{
        float draw_dist_sq = draw_dist * BS * draw_dist * BS;
        float delta_x = camplayer.pos.x - (chunk_x + .5f) * BS * CHUNKW;
        float delta_z = camplayer.pos.z - (chunk_z + .5f) * BS * CHUNKD;
        float dist_sq = delta_x * delta_x + delta_z * delta_z;
        return dist_sq < draw_dist_sq;
}

// Draw shadow pass to a specific framebuffer
// bias_constant and bias_slope are per-cascade depth bias values
void draw_shadow_pass(VkCommandBuffer cmdbuf, VkFramebuffer fb, float *shadow_pv, float bias_constant, float bias_slope)
{

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

        VkDeviceSize voffset = 0;
        for (int i = 0; i < VAOW; i++) {
                for (int j = 0; j < VAOD; j++) {
                        if (!VBOLEN_(i, j)) continue;
                        if (!chunk_in_frustum(shadow_pv, i, j)) continue;
                        if (!chunk_in_range(i, j)) continue;

                        push.chunk_x = i * BS * CHUNKW;
                        push.chunk_y = 0;
                        push.chunk_z = j * BS * CHUNKD;
                        vkCmdPushConstants(cmdbuf, vk.pipelines[shadow_pipe].layout,
                                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                                0, sizeof push, &push);
                        vkCmdBindVertexBuffers(cmdbuf, 0, 1, &world_buf[i * VAOD + j], &voffset);
                        vkCmdDraw(cmdbuf, VBOLEN_(i, j), 1, 0, 0);
                        shadow_polys += VBOLEN_(i, j);
                }
        }

        vkCmdEndRenderPass(cmdbuf);
}

//draw everything in the game on the screen
void draw_stuff()
{
        TIMER(gpu_sync);
        vulkan_acquire_next();
        TIMER(shadow_calc);
        struct main_ubo main_ubo = {0};

        float identity_mtrx[] = {
                1, 0, 0, 0,
                0, 1, 0, 0,
                0, 0, 1, 0,
                0, 0, 0, 1,
        };

        //glDisable(GL_MULTISAMPLE);

        memcpy(main_ubo.model, identity_mtrx, sizeof identity_mtrx);

        // Calculate sun/moon positions (used for lighting and shadows)
        float dist2sun = TILESW * BS;
        float chunk_size = BS * CHUNKW;
        float shadow_target[3] = {
                camplayer.pos.x,
                camplayer.pos.y,
                camplayer.pos.z,
        };

        // Original tilted sun path (non-equatorial for interesting lighting)
        sun_pos.x = shadow_target[0] + dist2sun * (cosf(sun_pitch) * cosf(sun_yaw));
        sun_pos.y = shadow_target[1] + dist2sun * (cosf(sun_pitch) * sinf(sun_yaw) * cosf(sun_roll) + sinf(sun_pitch) * sinf(sun_roll));
        sun_pos.z = shadow_target[2] + dist2sun * (cosf(sun_pitch) * sinf(sun_yaw) * sinf(sun_roll) - sinf(sun_pitch) * cosf(sun_roll));

        float moon_pitch = sun_pitch + PI;
        if (moon_pitch >= TAU) moon_pitch -= TAU;
        moon_pos.x = shadow_target[0] + dist2sun * (cosf(moon_pitch) * cosf(sun_yaw));
        moon_pos.y = shadow_target[1] + dist2sun * (cosf(moon_pitch) * sinf(sun_yaw) * cosf(sun_roll) + sinf(moon_pitch) * sinf(sun_roll));
        moon_pos.z = shadow_target[2] + dist2sun * (cosf(moon_pitch) * sinf(sun_yaw) * sinf(sun_roll) - sinf(moon_pitch) * cosf(sun_roll));

        // Store shadow PV matrices for rendering
        float shadow_pv_mtrx0[16] = {0};  // Near cascade (rendered every frame)
        // Mid/far cascades use shadow2a/b_matrix and shadow3a/b_matrix from defs.c

        // Quantization step for mid/far cascades (radians)
        const float MID_QUANT_STEP = 0.002f;
        const float FAR_QUANT_STEP = 0.002f;

        // Determine effective light pitch (always positive angle for slot calculation)
        float light_pitch = sun_pitch;
        if (light_pitch < 0) light_pitch += TAU;

        // Compute current slots for mid/far cascades
        int mid_slot = (int)floorf(light_pitch / MID_QUANT_STEP);
        int far_slot = (int)floorf(light_pitch / FAR_QUANT_STEP);

        // Alternating A/B shadow maps based on slot parity:
        // - Even slot: A=slot (lo), B=slot+1 (hi), blend 0→1
        // - Odd slot:  B=slot (lo), A=slot+1 (hi), blend 1→0
        // This ensures when crossing a boundary, the shadow map that was "hi" becomes "lo"
        // without needing to copy matrices.

        int mid_slot_is_even = (mid_slot % 2) == 0;
        int far_slot_is_even = (far_slot % 2) == 0;

        // Determine which slot each shadow map should be at
        int mid_a_slot = mid_slot_is_even ? mid_slot : mid_slot + 1;  // A = even slots
        int mid_b_slot = mid_slot_is_even ? mid_slot + 1 : mid_slot;  // B = odd slots
        int far_a_slot = far_slot_is_even ? far_slot : far_slot + 1;
        int far_b_slot = far_slot_is_even ? far_slot + 1 : far_slot;

        // Blend factor: position within current slot (0→1)
        float mid_blend_raw = (light_pitch - mid_slot * MID_QUANT_STEP) / MID_QUANT_STEP;
        float far_blend_raw = (light_pitch - far_slot * FAR_QUANT_STEP) / FAR_QUANT_STEP;

        // When slot is even: blend from A(0) to B(1), so use raw blend
        // When slot is odd:  blend from B(0) to A(1), so use 1-raw blend
        main_ubo.shadow2_blend = mid_slot_is_even ? mid_blend_raw : (1.0f - mid_blend_raw);
        main_ubo.shadow3_blend = far_slot_is_even ? far_blend_raw : (1.0f - far_blend_raw);

        // Static frame counter for alternating A/B renders
        static int shadow_frame = 0;
        shadow_frame++;

        // Alternate which shadow map to render each frame
        shadow2_render_index = (shadow_frame % 2);  // 0=A, 1=B
        shadow3_render_index = ((shadow_frame + 1) % 2);  // Offset so not both on same frame

        // Track which slot each shadow map needs to be rendered at
        // (these are used when rendering to know what angle to use)
        shadow2a_slot = mid_a_slot;
        shadow2b_slot = mid_b_slot;
        shadow3a_slot = far_a_slot;
        shadow3b_slot = far_b_slot;

        // Helper to compute light position from pitch
        #define COMPUTE_LIGHT_POS(pitch, lp) do { \
                lp[0] = shadow_target[0] + dist2sun * (cosf(pitch) * cosf(sun_yaw)); \
                lp[1] = shadow_target[1] + dist2sun * (cosf(pitch) * sinf(sun_yaw) * cosf(sun_roll) + sinf(pitch) * sinf(sun_roll)); \
                lp[2] = shadow_target[2] + dist2sun * (cosf(pitch) * sinf(sun_yaw) * sinf(sun_roll) - sinf(pitch) * cosf(sun_roll)); \
        } while(0)

        // compute shadow matrices and render shadow maps
        // CASCADE SIZES: Near=5000, Mid=50000, Far=500000
        TIMER(shadow_render);
        if (shadow_mapping) for(int s = 0; s < 3; s++)
        {
                float light_pos[3];
                float render_pitch;

                if (s == 0) {
                        // Near cascade: use current sun pitch
                        render_pitch = sun_pitch;
                } else if (s == 1) {
                        // Mid cascade: render at the slot assigned to A or B
                        if (shadow2_render_index == 0) {
                                render_pitch = shadow2a_slot * MID_QUANT_STEP;
                        } else {
                                render_pitch = shadow2b_slot * MID_QUANT_STEP;
                        }
                } else {
                        // Far cascade: render at the slot assigned to A or B
                        if (shadow3_render_index == 0) {
                                render_pitch = shadow3a_slot * FAR_QUANT_STEP;
                        } else {
                                render_pitch = shadow3b_slot * FAR_QUANT_STEP;
                        }
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
                // Cascade sizes - tweak these values as needed
                float snear = (s == 0 ? 10.f : s == 1 ? 80.f : 200.f);
                float sfar = dist2sun * 2;
                float mag = (s == 0 ? 5000.f : s == 1 ? 50000.f : 500000.f);

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

                // Store matrices
                if (s == 0) {
                        // Near cascade: render every frame
                        memcpy(shadow_pv_mtrx0, shadow_pv_mtrx, sizeof shadow_pv_mtrx);
                        memcpy(main_ubo.shadow_space, biased_shadow_mtrx, sizeof biased_shadow_mtrx);
                } else if (s == 1) {
                        // Mid cascade: store in A or B based on which we're rendering
                        if (shadow2_render_index == 0)
                                memcpy(shadow2a_matrix, shadow_pv_mtrx, sizeof shadow_pv_mtrx);
                        else
                                memcpy(shadow2b_matrix, shadow_pv_mtrx, sizeof shadow_pv_mtrx);
                        mat4_multiply(main_ubo.shadow2a_space, bias_mtrx, shadow2a_matrix);
                        mat4_multiply(main_ubo.shadow2b_space, bias_mtrx, shadow2b_matrix);
                } else {
                        // Far cascade: store in A or B based on which we're rendering
                        if (shadow3_render_index == 0)
                                memcpy(shadow3a_matrix, shadow_pv_mtrx, sizeof shadow_pv_mtrx);
                        else
                                memcpy(shadow3b_matrix, shadow_pv_mtrx, sizeof shadow_pv_mtrx);
                        mat4_multiply(main_ubo.shadow3a_space, bias_mtrx, shadow3a_matrix);
                        mat4_multiply(main_ubo.shadow3b_space, bias_mtrx, shadow3b_matrix);
                }

                for (int i = 0; i < VAOW; i++) for (int j = 0; j < VAOD; j++)
                {
                        if (!VBOLEN_(i, j)) continue;
                        int passes_vis_test = chunk_in_frustum(shadow_pv_mtrx, i, j) && chunk_in_range(i, j);
                        if (!frustum_culling || passes_vis_test)
                        {
                                //glBindVertexArray(VAO_(i, j));
                                main_ubo.model[12] = i * BS * CHUNKW;
                                main_ubo.model[14] = j * BS * CHUNKD;
                                //glUniformMatrix4fv(glGetUniformLocation(shadow_prog_id, "model"), 1, GL_FALSE, model_mtrx);
                                //glDrawArrays(GL_POINTS, 0, VBOLEN_(i, j));
                                shadow_polys += VBOLEN_(i, j);
                        }
                }

                fb_is_bad: ;
                //glBindFramebuffer(GL_FRAMEBUFFER, 0);
                //glDisable(GL_POLYGON_OFFSET_FILL);
        }

        do_atmos_colors();
        TIMER(frame_setup);

        //glViewport(0, 0, screenw, screenw);
        //glClearColor(fog_r, fog_g, fog_b, 1.f);
        //glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        if (antialiasing)
                ;//glEnable(GL_MULTISAMPLE);

        // compute proj matrix (Vulkan: Y flipped, depth 0-1)
        float near = 100.f;
        float far = 1000000.f;  // 1000 blocks with BS=1000
        float fov_degrees = 90.f;  // horizontal FOV in degrees
        float frustw = near * tanf(fov_degrees * PI / 360.f) * zoom_amt;  // tan(fov/2)
        float frusth = frustw * screenh / screenw;
        static int fov_debug = 1;
        if (fov_debug) { fov_debug = 0; fprintf(stderr, "FOV: %.1f deg, zoom_amt=%.2f, frustw=%.1f, tan(fov/2)=%.4f\n", fov_degrees, zoom_amt, frustw, tanf(fov_degrees * PI / 360.f)); }
        float proj_mtrx[] = {
                near/frustw,            0,                              0,  0,
                          0, -near/frusth,                              0,  0,  // negated for Vulkan
                          0,            0,         -far / (far - near), -1,     // Vulkan depth 0-1
                          0,            0, -(far * near) / (far - near),  0
        };

        // compute view matrix
        float eye0 = lerped_pos.x + PLYR_W / 2;
        float eye1 = lerped_pos.y + EYEDOWN * (camplayer.sneaking ? 2 : 1);
        float eye2 = lerped_pos.z + PLYR_W / 2;
        float f[3];
        float view_mtrx[16];
        lookit(view_mtrx, f, eye0, eye1, eye2, camplayer.pitch, camplayer.yaw);

        // find where we are pointing at
        rayshot(eye0, eye1, eye2, f[0], f[1], f[2]);

        // translate by hand
        float translated_view_mtrx[16];
        memcpy(translated_view_mtrx, view_mtrx, sizeof view_mtrx);
        translate(translated_view_mtrx, -eye0, -eye1, -eye2);

        static float proj_view_mtrx[16];
        if (!lock_culling)
                mat4_multiply(proj_view_mtrx, proj_mtrx, translated_view_mtrx);

        //glEnable(GL_BLEND);
        //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        //glEnable(GL_DEPTH_TEST);
        //glDepthFunc(GL_LEQUAL);
        //glDepthMask(GL_TRUE);
        //glEnable(GL_CULL_FACE);
        //glCullFace(GL_BACK);

        //glUseProgram(prog_id);

        //glActiveTexture(GL_TEXTURE0);
        //glBindTexture(GL_TEXTURE_2D_ARRAY, material_tex_id);
        //glUniform1i(glGetUniformLocation(prog_id, "tarray"), 0);

        //glActiveTexture(GL_TEXTURE1);
        //glBindTexture(GL_TEXTURE_2D, shadow_tex_id);
        //glUniform1i(glGetUniformLocation(prog_id, "shadow_map"), 1);

        //glActiveTexture(GL_TEXTURE2);
        //glBindTexture(GL_TEXTURE_2D, shadow2_tex_id);
        //glUniform1i(glGetUniformLocation(prog_id, "shadow2_map"), 2);

        main_ubo.shadow_mapping = shadow_mapping;

        memcpy(main_ubo.proj, proj_mtrx, sizeof proj_mtrx);
        memcpy(main_ubo.view, translated_view_mtrx, sizeof translated_view_mtrx);

        main_ubo.bs = BS;

        if (sun_pitch < PI)
        {
                main_ubo.light_pos[0] = sun_pos.x;
                main_ubo.light_pos[1] = sun_pos.y;
                main_ubo.light_pos[2] = sun_pos.z;
        }
        else
        {
                main_ubo.light_pos[0] = moon_pos.x;
                main_ubo.light_pos[1] = moon_pos.y;
                main_ubo.light_pos[2] = moon_pos.z;
        }

        main_ubo.view_pos[0] = eye0;
        main_ubo.view_pos[1] = eye1;
        main_ubo.view_pos[2] = eye2;

        {
                float m = ICLAMP(night_amt * 2.f, 0.f, 1.f);
                main_ubo.sharpness = m*m*m*(m*(m*6.f-15.f)+10.f);

                float r = lerp(night_amt * night_amt, DAY_R, NIGHT_R);
                float g = lerp(night_amt, DAY_G, NIGHT_G);
                float b = lerp(night_amt, DAY_B, NIGHT_B);
                main_ubo.day_color[0] = r;
                main_ubo.day_color[1] = g;
                main_ubo.day_color[2] = b;
                if (frame % 120 == 0) fprintf(stderr, "night_amt=%.3f day_color=(%.2f,%.2f,%.2f) sun_pitch=%.2f\n",
                        night_amt, r, g, b, sun_pitch);
                main_ubo.glo_color[0] = 0.92f;
                main_ubo.glo_color[1] = 0.83f;
                main_ubo.glo_color[2] = 0.69f;
                main_ubo.fog_color[0] = fog_r;
                main_ubo.fog_color[1] = fog_g;
                main_ubo.fog_color[2] = fog_b;
                main_ubo.fog_lo = draw_dist * BS * 0.5f;
                main_ubo.fog_hi = draw_dist * BS * 1.0f;
                if (frame % 120 == 0) fprintf(stderr, "fog_lo=%.0f fog_hi=%.0f view_pos=(%.0f,%.0f,%.0f) draw_dist=%.0f BS=%d\n",
                        main_ubo.fog_lo, main_ubo.fog_hi, main_ubo.view_pos[0], main_ubo.view_pos[1], main_ubo.view_pos[2], draw_dist, BS);
        }

        // Mark newly generated chunks as dirty
        TIMER(collect_dirty)
        #pragma omp critical
        {
                for (size_t i = 0; i < just_gen_len; i++) {
                        int cx = just_generated[i].x;
                        int cz = just_generated[i].z;
                        DIRTY_(cx, cz) = 1;
                }
                just_gen_len = 0;
        }

        // Collect all dirty chunks with distances from current player position
        struct qitem fresh[VAOW*VAOD];
        size_t fresh_len = 0;

        // Player's chunk (avoid overflow by using chunk units, not world units)
        int player_chunk_x = (int)(eye0 / (BS * CHUNKW));
        int player_chunk_z = (int)(eye2 / (BS * CHUNKD));

        for (int i = 0; i < VAOW; i++) {
                for (int j = 0; j < VAOD; j++) {
                        if (!DIRTY_(i, j)) continue;
                        if (!AGEN_(i, j)) continue;

                        int xd = i - player_chunk_x;
                        int zd = j - player_chunk_z;
                        fresh[fresh_len].x = i;
                        fresh[fresh_len].y = xd * xd + zd * zd;
                        fresh[fresh_len].z = j;
                        fresh_len++;
                }
        }

        // sort by distance (closest first)
        qsort(fresh, fresh_len, sizeof(struct qitem), sorter);

        // render non-fresh chunks
        TIMER(draw_cached)
        struct qitem stale[VAOW * VAOD] = {0}; // chunkx, distance sq, chunkz
        size_t stale_len = 0;
        for (int i = 0; i < VAOW; i++) for (int j = 0; j < VAOD; j++)
        {
                // skip chunks we will draw fresh this frame
                size_t limit = show_fresh_updates ? fresh_len : 4;
                for (size_t k = 0; k < limit; k++)
                        if (fresh[k].x == i && fresh[k].z == j)
                                goto skip;

                stale[stale_len].x = i;
                stale[stale_len].z = j;
                int xd = i - player_chunk_x;
                int zd = j - player_chunk_z;
                stale[stale_len].y = xd * xd + zd * zd;

                // only queue chunks we could see
                int passes_vis_test = chunk_in_frustum(proj_view_mtrx, i, j) && chunk_in_range(i, j);
                if (passes_vis_test)
                        stale_len++;

                skip: ;
        }

        qsort(stale, stale_len, sizeof *stale, sorter);
        for (size_t my = 0; my < stale_len; my++)
        {
                int myx = stale[my].x;
                int myz = stale[my].z;
                main_ubo.model[12] = myx * BS * CHUNKW;
                main_ubo.model[14] = myz * BS * CHUNKD;
                //glBindVertexArray(VAO_(myx, myz));
                //glDrawArrays(GL_POINTS, 0, VBOLEN_(myx, myz));
                polys += VBOLEN_(myx, myz);
        }

        // package, ship and render fresh chunks (while the stales are rendering!)
        TIMER(build_meshes);
        int meshes_built = 0;
        for (size_t my = 0; my < fresh_len && meshes_built < MAX_MESHES_PER_FRAME; my++)
        {
                int myx = fresh[my].x;
                int myz = fresh[my].z;
                int xlo = myx * CHUNKW;
                int xhi = xlo + CHUNKW;
                int zlo = myz * CHUNKD;
                int zhi = zlo + CHUNKD;

                // Skip chunks outside frustum or range
                if (!chunk_in_frustum(proj_view_mtrx, myx, myz) || !chunk_in_range(myx, myz))
                        continue;

                //glBindVertexArray(VAO_(myx, myz));
                //glBindBuffer(GL_ARRAY_BUFFER, VBO_(myx, myz));
                v = vbuf; // reset vertex buffer pointer
                w = wbuf; // same for water buffer

                TIMER(build_meshes);

                // Track per-thread vertex counts for merging
                int v_counts[MAX_MESH_THREADS] = {0};
                int w_counts[MAX_MESH_THREADS] = {0};

                #pragma omp parallel
                {
                        int tid = omp_get_thread_num();
                        struct vbufv *tv = vbuf_mt[tid];
                        struct vbufv *tv_start = tv;
                        struct vbufv *tv_limit = tv + VERTEX_BUFLEN;
                        struct vbufv *tw = wbuf_mt[tid];
                        struct vbufv *tw_start = tw;

                        #pragma omp for schedule(static)
                        for (int z = zlo; z < zhi; z++) {
                        for (int y = 0; y < TILESH; y++) for (int x = xlo; x < xhi; x++)
                        {
                                if (tv >= tv_limit) break;

                                if (T_(x, y, z) == OPEN && (!show_light_values || !in_test_area(x, y, z)))
                                        continue;

                                //lighting
                                float usw = CORN_(x  , y  , z  );
                                float use = CORN_(x+1, y  , z  );
                                float unw = CORN_(x  , y  , z+1);
                                float une = CORN_(x+1, y  , z+1);
                                float dsw = CORN_(x  , y+1, z  );
                                float dse = CORN_(x+1, y+1, z  );
                                float dnw = CORN_(x  , y+1, z+1);
                                float dne = CORN_(x+1, y+1, z+1);
                                float USW = KORN_(x  , y  , z  );
                                float USE = KORN_(x+1, y  , z  );
                                float UNW = KORN_(x  , y  , z+1);
                                float UNE = KORN_(x+1, y  , z+1);
                                float DSW = KORN_(x  , y+1, z  );
                                float DSE = KORN_(x+1, y+1, z  );
                                float DNW = KORN_(x  , y+1, z+1);
                                float DNE = KORN_(x+1, y+1, z+1);
                                int t = T_(x, y, z);
                                int m = x & (CHUNKW-1);
                                int n = z & (CHUNKD-1);

                                if (t == GRAS)
                                {
                                        if (y == 0        || IS_SEE_THROUGH(T_(x  , y-1, z  ))) *tv++ = (struct vbufv){ 0,    UP, m, y, n, usw, use, unw, une, USW, USE, UNW, UNE, 1 };
                                        if (z == 0        || IS_SEE_THROUGH(T_(x  , y  , z-1))) *tv++ = (struct vbufv){ 1, SOUTH, m, y, n, use, usw, dse, dsw, USE, USW, DSE, DSW, 1 };
                                        if (z == TILESD-1 || IS_SEE_THROUGH(T_(x  , y  , z+1))) *tv++ = (struct vbufv){ 1, NORTH, m, y, n, unw, une, dnw, dne, UNW, UNE, DNW, DNE, 1 };
                                        if (x == 0        || IS_SEE_THROUGH(T_(x-1, y  , z  ))) *tv++ = (struct vbufv){ 1,  WEST, m, y, n, usw, unw, dsw, dnw, USW, UNW, DSW, DNW, 1 };
                                        if (x == TILESW-1 || IS_SEE_THROUGH(T_(x+1, y  , z  ))) *tv++ = (struct vbufv){ 1,  EAST, m, y, n, une, use, dne, dse, UNE, USE, DNE, DSE, 1 };
                                        if (y <  TILESH-1 && IS_SEE_THROUGH(T_(x  , y+1, z  ))) *tv++ = (struct vbufv){ 2,  DOWN, m, y, n, dse, dsw, dne, dnw, DSE, DSW, DNE, DNW, 1 };
                                }
                                else if (t == DIRT || t == GRG1 || t == GRG2)
                                {
                                        int u = (t == DIRT) ? 2 :
                                                (t == GRG1) ? 3 : 4;
                                        if (y == 0        || IS_SEE_THROUGH(T_(x  , y-1, z  ))) *tv++ = (struct vbufv){ u,    UP, m, y, n, usw, use, unw, une, USW, USE, UNW, UNE, 1 };
                                        if (z == 0        || IS_SEE_THROUGH(T_(x  , y  , z-1))) *tv++ = (struct vbufv){ 2, SOUTH, m, y, n, use, usw, dse, dsw, USE, USW, DSE, DSW, 1 };
                                        if (z == TILESD-1 || IS_SEE_THROUGH(T_(x  , y  , z+1))) *tv++ = (struct vbufv){ 2, NORTH, m, y, n, unw, une, dnw, dne, UNW, UNE, DNW, DNE, 1 };
                                        if (x == 0        || IS_SEE_THROUGH(T_(x-1, y  , z  ))) *tv++ = (struct vbufv){ 2,  WEST, m, y, n, usw, unw, dsw, dnw, USW, UNW, DSW, DNW, 1 };
                                        if (x == TILESW-1 || IS_SEE_THROUGH(T_(x+1, y  , z  ))) *tv++ = (struct vbufv){ 2,  EAST, m, y, n, une, use, dne, dse, UNE, USE, DNE, DSE, 1 };
                                        if (y <  TILESH-1 && IS_SEE_THROUGH(T_(x  , y+1, z  ))) *tv++ = (struct vbufv){ 2,  DOWN, m, y, n, dse, dsw, dne, dnw, DSE, DSW, DNE, DNW, 1 };
                                }
                                else if (t == STON || t == SAND || t == ORE || t == OREH || t == HARD || t == WOOD || t == GRAN ||
                                         t == RLEF || t == YLEF)
                                {
                                        int f = (t == STON) ?  5 :
                                                (t == SAND) ?  6 :
                                                (t == ORE ) ? 11 :
                                                (t == OREH) ? 12 :
                                                (t == HARD) ? 13 :
                                                (t == WOOD) ? 14 :
                                                (t == GRAN) ? 15 :
                                                (t == RLEF) ? 16 :
                                                (t == YLEF) ? 17 :
                                                               0 ;
                                        if (y == 0        || IS_SEE_THROUGH(T_(x  , y-1, z  ))) *tv++ = (struct vbufv){ f,    UP, m, y, n, usw, use, unw, une, USW, USE, UNW, UNE, 1 };
                                        if (z == 0        || IS_SEE_THROUGH(T_(x  , y  , z-1))) *tv++ = (struct vbufv){ f, SOUTH, m, y, n, use, usw, dse, dsw, USE, USW, DSE, DSW, 1 };
                                        if (z == TILESD-1 || IS_SEE_THROUGH(T_(x  , y  , z+1))) *tv++ = (struct vbufv){ f, NORTH, m, y, n, unw, une, dnw, dne, UNW, UNE, DNW, DNE, 1 };
                                        if (x == 0        || IS_SEE_THROUGH(T_(x-1, y  , z  ))) *tv++ = (struct vbufv){ f,  WEST, m, y, n, usw, unw, dsw, dnw, USW, UNW, DSW, DNW, 1 };
                                        if (x == TILESW-1 || IS_SEE_THROUGH(T_(x+1, y  , z  ))) *tv++ = (struct vbufv){ f,  EAST, m, y, n, une, use, dne, dse, UNE, USE, DNE, DSE, 1 };
                                        if (y <  TILESH-1 && IS_SEE_THROUGH(T_(x  , y+1, z  ))) *tv++ = (struct vbufv){ f,  DOWN, m, y, n, dse, dsw, dne, dnw, DSE, DSW, DNE, DNW, 1 };
                                }
                                else if (t == WATR)
                                {
                                        if (y == 0        || T_(x  , y-1, z  ) == OPEN)
                                        {
                                                int f = 7 + (pframe / 10 + (x ^ z)) % 4;
                                                *tw++ = (struct vbufv){ f,    UP, m, y+0.06f, n, usw, use, unw, une, USW, USE, UNW, UNE, 0.5f };
                                                *tw++ = (struct vbufv){ f,  DOWN, m, y-0.94f, n, dse, dsw, dne, dnw, DSE, DSW, DNE, DNW, 0.5f };
                                        }
                                }
                                else if (t == LITE)
                                {
                                        *tw++ = (struct vbufv){ 18, SOUTH, m     , y, n+0.5f, use, usw, dse, dsw, 1.3f, 1.3f, 1.3f, 1.3f, 1 };
                                        *tw++ = (struct vbufv){ 18, NORTH, m     , y, n-0.5f, unw, une, dnw, dne, 1.3f, 1.3f, 1.3f, 1.3f, 1 };
                                        *tw++ = (struct vbufv){ 18,  WEST, m+0.5f, y, n     , usw, unw, dsw, dnw, 1.3f, 1.3f, 1.3f, 1.3f, 1 };
                                        *tw++ = (struct vbufv){ 18,  EAST, m-0.5f, y, n     , une, use, dne, dse, 1.3f, 1.3f, 1.3f, 1.3f, 1 };
                                }

                                if (show_light_values && in_test_area(x, y, z))
                                {
                                        int f = MAX(GLO_(x, y, z), SUN_(x, y, z)) + PNG0;
                                        int ty = y;
                                        float lit = 1.f;
                                        if (IS_OPAQUE(x, y, z))
                                        {
                                                ty = y - 1;
                                                lit = 0.1f;
                                        }
                                        *tw++ = (struct vbufv){ f,    UP, m, ty+0.99f, n, lit, lit, lit, lit, lit, lit, lit, lit, 1.f };
                                        *tw++ = (struct vbufv){ f,  DOWN, m, ty-0.01f, n, lit, lit, lit, lit, lit, lit, lit, lit, 1.f };
                                }
                        }
                        }

                        // Store counts for this thread
                        v_counts[tid] = tv - tv_start;
                        w_counts[tid] = tw - tw_start;
                }

                // Merge thread buffers into main buffer
                for (int tid = 0; tid < MAX_MESH_THREADS; tid++)
                {
                        if (v_counts[tid] > 0) {
                                memcpy(v, vbuf_mt[tid], v_counts[tid] * sizeof *v);
                                v += v_counts[tid];
                        }
                        if (w_counts[tid] > 0) {
                                memcpy(w, wbuf_mt[tid], w_counts[tid] * sizeof *w);
                                w += w_counts[tid];
                        }
                }

                if (w - wbuf < v_limit - v) // room for water in vertex buffer?
                {
                        memcpy(v, wbuf, (w - wbuf) * sizeof *wbuf);
                        v += w - wbuf;
                }

                VBOLEN_(myx, myz) = v - vbuf;
                polys += VBOLEN_(myx, myz);
                TIMER(gpu_upload)

                // Debug: print first 3 vertices of first chunk
                static int debug_once = 0;
                if (!debug_once && VBOLEN_(myx, myz) >= 3) {
                        debug_once = 1;
                        fprintf(stderr, "Chunk (%d,%d) first 3 verts:\n", myx, myz);
                        for (int dv = 0; dv < 3; dv++) {
                                fprintf(stderr, "  v[%d]: tex=%.0f orient=%.0f pos=(%.1f,%.1f,%.1f) illum=(%.2f,%.2f,%.2f,%.2f) alpha=%.1f\n",
                                        dv, vbuf[dv].tex, vbuf[dv].orient,
                                        vbuf[dv].x, vbuf[dv].y, vbuf[dv].z,
                                        vbuf[dv].illum0, vbuf[dv].illum1, vbuf[dv].illum2, vbuf[dv].illum3,
                                        vbuf[dv].alpha);
                        }
                }

                int offset = myz * world_aligned_sz;
                void *data = (char *)world_mapped[myx] + offset;
                memcpy(data, vbuf, (v - vbuf) * sizeof *vbuf);

                // Mark chunk as clean after rebuild
                DIRTY_(myx, myz) = 0;
                meshes_built++;

                //glBufferData(GL_ARRAY_BUFFER, VBOLEN_(myx, myz) * sizeof *vbuf, vbuf, GL_STATIC_DRAW);
        }

        // Upload UBO to per-frame buffer
        {
                int frame = vk.currentFrame;
                if (frame >= MAX_FRAMES_IN_FLIGHT) {
                        fprintf(stderr, "ERROR: frame %d >= MAX_FRAMES_IN_FLIGHT %d\n", frame, MAX_FRAMES_IN_FLIGHT);
                }
                void* data;
                vkMapMemory(vk.device, main_memory[frame], 0, sizeof main_ubo, 0, &data);
                memcpy(data, &main_ubo, sizeof main_ubo);
                vkUnmapMemory(vk.device, main_memory[frame]);
        }

        VkCommandBuffer cmdbuf = vk.commandBuffers[vk.imageIndex];

        // Render shadow maps (before main render pass)
        if (shadow_mapping) {
                // End the auto-started main render pass
                vkCmdEndRenderPass(cmdbuf);

                // Render shadow passes with per-cascade bias
                // Near cascade: rendered every frame
                draw_shadow_pass(cmdbuf, shadow_framebuffer, shadow_pv_mtrx0, 1.5f, 1.5f);

                // Mid cascade: render to A or B based on which needs updating
                if (shadow2_render_index >= 0) {
                        VkFramebuffer mid_fb = (shadow2_render_index == 0) ? shadow2a_framebuffer : shadow2b_framebuffer;
                        float *mid_pv = (shadow2_render_index == 0) ? shadow2a_matrix : shadow2b_matrix;
                        draw_shadow_pass(cmdbuf, mid_fb, mid_pv, 0.5f, 0.5f);
                }

                // Far cascade: render to A or B based on which needs updating
                if (shadow3_render_index >= 0) {
                        VkFramebuffer far_fb = (shadow3_render_index == 0) ? shadow3a_framebuffer : shadow3b_framebuffer;
                        float *far_pv = (shadow3_render_index == 0) ? shadow3a_matrix : shadow3b_matrix;
                        draw_shadow_pass(cmdbuf, far_fb, far_pv, 0.5f, 0.5f);
                }

                // Restart main render pass
                VkClearValue clearValues[2] = {
                        {.color = {{fog_r, fog_g, fog_b, 1.0f}}},
                        {.depthStencil = {1.0f, 0}}
                };
                VkRenderPassBeginInfo renderPassBeginInfo = {
                        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                        .renderPass = vk.renderPass,
                        .framebuffer = vk.framebuffers[vk.imageIndex],
                        .renderArea = {{0, 0}, {vk.bestSwapchainExtent.width, vk.bestSwapchainExtent.height}},
                        .clearValueCount = 2,
                        .pClearValues = clearValues,
                };
                vkCmdBeginRenderPass(cmdbuf, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
        }

        // Render sky first (behind everything)
        sky_draw(cmdbuf, proj_mtrx, view_mtrx);
        sun_draw(cmdbuf, proj_mtrx, view_mtrx, sun_pitch, sun_yaw, sun_roll);

        // Render terrain
        TIMER(draw_terrain);
        vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipelines[main_pipe].pipeline);

        VkViewport viewport = { 0, 0, vk.bestSwapchainExtent.width, vk.bestSwapchainExtent.height, 0, 1 };
        VkRect2D scissor = { {0, 0}, {vk.bestSwapchainExtent.width, vk.bestSwapchainExtent.height} };
        vkCmdSetViewport(cmdbuf, 0, 1, &viewport);
        vkCmdSetScissor(cmdbuf, 0, 1, &scissor);

        vkCmdBindDescriptorSets(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS,
                vk.pipelines[main_pipe].layout, 0, 1, &main_descriptor_set[vk.currentFrame], 0, NULL);

        struct { float pv[16]; float chunk_x; float chunk_y; float chunk_z; float bs; } push;
        memcpy(push.pv, proj_view_mtrx, sizeof push.pv);
        push.bs = BS;

        VkDeviceSize voffset = 0;
        int chunks_drawn = 0;
        int total_verts = 0;
        int chunks_with_data = 0;
        for (int i = 0; i < VAOW; i++) {
                for (int j = 0; j < VAOD; j++) {
                        if (VBOLEN_(i, j) > 0) chunks_with_data++;
                        if (!VBOLEN_(i, j)) continue;
                        if (!chunk_in_frustum(proj_view_mtrx, i, j) || !chunk_in_range(i, j)) continue;

                        push.chunk_x = i * BS * CHUNKW;
                        push.chunk_y = 0;
                        push.chunk_z = j * BS * CHUNKD;
                        vkCmdPushConstants(cmdbuf, vk.pipelines[main_pipe].layout,
                                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof push, &push);
                        vkCmdBindVertexBuffers(cmdbuf, 0, 1, &world_buf[i * VAOD + j], &voffset);
                        vkCmdDraw(cmdbuf, VBOLEN_(i, j), 1, 0, 0);
                        chunks_drawn++;
                        total_verts += VBOLEN_(i, j);
                }
        }
        if (frame % 60 == 0) fprintf(stderr, "Drew %d/%d chunks, %d verts. Player: %d,%d scoot: %d,%d\n",
                chunks_drawn, chunks_with_data, total_verts,
                (int)(camplayer.pos.x / BS / CHUNKW), (int)(camplayer.pos.z / BS / CHUNKD),
                chunk_scootx, chunk_scootz);

        if (mouselook) cursor(cmdbuf);

        debrief();

        TIMER(frame_submit);
        vulkan_submit();
        TIMER();
}

#endif // BLOCKO_DRAW_C_INCLUDED

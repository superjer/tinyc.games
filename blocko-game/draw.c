#include "blocko.c"
#ifndef BLOCKO_DRAW_C_INCLUDED
#define BLOCKO_DRAW_C_INCLUDED

// Visible chunk info for single-pass frustum culling
struct visible_chunk {
        int x, z;           // chunk indices
        unsigned char shadow_mask;  // bitmask: which shadow cascades see this chunk
        unsigned char camera_visible;  // true if visible to main camera
};
struct visible_chunk visible_chunks[VAOW * VAOD];
int visible_chunk_count = 0;

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
// poly_counter is incremented with the number of polygons rendered
// cascade_bit: which bit in shadow_mask to check (1=Near, 2=Mid, 4=Far, 8=Extreme)
void draw_shadow_pass(VkCommandBuffer cmdbuf, VkFramebuffer fb, float *shadow_pv, float bias_constant, float bias_slope, int *poly_counter, unsigned char cascade_bit)
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
                vkCmdDraw(cmdbuf, VBOLEN_(i, j), 1, 0, 0);
                shadow_polys += VBOLEN_(i, j);
                *poly_counter += VBOLEN_(i, j);
                cascade_x_draw_calls++;
        }

        if (cascade_bit == 8) fprintf(stderr, "visible_chunk_count: %d, CX draw calls: %d\n",
                visible_chunk_count, cascade_x_draw_calls);

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
        float shadow_pv_mtrx1[16] = {0};  // Mid cascade (rendered every frame)
        // Far/extreme cascades use shadow3a/b_matrix and shadow4a/b_matrix from defs.c

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
        shadow3a_slot = far_slot_is_even ? far_slot : far_slot + 1;  // A = even slots
        shadow3b_slot = far_slot_is_even ? far_slot + 1 : far_slot;  // B = odd slots
        shadow4a_slot = extreme_slot_is_even ? extreme_slot : extreme_slot + 1;
        shadow4b_slot = extreme_slot_is_even ? extreme_slot + 1 : extreme_slot;

        // Blend factor: position within current slot (0→1)
        float far_blend_raw = (light_pitch - far_slot * FAR_QUANT_STEP) / FAR_QUANT_STEP;
        float extreme_blend_raw = (light_pitch - extreme_slot * EXTREME_QUANT_STEP) / EXTREME_QUANT_STEP;

        // When slot is even: blend from A(0) to B(1), so use raw blend
        // When slot is odd:  blend from B(0) to A(1), so use 1-raw blend
        main_ubo.shadow3_blend = far_slot_is_even ? far_blend_raw : (1.0f - far_blend_raw);
        main_ubo.shadow4_blend = extreme_slot_is_even ? extreme_blend_raw : (1.0f - extreme_blend_raw);

        // Static frame counter for alternating A/B renders
        static int shadow_frame = 0;
        shadow_frame++;

        // Alternate which shadow map to render each frame (only for far/extreme)
        shadow3_render_index = (shadow_frame % 2);  // 0=A, 1=B
        shadow4_render_index = ((shadow_frame + 1) % 2);  // Offset so not both on same frame

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
                float light_pos[3];
                float render_pitch;

                if (s == 0 || s == 1) {
                        // Near and Mid cascades: use current light pitch (sun or moon)
                        render_pitch = light_pitch;
                } else if (s == 2) {
                        // Far cascade: render at the slot assigned to A or B
                        if (shadow3_render_index == 0) {
                                render_pitch = shadow3a_slot * FAR_QUANT_STEP;
                        } else {
                                render_pitch = shadow3b_slot * FAR_QUANT_STEP;
                        }
                } else {
                        // Extreme cascade: render at the slot assigned to A or B
                        if (shadow4_render_index == 0) {
                                render_pitch = shadow4a_slot * EXTREME_QUANT_STEP;
                        } else {
                                render_pitch = shadow4b_slot * EXTREME_QUANT_STEP;
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
                float snear = (s == 0 ? 10.f : s == 1 ? 40.f : s == 2 ? 80.f : 200.f);
                float sfar = dist2sun * 2;
                float mag = (s == 0 ? 5000.f : s == 1 ? 20000.f : s == 2 ? 50000.f : 500000.f);

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
                        // Mid cascade: render every frame (no A/B blending)
                        memcpy(shadow_pv_mtrx1, shadow_pv_mtrx, sizeof shadow_pv_mtrx);
                        memcpy(main_ubo.shadow2_space, biased_shadow_mtrx, sizeof biased_shadow_mtrx);
                } else if (s == 2) {
                        // Far cascade: store in A or B based on which we're rendering
                        if (shadow3_render_index == 0)
                                memcpy(shadow3a_matrix, shadow_pv_mtrx, sizeof shadow_pv_mtrx);
                        else
                                memcpy(shadow3b_matrix, shadow_pv_mtrx, sizeof shadow_pv_mtrx);
                        mat4_multiply(main_ubo.shadow3a_space, bias_mtrx, shadow3a_matrix);
                        mat4_multiply(main_ubo.shadow3b_space, bias_mtrx, shadow3b_matrix);
                } else {
                        // Extreme cascade: store in A or B based on which we're rendering
                        if (shadow4_render_index == 0)
                                memcpy(shadow4a_matrix, shadow_pv_mtrx, sizeof shadow_pv_mtrx);
                        else
                                memcpy(shadow4b_matrix, shadow_pv_mtrx, sizeof shadow_pv_mtrx);
                        mat4_multiply(main_ubo.shadow4a_space, bias_mtrx, shadow4a_matrix);
                        mat4_multiply(main_ubo.shadow4b_space, bias_mtrx, shadow4b_matrix);
                }
        }

        do_atmos_colors();
        TIMER(frame_setup);

        if (antialiasing)
                ;//glEnable(GL_MULTISAMPLE);

        // compute proj matrix
        float near = 100.f;
        float far = 1000 * BS;  // 1000 blocks with BS=1000
        float fov_degrees = 90.f;  // horizontal FOV in degrees
        float frustw = near * tanf(fov_degrees * PI / 360.f) * zoom_amt;
        float frusth = frustw * screenh / screenw;
        float proj_mtrx[] = {
                near/frustw,            0,                                0,  0,
                          0, -near/frusth,                                0,  0,
                          0,            0,          -far / (far - near), -1,
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
        mat4_multiply(proj_view_mtrx, proj_mtrx, translated_view_mtrx);

        // Build visible chunk list (single pass: check all frustums)
        visible_chunk_count = 0;
        float *far_shadow_pv = (shadow3_render_index == 0) ? shadow3a_matrix : shadow3b_matrix;
        float *extreme_shadow_pv = (shadow4_render_index == 0) ? shadow4a_matrix : shadow4b_matrix;
        for (int i = 0; i < VAOW; i++) {
                for (int j = 0; j < VAOD; j++) {
                        if (!VBOLEN_(i, j)) continue;

                        // Check camera visibility
                        int camera_visible = chunk_in_frustum(proj_view_mtrx, i, j) && chunk_in_range(i, j);

                        // Check shadow frustums
                        unsigned char shadow_mask = 0;
                        if (shadow_mapping) {
                                if (chunk_in_frustum(shadow_pv_mtrx0, i, j)) shadow_mask |= 1;  // Near
                                if (chunk_in_frustum(shadow_pv_mtrx1, i, j)) shadow_mask |= 2;  // Mid
                                if (shadow3_render_index >= 0 && chunk_in_frustum(far_shadow_pv, i, j)) shadow_mask |= 4;  // Far
                                // Extreme cascade: only include if camera-visible (too far to cast visible shadows from behind)
                                if (camera_visible && shadow4_render_index >= 0 && chunk_in_frustum(extreme_shadow_pv, i, j)) shadow_mask |= 8;
                        }

                        // Include if visible to camera OR any shadow frustum
                        if (!camera_visible && !shadow_mask) continue;

                        visible_chunks[visible_chunk_count].x = i;
                        visible_chunks[visible_chunk_count].z = j;
                        visible_chunks[visible_chunk_count].shadow_mask = shadow_mask;
                        visible_chunks[visible_chunk_count].camera_visible = camera_visible;
                        visible_chunk_count++;
                }
        }

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
                main_ubo.glo_color[0] = 0.92f;
                main_ubo.glo_color[1] = 0.83f;
                main_ubo.glo_color[2] = 0.69f;
                main_ubo.fog_color[0] = fog_r;
                main_ubo.fog_color[1] = fog_g;
                main_ubo.fog_color[2] = fog_b;
                main_ubo.fog_lo = draw_dist * BS * 0.5f;
                main_ubo.fog_hi = draw_dist * BS * 1.0f;

                // Sun strength and warmth for day/night cycle
                // Full brightness between PI/16 and PI-PI/16, dimming/warming only near horizons
                // During night: moonlight at constant strength, no warmth
                if (sun_pitch < PI) {
                        // Daytime
                        float transition = PI / 64.0f;  // ~11 degrees transition zone
                        float sunrise_end = transition;
                        float sunset_start = PI - transition;

                        if (sun_pitch < sunrise_end) {
                                // Sunrise transition: dim/warm → bright/white
                                float t = sun_pitch / sunrise_end;
                                main_ubo.sun_strength = t;
                                main_ubo.sun_warmth = 1.0f - t;
                        } else if (sun_pitch > sunset_start) {
                                // Sunset transition: bright/white → dim/warm
                                float t = (sun_pitch - sunset_start) / transition;
                                main_ubo.sun_strength = 1.0f - t;
                                main_ubo.sun_warmth = t;
                        } else {
                                // Full day: constant brightness, no warmth
                                main_ubo.sun_strength = 1.0f;
                                main_ubo.sun_warmth = 0.0f;
                        }
                        main_ubo.outside_cascade_lit = main_ubo.sun_strength;
                } else {
                        // Nighttime - moonlight fades in/out near horizons
                        float transition = PI / 16.0f;
                        float moonrise_end = PI + transition;
                        float moonset_start = TAU - transition;

                        if (sun_pitch < moonrise_end) {
                                // Moonrise transition: dim → bright, warm → cool
                                float t = (sun_pitch - PI) / transition;
                                main_ubo.sun_strength = t;
                                main_ubo.sun_warmth = 1.0f - t;  // Fade out warmth from sunset
                        } else if (sun_pitch > moonset_start) {
                                // Moonset transition: bright → dim, cool → warm
                                float t = (sun_pitch - moonset_start) / transition;
                                main_ubo.sun_strength = 1.0f - t;
                                main_ubo.sun_warmth = t;  // Fade in warmth for sunrise
                        } else {
                                // Full night: constant moonlight, no warmth
                                main_ubo.sun_strength = 1.0f;
                                main_ubo.sun_warmth = 0.0f;
                        }
                        main_ubo.outside_cascade_lit = 0.0f;      // Areas outside shadow cascade always dark at night
                }
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

        // Check LOD chunks for face visibility changes (skip if lock_culling for debugging)
        if (!lock_culling)
        for (int i = 0; i < VAOW; i++) {
                for (int j = 0; j < VAOD; j++) {
                        if (!LOD_(i, j)) continue;  // skip full-detail chunks
                        if (DIRTY_(i, j)) continue; // already dirty
                        if (!AGEN_(i, j)) continue; // not generated yet

                        // Calculate current face needs
                        float chunk_cx = (i + 0.5f) * CHUNKW * BS;
                        float chunk_cz = (j + 0.5f) * CHUNKD * BS;
                        float dx = camplayer.pos.x - chunk_cx;
                        float dz = camplayer.pos.z - chunk_cz;
                        float dist = sqrtf(dx*dx + dz*dz);
                        float dist_blocks = dist / BS;

                        // If close enough, switch to full detail
                        if (dist_blocks < LOD_DIST_THRESHOLD) {
                                DIRTY_(i, j) = 1;
                                continue;
                        }

                        // Check if any excluded face is now needed
                        float thresh = dist * LOD_ANGLE_SIN;
                        unsigned char needed = FACE_UP | FACE_DOWN;
                        if (dz > -thresh) needed |= FACE_NORTH;
                        if (dz <  thresh) needed |= FACE_SOUTH;
                        if (dx > -thresh) needed |= FACE_EAST;
                        if (dx <  thresh) needed |= FACE_WEST;

                        if (needed & ~FACES_(i, j))
                                DIRTY_(i, j) = 1;
                }
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
        /*
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
                polys += VBOLEN_(myx, myz);
        }
        */

        // package, ship and render fresh chunks
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

                // Calculate LOD face mask based on distance and viewing angle
                float chunk_cx = (myx + 0.5f) * CHUNKW * BS;
                float chunk_cz = (myz + 0.5f) * CHUNKD * BS;
                float dx = camplayer.pos.x - chunk_cx;
                float dz = camplayer.pos.z - chunk_cz;
                float dist = sqrtf(dx*dx + dz*dz);
                float dist_blocks = dist / BS;

                unsigned char face_mask;
                int use_lod;
                if (dist_blocks < LOD_DIST_THRESHOLD) {
                        // Close chunk: full detail
                        face_mask = FACE_ALL;
                        use_lod = 0;
                } else {
                        // Far chunk: cull backfaces with angular tolerance
                        float thresh = dist * LOD_ANGLE_SIN;
                        face_mask = FACE_UP | FACE_DOWN;  // always include vertical
                        if (dz > -thresh) face_mask |= FACE_NORTH;
                        if (dz <  thresh) face_mask |= FACE_SOUTH;
                        if (dx > -thresh) face_mask |= FACE_EAST;
                        if (dx <  thresh) face_mask |= FACE_WEST;
                        use_lod = 1;
                }

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
                                        if ((face_mask & FACE_UP)    && (y == 0        || IS_SEE_THROUGH(T_(x  , y-1, z  )))) *tv++ = (struct vbufv){ 0,    UP, m, y, n, usw, use, unw, une, USW, USE, UNW, UNE, 1 };
                                        if ((face_mask & FACE_SOUTH) && (z == 0        || IS_SEE_THROUGH(T_(x  , y  , z-1)))) *tv++ = (struct vbufv){ 1, SOUTH, m, y, n, use, usw, dse, dsw, USE, USW, DSE, DSW, 1 };
                                        if ((face_mask & FACE_NORTH) && (z == TILESD-1 || IS_SEE_THROUGH(T_(x  , y  , z+1)))) *tv++ = (struct vbufv){ 1, NORTH, m, y, n, unw, une, dnw, dne, UNW, UNE, DNW, DNE, 1 };
                                        if ((face_mask & FACE_WEST)  && (x == 0        || IS_SEE_THROUGH(T_(x-1, y  , z  )))) *tv++ = (struct vbufv){ 1,  WEST, m, y, n, usw, unw, dsw, dnw, USW, UNW, DSW, DNW, 1 };
                                        if ((face_mask & FACE_EAST)  && (x == TILESW-1 || IS_SEE_THROUGH(T_(x+1, y  , z  )))) *tv++ = (struct vbufv){ 1,  EAST, m, y, n, une, use, dne, dse, UNE, USE, DNE, DSE, 1 };
                                        if ((face_mask & FACE_DOWN)  && (y <  TILESH-1 && IS_SEE_THROUGH(T_(x  , y+1, z  )))) *tv++ = (struct vbufv){ 2,  DOWN, m, y, n, dse, dsw, dne, dnw, DSE, DSW, DNE, DNW, 1 };
                                }
                                else if (t == DIRT || t == GRG1 || t == GRG2)
                                {
                                        int u = (t == DIRT) ? 2 :
                                                (t == GRG1) ? 3 : 4;
                                        if ((face_mask & FACE_UP)    && (y == 0        || IS_SEE_THROUGH(T_(x  , y-1, z  )))) *tv++ = (struct vbufv){ u,    UP, m, y, n, usw, use, unw, une, USW, USE, UNW, UNE, 1 };
                                        if ((face_mask & FACE_SOUTH) && (z == 0        || IS_SEE_THROUGH(T_(x  , y  , z-1)))) *tv++ = (struct vbufv){ 2, SOUTH, m, y, n, use, usw, dse, dsw, USE, USW, DSE, DSW, 1 };
                                        if ((face_mask & FACE_NORTH) && (z == TILESD-1 || IS_SEE_THROUGH(T_(x  , y  , z+1)))) *tv++ = (struct vbufv){ 2, NORTH, m, y, n, unw, une, dnw, dne, UNW, UNE, DNW, DNE, 1 };
                                        if ((face_mask & FACE_WEST)  && (x == 0        || IS_SEE_THROUGH(T_(x-1, y  , z  )))) *tv++ = (struct vbufv){ 2,  WEST, m, y, n, usw, unw, dsw, dnw, USW, UNW, DSW, DNW, 1 };
                                        if ((face_mask & FACE_EAST)  && (x == TILESW-1 || IS_SEE_THROUGH(T_(x+1, y  , z  )))) *tv++ = (struct vbufv){ 2,  EAST, m, y, n, une, use, dne, dse, UNE, USE, DNE, DSE, 1 };
                                        if ((face_mask & FACE_DOWN)  && (y <  TILESH-1 && IS_SEE_THROUGH(T_(x  , y+1, z  )))) *tv++ = (struct vbufv){ 2,  DOWN, m, y, n, dse, dsw, dne, dnw, DSE, DSW, DNE, DNW, 1 };
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
                                        if ((face_mask & FACE_UP)    && (y == 0        || IS_SEE_THROUGH(T_(x  , y-1, z  )))) *tv++ = (struct vbufv){ f,    UP, m, y, n, usw, use, unw, une, USW, USE, UNW, UNE, 1 };
                                        if ((face_mask & FACE_SOUTH) && (z == 0        || IS_SEE_THROUGH(T_(x  , y  , z-1)))) *tv++ = (struct vbufv){ f, SOUTH, m, y, n, use, usw, dse, dsw, USE, USW, DSE, DSW, 1 };
                                        if ((face_mask & FACE_NORTH) && (z == TILESD-1 || IS_SEE_THROUGH(T_(x  , y  , z+1)))) *tv++ = (struct vbufv){ f, NORTH, m, y, n, unw, une, dnw, dne, UNW, UNE, DNW, DNE, 1 };
                                        if ((face_mask & FACE_WEST)  && (x == 0        || IS_SEE_THROUGH(T_(x-1, y  , z  )))) *tv++ = (struct vbufv){ f,  WEST, m, y, n, usw, unw, dsw, dnw, USW, UNW, DSW, DNW, 1 };
                                        if ((face_mask & FACE_EAST)  && (x == TILESW-1 || IS_SEE_THROUGH(T_(x+1, y  , z  )))) *tv++ = (struct vbufv){ f,  EAST, m, y, n, une, use, dne, dse, UNE, USE, DNE, DSE, 1 };
                                        if ((face_mask & FACE_DOWN)  && (y <  TILESH-1 && IS_SEE_THROUGH(T_(x  , y+1, z  )))) *tv++ = (struct vbufv){ f,  DOWN, m, y, n, dse, dsw, dne, dnw, DSE, DSW, DNE, DNW, 1 };
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

                int offset = myz * world_aligned_sz;
                void *data = (char *)world_mapped[myx] + offset;
                memcpy(data, vbuf, (v - vbuf) * sizeof *vbuf);

                // Mark chunk as clean after mesh rebuild and store LOD state
                DIRTY_(myx, myz) = 0;
                FACES_(myx, myz) = face_mask;
                LOD_(myx, myz) = use_lod;
                meshes_built++;
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
        TIMER(shadow_render);
        if (shadow_mapping) {
                // End the auto-started main render pass
                vkCmdEndRenderPass(cmdbuf);

                // Render shadow passes with per-cascade bias (using pre-built visible chunk list)
                // Near cascade: rendered every frame with PCF
                draw_shadow_pass(cmdbuf, shadow_framebuffer, shadow_pv_mtrx0, 1.5f, 1.5f, &shadow_polys_near, 1);

                // Mid cascade: rendered every frame, no PCF, no A/B blending
                draw_shadow_pass(cmdbuf, shadow2_framebuffer, shadow_pv_mtrx1, 1.0f, 1.0f, &shadow_polys_mid, 2);

                // Far cascade: render to A or B based on which needs updating
                if (shadow3_render_index >= 0) {
                        VkFramebuffer far_fb = (shadow3_render_index == 0) ? shadow3a_framebuffer : shadow3b_framebuffer;
                        float *far_pv = (shadow3_render_index == 0) ? shadow3a_matrix : shadow3b_matrix;
                        draw_shadow_pass(cmdbuf, far_fb, far_pv, 0.5f, 0.5f, &shadow_polys_far, 4);
                }

                // Extreme cascade: render to A or B based on which needs updating
                if (shadow4_render_index >= 0) {
                        VkFramebuffer extreme_fb = (shadow4_render_index == 0) ? shadow4a_framebuffer : shadow4b_framebuffer;
                        float *extreme_pv = (shadow4_render_index == 0) ? shadow4a_matrix : shadow4b_matrix;
                        draw_shadow_pass(cmdbuf, extreme_fb, extreme_pv, 0.5f, 0.5f, &shadow_polys_extreme, 8);
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

        // Render sky, sun, and terrain
        TIMER(draw_terrain);
        sky_draw(cmdbuf, proj_mtrx, view_mtrx);
        sun_draw(cmdbuf, proj_mtrx, view_mtrx, sun_pitch, sun_yaw, sun_roll);
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

        // Use pre-built visible chunk list
        for (int k = 0; k < visible_chunk_count; k++) {
                if (!visible_chunks[k].camera_visible) continue;
                int i = visible_chunks[k].x;
                int j = visible_chunks[k].z;

                push.chunk_x = i * BS * CHUNKW;
                push.chunk_y = 0;
                push.chunk_z = j * BS * CHUNKD;
                vkCmdPushConstants(cmdbuf, vk.pipelines[main_pipe].layout,
                        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof push, &push);
                vkCmdBindVertexBuffers(cmdbuf, 0, 1, &world_buf[i * VAOD + j], &voffset);
                vkCmdDraw(cmdbuf, VBOLEN_(i, j), 1, 0, 0);
                chunks_drawn++;
                total_verts += VBOLEN_(i, j);
                polys += VBOLEN_(i, j);
        }

        /*
        if (frame % 60 == 0) fprintf(stderr, "Drew %d/%d chunks, %d verts. Player: %d,%d scoot: %d,%d\n",
                chunks_drawn, chunks_with_data, total_verts,
                (int)(camplayer.pos.x / BS / CHUNKW), (int)(camplayer.pos.z / BS / CHUNKD),
                chunk_scootx, chunk_scootz);
        */

        if (mouselook) cursor(cmdbuf);

        debrief();

        TIMER(frame_submit);
        vulkan_submit();
        TIMER();
}

#endif // BLOCKO_DRAW_C_INCLUDED

#include "blocko.c"
#ifndef BLOCKO_DRAW_C_INCLUDED
#define BLOCKO_DRAW_C_INCLUDED

static int chunk_dist_compare(const struct visible_chunk *a, const struct visible_chunk *b)
{
        if (a->dist_sq < b->dist_sq) return -1;
        if (a->dist_sq > b->dist_sq) return 1;
        return 0;
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

//draw everything in the game on the screen
void draw_stuff()
{
        TIMER(gpu_sync);
        vulkan_acquire_next();

        TIMER(draw_start);
        // Read GPU timestamps from previous frame (non-blocking, from 2 frames ago)
        if (gpu_timestamp_pool && frame > 1) {
                VkResult result = vkGetQueryPoolResults(vk.device, gpu_timestamp_pool,
                        0, GPU_TS_COUNT, sizeof(gpu_timestamps), gpu_timestamps,
                        sizeof(uint64_t), VK_QUERY_RESULT_64_BIT);
                gpu_timestamps_valid = (result == VK_SUCCESS);
        }

        memset(&main_ubo, 0, sizeof main_ubo);

        float identity_mtrx[] = {
                1, 0, 0, 0,
                0, 1, 0, 0,
                0, 0, 1, 0,
                0, 0, 0, 1,
        };

        memcpy(main_ubo.model, identity_mtrx, sizeof identity_mtrx);

        // Calculate sun/moon positions (used for lighting and shadows)
        float chunk_size = BS * CHUNKW;
        shadow_target[0] = camplayer.pos.x;
        shadow_target[1] = camplayer.pos.y;
        shadow_target[2] = camplayer.pos.z;

        // Original tilted sun path (non-equatorial for interesting lighting)
        sun_pos.x = shadow_target[0] + dist2sun * (cosf(sun_pitch) * cosf(sun_yaw));
        sun_pos.y = shadow_target[1] + dist2sun * (cosf(sun_pitch) * sinf(sun_yaw) * cosf(sun_roll) + sinf(sun_pitch) * sinf(sun_roll));
        sun_pos.z = shadow_target[2] + dist2sun * (cosf(sun_pitch) * sinf(sun_yaw) * sinf(sun_roll) - sinf(sun_pitch) * cosf(sun_roll));

        moon_pitch = sun_pitch + PI;
        if (moon_pitch >= TAU) moon_pitch -= TAU;
        moon_pos.x = shadow_target[0] + dist2sun * (cosf(moon_pitch) * cosf(sun_yaw));
        moon_pos.y = shadow_target[1] + dist2sun * (cosf(moon_pitch) * sinf(sun_yaw) * cosf(sun_roll) + sinf(moon_pitch) * sinf(sun_roll));
        moon_pos.z = shadow_target[2] + dist2sun * (cosf(moon_pitch) * sinf(sun_yaw) * sinf(sun_roll) - sinf(moon_pitch) * cosf(sun_roll));

        TIMER(shadow_calc);
        do_shadows();

        TIMER(frame_setup);
        do_atmos_colors();

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
        peye0 = lerped_pos.x + PLYR_W / 2;
        peye1 = lerped_pos.y + EYEDOWN * (camplayer.sneaking ? 2 : 1);
        peye2 = lerped_pos.z + PLYR_W / 2;
        float f[3];
        float view_mtrx[16];
        lookit(view_mtrx, f, peye0, peye1, peye2, camplayer.pitch, camplayer.yaw);

        // find where we are pointing at
        rayshot(peye0, peye1, peye2, f[0], f[1], f[2]);

        // translate by hand
        float translated_view_mtrx[16];
        memcpy(translated_view_mtrx, view_mtrx, sizeof view_mtrx);
        translate(translated_view_mtrx, -peye0, -peye1, -peye2);

        mat4_multiply(proj_view_mtrx, proj_mtrx, translated_view_mtrx);

        // Build visible chunk list (single pass: check all frustums)
        visible_chunk_count = 0;
        int far_idx = (shadow_far_render_ab == 0) ? SHADOW_FAR_A : SHADOW_FAR_B;
        int ext_idx = (shadow_ext_render_ab == 0) ? SHADOW_EXT_A : SHADOW_EXT_B;
        for (int i = 0; i < VAOW; i++) {
                for (int j = 0; j < VAOD; j++) {
                        if (!VBOLEN_(i, j)) continue;

                        // Check camera visibility
                        int camera_visible = chunk_in_frustum(proj_view_mtrx, i, j) && chunk_in_range(i, j);

                        // Check shadow frustums
                        unsigned char shadow_mask = 0;
                        if (shadow_mapping) {
                                if (chunk_in_frustum(shadow[SHADOW_NEAR].matrix, i, j)) shadow_mask |= 1;  // Near
                                if (chunk_in_frustum(shadow[SHADOW_MID].matrix, i, j)) shadow_mask |= 2;  // Mid
                                if (shadow_far_render_ab >= 0 && chunk_in_frustum(shadow[far_idx].matrix, i, j)) shadow_mask |= 4;  // Far
                                // Extreme cascade: only include if camera-visible (too far to cast visible shadows from behind)
                                if (camera_visible && shadow_ext_render_ab >= 0 && chunk_in_frustum(shadow[ext_idx].matrix, i, j)) shadow_mask |= 8;
                        }

                        // Include if visible to camera OR any shadow frustum
                        if (!camera_visible && !shadow_mask) continue;

                        // Calculate squared distance to camera for sorting
                        float chunk_cx = (i + 0.5f) * CHUNKW * BS;
                        float chunk_cz = (j + 0.5f) * CHUNKD * BS;
                        float dx = peye0 - chunk_cx;
                        float dz = peye2 - chunk_cz;

                        visible_chunks[visible_chunk_count].x = i;
                        visible_chunks[visible_chunk_count].z = j;
                        visible_chunks[visible_chunk_count].shadow_mask = shadow_mask;
                        visible_chunks[visible_chunk_count].camera_visible = camera_visible;
                        visible_chunks[visible_chunk_count].dist_sq = dx*dx + dz*dz;
                        visible_chunk_count++;
                }
        }

        // Sort visible chunks front-to-back for early-Z optimization
        qsort(visible_chunks, visible_chunk_count, sizeof(struct visible_chunk),
              (int (*)(const void *, const void *))chunk_dist_compare);

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

        main_ubo.view_pos[0] = peye0;
        main_ubo.view_pos[1] = peye1;
        main_ubo.view_pos[2] = peye2;

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
                main_ubo.fog_lo = BS * 50.0f;
                main_ubo.fog_hi = draw_dist * BS * 0.9f;

                // Sun strength and warmth for day/night cycle
                // Full brightness between PI/16 and PI-PI/16, dimming/warming only near horizons
                // During night: moonlight at constant strength, no warmth
                if (sun_pitch < PI) {
                        // Daytime
                        float transition = PI / 16.0f;  // ~11 degrees transition zone
                        float sunrise_end = transition;
                        float sunset_start = PI - transition;

                        if (sun_pitch < sunrise_end) {
                                // Sunrise transition: dim/warm → bright/white
                                float t = sun_pitch / sunrise_end;
                                main_ubo.sun_strength = t * t;
                                main_ubo.sun_warmth = 1.0f - t;
                        } else if (sun_pitch > sunset_start) {
                                // Sunset transition: bright/white → dim/warm
                                float t = (sun_pitch - sunset_start) / transition;
                                main_ubo.sun_strength = 1.0f - t * t;
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
                                main_ubo.sun_strength = t * t;
                                main_ubo.sun_warmth = 1.0f - t;  // Fade out warmth from sunset
                        } else if (sun_pitch > moonset_start) {
                                // Moonset transition: bright → dim, cool → warm
                                float t = (sun_pitch - moonset_start) / transition;
                                main_ubo.sun_strength = 1.0f - t * t;
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
        TIMER(sync_w_terrain_gen)
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

        TIMER(build_meshes);
        build_meshes();

        TIMER(upload_ubo);
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

        // End the auto-started render pass so we can reset query pool and do shadow passes
        vkCmdEndRenderPass(cmdbuf);

        // Reset and start GPU timestamp queries (must be outside render pass)
        if (gpu_timestamp_pool) {
                vkCmdResetQueryPool(cmdbuf, gpu_timestamp_pool, 0, GPU_TS_COUNT);
                vkCmdWriteTimestamp(cmdbuf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, gpu_timestamp_pool, GPU_TS_FRAME_START);
        }

        // Render shadow maps (before main render pass)
        TIMER(shadow_render);
        shadow_render(cmdbuf);

        // Start main render pass
        TIMER(draw_terrain);
        VkClearValue clearValues[2] = {
                {},
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

        // Render terrain first (front-to-back for early-Z optimization)
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

        // Render sky/sun last - only fills pixels not covered by terrain (early-Z optimization)
        sky_draw(cmdbuf, proj_mtrx, view_mtrx);
        sun_draw(cmdbuf, proj_mtrx, view_mtrx, sun_pitch, sun_yaw, sun_roll);

        if (mouselook) cursor(cmdbuf);

        // GPU timestamp after terrain
        if (gpu_timestamp_pool) vkCmdWriteTimestamp(cmdbuf, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, gpu_timestamp_pool, GPU_TS_TERRAIN_END);

        debrief();

        // GPU timestamp at frame end
        if (gpu_timestamp_pool) vkCmdWriteTimestamp(cmdbuf, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, gpu_timestamp_pool, GPU_TS_FRAME_END);

        TIMER(frame_submit);
        vulkan_submit();
        TIMER();
}

#endif // BLOCKO_DRAW_C_INCLUDED

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

// measure to the nearest point of the chunk's footprint, not its center -
// culling by center dropped corners up to CHUNKW*sqrt(2)/2 blocks inside
// draw_dist, leaving visible holes before the fog line (fog_hi = 0.9*draw_dist)
int chunk_in_range(int chunk_x, int chunk_z)
{
        float draw_dist_sq = draw_dist * BS * draw_dist * BS;
        float lo_x = chunk_x * BS * CHUNKW, hi_x = lo_x + BS * CHUNKW;
        float lo_z = chunk_z * BS * CHUNKD, hi_z = lo_z + BS * CHUNKD;
        float delta_x = cull_x - CLAMP(cull_x, lo_x, hi_x);
        float delta_z = cull_z - CLAMP(cull_z, lo_z, hi_z);
        float dist_sq = delta_x * delta_x + delta_z * delta_z;
        return dist_sq < draw_dist_sq;
}

// Adopt chunks the terrain workers finished since last frame: mark them dirty,
// hide the previous occupant's mesh, and replay recorded edits before their
// meshes build. Called from draw_stuff normally; the headless loop calls it
// directly since nothing else needs to happen for the world to be right.
void sync_fresh_chunks()
{
        int fresh[VAOW*VAOD][2], nr_fresh = 0;
        #pragma omp critical
        {
                for (size_t i = 0; i < just_gen_len; i++) {
                        int cx = just_generated[i].x + chunk_scootx; // absolute -> window
                        int cz = just_generated[i].z + chunk_scootz;
                        DIRTY_(cx, cz) = 1;
                        if (!MESHGEN_(cx, cz))
                                VBOLEN_(cx, cz) = 0; // another chunk's mesh: hide it
                        // same chunk regenerated in place: keep the old mesh
                        // up until the rebuild lands, so regens don't blink
                        fresh[nr_fresh][0] = just_generated[i].x;
                        fresh[nr_fresh][1] = just_generated[i].z;
                        nr_fresh++;
                }
                just_gen_len = 0;
        }

        // (outside the critical - replaying floods light)
        for (int i = 0; i < nr_fresh; i++)
                edit_apply_chunk(fresh[i][0], fresh[i][1]);
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
                if (gpu_timestamps_valid && gpu_timestamp_period > 0) {
                        double k = gpu_timestamp_period / 1e6;
                        for (int i = 1; i < GPU_TS_COUNT; i++)
                                gpu_ms_accum[i] += (gpu_timestamps[i] - gpu_timestamps[i-1]) * k;
                        gpu_ms_frames++;
                }
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

        // While the light source sits within a couple degrees of the horizon its
        // directional light has faded to nothing (same ramp as sun_strength
        // below), but the low-angle shadow frustums sweep more terrain than at
        // any other time of day - the most expensive shadows are the invisible
        // ones. Idle the whole shadow pipeline until the light climbs back up.
        {
                float transition = PI / 16.0f;
                float p = sun_pitch < PI ? sun_pitch : sun_pitch - PI; // moon mirrors the sun
                float strength = 1.0f;
                if (p < transition) {
                        float t = p / transition;
                        strength = t * t;
                } else if (p > PI - transition) {
                        float t = (p - (PI - transition)) / transition;
                        strength = 1.0f - t * t;
                }
                shadow_idle = strength < 0.05f;

                // Ease shadow contrast to zero over the bottom of the strength
                // ramp (5%..50%, i.e. the last ~5.5 degrees of elevation) so the
                // idle flip above has nothing visible left to cut off.
                float f = (strength - 0.05f) / 0.45f;
                f = f < 0.f ? 0.f : f > 1.f ? 1.f : f;
                main_ubo.shadow_fade = f * f * (3.f - 2.f * f);
        }

        // do_shadows() runs every frame even when frozen: the UBO is memset to 0
        // above, so it must refill shadow_space[] or the sampling coords collapse to
        // the origin and shadows vanish. When frozen it rebuilds those from the
        // stored shadow[].matrix (which apply_scoot keeps world-aligned) instead of
        // recentering on the player, and the shadow-map re-render is skipped.
        TIMER(shadow_calc);
        do_shadows();

        TIMER(frame_setup);
        do_atmos_colors();

        // eye position + underwater check (needed before proj for FOV)
        peye0 = lerped_pos.x + PLYR_W / 2;
        peye1 = lerped_pos.y + EYEDOWN * (camplayer.sneaking ? 2 : 1);
        peye2 = lerped_pos.z + PLYR_W / 2;
        int eyex = peye0 / BS;
        int eyey = peye1 / BS;
        int eyez = peye2 / BS;
        main_ubo.underwater =
                (legit_tile(eyex, eyey, eyez) && T_(eyex, eyey, eyez) == WATR) ? 1.f : 0.f;

        // narrow the FOV underwater - refraction magnifies everything
        static float underwater_zoom = 1.f;
        float zoom_target = main_ubo.underwater ? 0.875f : 1.f;
        underwater_zoom += (zoom_target - underwater_zoom) * 0.15f;

        // compute proj matrix
        float near = 100.f;
        float far = 1000 * BS;  // 1000 blocks with BS=1000
        float fov_degrees = 90.f;  // horizontal FOV in degrees
        float frustw = near * tanf(fov_degrees * PI / 360.f) * zoom_amt * underwater_zoom;
        float frusth = frustw * screenh / screenw;
        float proj_mtrx[] = {
                near/frustw,            0,                                0,  0,
                          0, -near/frusth,                                0,  0,
                          0,            0,          -far / (far - near), -1,
                          0,            0, -(far * near) / (far - near),  0
        };

        // compute view matrix
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

        if (!lock_culling)
        {
                memcpy(cull_mtrx, proj_view_mtrx, sizeof cull_mtrx);
                cull_x = camplayer.pos.x;
                cull_z = camplayer.pos.z;
        }

        // Adopt newly generated chunks - must happen before the visible list
        // below or the stale mesh gets drawn for a frame
        TIMER(sync_w_terrain_gen)
        sync_fresh_chunks();

        // Build visible chunk list (single pass: check all frustums)
        visible_chunk_count = 0;
        int far_idx = (shadow_far_render_ab == 0) ? SHADOW_FAR_A : SHADOW_FAR_B;
        int ext_idx = (shadow_ext_render_ab == 0) ? SHADOW_EXT_A : SHADOW_EXT_B;
        for (int i = 0; i < VAOW; i++) {
                for (int j = 0; j < VAOD; j++) {
                        if (!VBOLEN_(i, j)) continue;
                        if (!MESHGEN_(i, j)) continue; // slot holds another chunk's mesh
                        // (a stale mesh of THIS chunk still draws: during a
                        // regen the old terrain stays up until replaced)

                        // Check camera visibility
                        int camera_visible = chunk_in_frustum(cull_mtrx, i, j) && chunk_in_range(i, j);

                        // Check shadow frustums
                        unsigned char shadow_mask = 0;
                        if (shadow_mapping && !shadow_idle) {
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

        // When idle, tell the shader too: unshadow pins to 1.0, which is invisible
        // under a <5%-strength directional light, and the PCF sampling is skipped
        main_ubo.shadow_mapping = shadow_mapping && !shadow_idle;

        memcpy(main_ubo.proj, proj_mtrx, sizeof proj_mtrx);
        memcpy(main_ubo.view, translated_view_mtrx, sizeof translated_view_mtrx);

        main_ubo.bs = BS;

        // window->world block offset, so shaders can recover absolute world
        // coords from the window-relative mesh (tall grass hashes its per-cell
        // rotation/jitter on the absolute cell, else it shifts on scoot)
        main_ubo.scootx = scootx;
        main_ubo.scootz = scootz;

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
                main_ubo.fog_lo = BS * 50.0f;
                main_ubo.fog_hi = draw_dist * BS * 0.9f;

                // sun direction + night_amt drive the per-pixel sky-colored
                // fog (sky_color.glsl) — same formula as sky_draw's push
                main_ubo.sun_dir[0] = cosf(sun_pitch) * cosf(sun_yaw);
                main_ubo.sun_dir[1] = sinf(sun_pitch);
                main_ubo.sun_dir[2] = -cosf(sun_pitch) * sinf(sun_yaw);
                main_ubo.night_amt = night_amt;

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
                        main_ubo.outside_cascade_lit = main_ubo.sun_strength;
                }
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

        // refresh (or retire) the reject+patch for any pending block edit; must
        // run after build_meshes so it sees which chunks just rebuilt
        patch_update();

        main_ubo.water_frame = pframe;

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

        // Build mob + mining geometry once so both the shadow pass (near
        // cascade) and the main pass can draw the same vertex buffers
        mob_build();
        pmodel_build();
        mine_overlay_build();
        item_build();
        hand_build();

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

        // Render opaque terrain (front-to-back for early-Z optimization)
        vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipelines[main_pipe].pipeline);

        VkViewport viewport = { 0, 0, vk.bestSwapchainExtent.width, vk.bestSwapchainExtent.height, 0, 1 };
        VkRect2D scissor = { {0, 0}, {vk.bestSwapchainExtent.width, vk.bestSwapchainExtent.height} };
        vkCmdSetViewport(cmdbuf, 0, 1, &viewport);
        vkCmdSetScissor(cmdbuf, 0, 1, &scissor);

        vkCmdBindDescriptorSets(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS,
                vk.pipelines[main_pipe].layout, 0, 1, &main_descriptor_set[vk.currentFrame], 0, NULL);

        struct { float pv[16]; float chunk_x, chunk_y, chunk_z, bs;
                 float reject_lo[4], reject_hi[4]; } push;
        memcpy(push.pv, proj_view_mtrx, sizeof push.pv);
        push.bs = BS;

        // opaque terrain rejects the faces of any cell in the pending edit box
        // (window tile coords this frame); patch_render redraws them. Empty box
        // (lo > hi) when no edit is pending, so nothing is rejected.
        patch_reject_box(push.reject_lo, push.reject_hi);

        VkDeviceSize voffset = 0;
        int chunks_drawn = 0;
        int total_verts = 0;

        // Pass 1: opaque terrain (front-to-back)
        for (int k = 0; k < visible_chunk_count; k++) {
                if (!visible_chunks[k].camera_visible) continue;
                int i = visible_chunks[k].x;
                int j = visible_chunks[k].z;
                size_t terrain_verts = WBOSTART_(i, j);
                if (!terrain_verts) continue;

                push.chunk_x = i * BS * CHUNKW;
                push.chunk_y = 0;
                push.chunk_z = j * BS * CHUNKD;
                vkCmdPushConstants(cmdbuf, vk.pipelines[main_pipe].layout,
                        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof push, &push);
                vkCmdBindVertexBuffers(cmdbuf, 0, 1, &WBUF_(i, j), &voffset);
                vkCmdDraw(cmdbuf, 4, terrain_verts, 0, 0);
                chunks_drawn++;
                total_verts += terrain_verts;
                polys += terrain_verts;
        }

        // Mobs draw on their own pipeline (mob.vert); rebind the opaque terrain
        // pipeline afterward for the block-breaking overlay and the edit patch.
        mob_render(cmdbuf, mob_pipe, proj_view_mtrx);
        item_render(cmdbuf, mob_pipe, proj_view_mtrx); // spins on the mob pipeline
        pmodel_render(cmdbuf, pmodel_pipe, proj_view_mtrx);
        vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipelines[main_pipe].pipeline);
        mine_overlay_render(cmdbuf, main_pipe, proj_view_mtrx);

        // Draw the patch: the corrected mesh of the pending edit box, filling in
        // the faces the reject test just culled from the big chunk buffers
        patch_render(cmdbuf, main_pipe, proj_view_mtrx);

        // the held block, floating at the lower right; drawn on top of the world
        // via a squashed depth range so it never clips into nearby terrain
        hand_render(cmdbuf, main_pipe, proj_view_mtrx);

        // Render sky/sun between opaque terrain and transparent water
        sky_draw(cmdbuf, proj_mtrx, view_mtrx);
        if (!main_ubo.underwater) // too murky to see the sun/moon
                sun_draw(cmdbuf, proj_mtrx, view_mtrx, sun_pitch, sun_yaw, sun_roll);

        // Pass 2: water, split by distance. Far chunks are fully past the
        // smoothstep(40,100) alpha ramp in main.frag, so their fragments are
        // already opaque - out there we render on the solid pipeline (no blend,
        // back-face culled, front-to-back for early-Z) and only near chunks pay
        // for real transparency. The threshold is the chunk-CENTER distance at
        // which the chunk's NEAREST point clears 100 blocks, so the two paths
        // meet with identical color and the seam is invisible.
        // (Keep 100.0 in sync with the ramp's upper bound in main.frag.)
        float solid_near = 100.f * BS;
        float halfdiag = 0.5f * sqrtf((float)(CHUNKW*BS)*(float)(CHUNKW*BS)
                                    + (float)(CHUNKD*BS)*(float)(CHUNKD*BS));
        float solid_thresh_sq = (solid_near + halfdiag) * (solid_near + halfdiag);

        // reject stale water faces inside the pending edit box; patch_render_water
        // redraws them (Phase 2). Empty box when no edit is pending. Shared by both
        // water passes (far chunks never overlap the edit box, so it's a no-op there).
        patch_reject_box(push.reject_lo, push.reject_hi);

        // Pass 2a: far water, solid pipeline, front-to-back (early-Z)
        vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipelines[water_solid_pipe].pipeline);
        vkCmdSetViewport(cmdbuf, 0, 1, &viewport);
        vkCmdSetScissor(cmdbuf, 0, 1, &scissor);
        vkCmdBindDescriptorSets(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS,
                vk.pipelines[water_solid_pipe].layout, 0, 1, &main_descriptor_set[vk.currentFrame], 0, NULL);

        for (int k = 0; k < visible_chunk_count; k++) {
                if (!visible_chunks[k].camera_visible) continue;
                if (visible_chunks[k].dist_sq < solid_thresh_sq) continue; // near -> Pass 2b
                int i = visible_chunks[k].x;
                int j = visible_chunks[k].z;
                size_t water_start = WBOSTART_(i, j);
                size_t water_verts = VBOLEN_(i, j) - water_start;
                if (!water_verts) continue;

                push.chunk_x = i * BS * CHUNKW;
                push.chunk_y = 0;
                push.chunk_z = j * BS * CHUNKD;
                VkDeviceSize water_offset = water_start * sizeof(struct vbufv);
                vkCmdPushConstants(cmdbuf, vk.pipelines[water_solid_pipe].layout,
                        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof push, &push);
                vkCmdBindVertexBuffers(cmdbuf, 0, 1, &WBUF_(i, j), &water_offset);
                vkCmdDraw(cmdbuf, 4, water_verts, 0, 0);
                total_verts += water_verts;
                polys += water_verts;
        }

        // Pass 2b: near water, transparent pipeline, back-to-front
        vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipelines[water_pipe].pipeline);
        vkCmdSetViewport(cmdbuf, 0, 1, &viewport);
        vkCmdSetScissor(cmdbuf, 0, 1, &scissor);
        vkCmdBindDescriptorSets(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS,
                vk.pipelines[water_pipe].layout, 0, 1, &main_descriptor_set[vk.currentFrame], 0, NULL);

        for (int k = visible_chunk_count - 1; k >= 0; k--) {
                if (!visible_chunks[k].camera_visible) continue;
                if (visible_chunks[k].dist_sq >= solid_thresh_sq) continue; // far -> Pass 2a
                int i = visible_chunks[k].x;
                int j = visible_chunks[k].z;
                size_t water_start = WBOSTART_(i, j);
                size_t water_verts = VBOLEN_(i, j) - water_start;
                if (!water_verts) continue;

                push.chunk_x = i * BS * CHUNKW;
                push.chunk_y = 0;
                push.chunk_z = j * BS * CHUNKD;
                VkDeviceSize water_offset = water_start * sizeof(struct vbufv);
                vkCmdPushConstants(cmdbuf, vk.pipelines[water_pipe].layout,
                        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof push, &push);
                vkCmdBindVertexBuffers(cmdbuf, 0, 1, &WBUF_(i, j), &water_offset);
                vkCmdDraw(cmdbuf, 4, water_verts, 0, 0);
                total_verts += water_verts;
                polys += water_verts;
        }

        // patch the edit box's water/glow faces the reject test just culled
        patch_render_water(cmdbuf, water_pipe, proj_view_mtrx);

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

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
        float delta_x = cull_x - (chunk_x + .5f) * BS * CHUNKW;
        float delta_z = cull_z - (chunk_z + .5f) * BS * CHUNKD;
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
        vulkan_acquire_next();

        memset(&main_ubo, 0, sizeof main_ubo);

        float identity_mtrx[] = {
                1, 0, 0, 0,
                0, 1, 0, 0,
                0, 0, 1, 0,
                0, 0, 0, 1,
        };

        memcpy(main_ubo.model, identity_mtrx, sizeof identity_mtrx);

        // The sun sits fixed in the sky; its position only follows the player
        shadow_target[0] = camplayer.pos.x;
        shadow_target[1] = camplayer.pos.y;
        shadow_target[2] = camplayer.pos.z;
        sun_pos.x = shadow_target[0] + dist2sun * sun_dir[0];
        sun_pos.y = shadow_target[1] + dist2sun * sun_dir[1];
        sun_pos.z = shadow_target[2] + dist2sun * sun_dir[2];

        do_shadows();

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

        memcpy(cull_mtrx, proj_view_mtrx, sizeof cull_mtrx);
        cull_x = camplayer.pos.x;
        cull_z = camplayer.pos.z;

        // Adopt newly generated chunks - must happen before the visible list
        // below or the stale mesh gets drawn for a frame
        sync_fresh_chunks();

        // Build visible chunk list (single pass: check both frustums)
        visible_chunk_count = 0;
        for (int i = 0; i < VAOW; i++) {
                for (int j = 0; j < VAOD; j++) {
                        if (!VBOLEN_(i, j)) continue;
                        if (!MESHGEN_(i, j)) continue; // slot holds another chunk's mesh
                        // (a stale mesh of THIS chunk still draws: during a
                        // regen the old terrain stays up until replaced)

                        // Check camera visibility
                        int camera_visible = chunk_in_frustum(cull_mtrx, i, j) && chunk_in_range(i, j);

                        // Check the shadow frustum
                        unsigned char shadow_mask = 0;
                        if (shadow_mapping && chunk_in_frustum(shadow[SHADOW_NEAR].matrix, i, j))
                                shadow_mask = 1;

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

        // window->world block offset, so shaders can recover absolute world
        // coords from the window-relative mesh (tall grass hashes its per-cell
        // rotation/jitter on the absolute cell, else it shifts on scoot)
        main_ubo.scootx = scootx;
        main_ubo.scootz = scootz;

        main_ubo.light_pos[0] = sun_pos.x;
        main_ubo.light_pos[1] = sun_pos.y;
        main_ubo.light_pos[2] = sun_pos.z;

        main_ubo.view_pos[0] = peye0;
        main_ubo.view_pos[1] = peye1;
        main_ubo.view_pos[2] = peye2;

        main_ubo.glo_color[0] = 0.92f;
        main_ubo.glo_color[1] = 0.83f;
        main_ubo.glo_color[2] = 0.69f;
        main_ubo.fog_lo = BS * 50.0f;
        main_ubo.fog_hi = draw_dist * BS * 0.9f;

        // Check LOD chunks for face visibility changes
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

        build_meshes();

        // refresh (or retire) the reject+patch for any pending block edit; must
        // run after build_meshes so it sees which chunks just rebuilt
        patch_update();

        main_ubo.water_frame = pframe;

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

        // End the auto-started render pass so we can do shadow passes
        vkCmdEndRenderPass(cmdbuf);

        // Build mob + mining geometry once so both the shadow pass (near
        // cascade) and the main pass can draw the same vertex buffers
        mob_build();
        mine_overlay_build();
        item_build();
        hand_build();

        // Render shadow maps (before main render pass)
        shadow_render(cmdbuf);

        // Start main render pass
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
        }

        // Mobs draw on their own pipeline (mob.vert); rebind the opaque terrain
        // pipeline afterward for the block-breaking overlay and the edit patch.
        mob_render(cmdbuf, mob_pipe, proj_view_mtrx);
        item_render(cmdbuf, mob_pipe, proj_view_mtrx); // spins on the mob pipeline
        vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipelines[main_pipe].pipeline);
        mine_overlay_render(cmdbuf, main_pipe, proj_view_mtrx);

        // Draw the patch: the corrected mesh of the pending edit box, filling in
        // the faces the reject test just culled from the big chunk buffers
        patch_render(cmdbuf, main_pipe, proj_view_mtrx);

        // the held block, floating at the lower right; drawn on top of the world
        // via a squashed depth range so it never clips into nearby terrain
        hand_render(cmdbuf, main_pipe, proj_view_mtrx);

        // Render the sun between opaque terrain and transparent water
        if (!main_ubo.underwater) // too murky to see the sun
                sun_draw(cmdbuf, proj_mtrx, view_mtrx);

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
        }

        // patch the edit box's water/glow faces the reject test just culled
        patch_render_water(cmdbuf, water_pipe, proj_view_mtrx);

        if (mouselook) cursor(cmdbuf);

        debrief();

        vulkan_submit();
}

#endif // BLOCKO_DRAW_C_INCLUDED

#include "blocko.c"
#ifndef BLOCKO_DRAW_C_INCLUDED
#define BLOCKO_DRAW_C_INCLUDED

int sorter(const void * _a, const void * _b)
{
        const struct qitem *a = _a;
        const struct qitem *b = _b;
        return (a->y == b->y) ?  0 :
               (a->y <  b->y) ?  1 : -1;
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

// prevent shaking shadows by quantizing sun or moon pitch
float quantize(float p)
{
        float quantizer;
        float qbracket = sinf(p);

        if      (qbracket > 0.8f) quantizer = 0.001f;
        else if (qbracket > 0.6f) quantizer = 0.0005f;
        else if (qbracket > 0.4f) quantizer = 0.00025f;
        else if (qbracket > 0.2f) quantizer = 0.000125f;
        else                      quantizer = 0.0000625f;

        return roundf(p / quantizer) * quantizer;
}

//draw everything in the game on the screen
void draw_stuff()
{
        vulkan_acquire_next();
        struct main_ubo main_ubo = {0};

        float identity_mtrx[] = {
                1, 0, 0, 0,
                0, 1, 0, 0,
                0, 0, 1, 0,
                0, 0, 0, 1,
        };

        //glDisable(GL_MULTISAMPLE);

        memcpy(main_ubo.model, identity_mtrx, sizeof identity_mtrx);

        // make shadow map
        if (shadow_mapping) for(int s = 0; s < 2; s++)
        {
                //glBindFramebuffer(GL_FRAMEBUFFER, s == 0 ? shadow_fbo : shadow2_fbo);
                //if (is_framebuffer_incomplete()) goto fb_is_bad;

                //glViewport(0, 0, SHADOW_SZ, SHADOW_SZ);
                //glClear(GL_DEPTH_BUFFER_BIT);

                //glEnable(GL_BLEND);
                //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                //glEnable(GL_DEPTH_TEST);
                //glDepthFunc(GL_LEQUAL);
                //glDepthMask(GL_TRUE);
                //glEnable(GL_CULL_FACE);
                //glCullFace(GL_FRONT);
                //glEnable(GL_POLYGON_OFFSET_FILL);
                //glPolygonOffset(4.f, 4.f);

                ////render shadows here
                //glUseProgram(shadow_prog_id);
                // view matrix
                float view_mtrx[16];

                float moon_pitch = sun_pitch + PI;
                if (moon_pitch >= TAU) moon_pitch -= TAU;

                float dist2sun = TILESW * BS;

                float f[3] = {
                        camplayer.pos.x,
                        100 * BS,
                        camplayer.pos.z,
                };

                sun_pos.x = f[0] + dist2sun * (cosf(sun_pitch) * cosf(sun_yaw));
                sun_pos.y = f[1] + dist2sun * (cosf(sun_pitch) * sinf(sun_yaw) * cosf(sun_roll) + sinf(sun_pitch) * sinf(sun_roll));
                sun_pos.z = f[2] + dist2sun * (cosf(sun_pitch) * sinf(sun_yaw) * sinf(sun_roll) - sinf(sun_pitch) * cosf(sun_roll));

                moon_pos.x = f[0] + dist2sun * (cosf(moon_pitch) * cosf(sun_yaw));
                moon_pos.y = f[1] + dist2sun * (cosf(moon_pitch) * sinf(sun_yaw) * cosf(sun_roll) + sinf(moon_pitch) * sinf(sun_roll));
                moon_pos.z = f[2] + dist2sun * (cosf(moon_pitch) * sinf(sun_yaw) * sinf(sun_roll) - sinf(moon_pitch) * cosf(sun_roll));

                /*
                T_((int)(sun_pos.x / BS), (int)(sun_pos.y / BS), (int)(sun_pos.z / BS)) = GRAS;
                T_((int)(moon_pos.x / BS), (int)(moon_pos.y / BS), (int)(moon_pos.z / BS)) = GRAN;
                */

                if (sun_pitch < PI)
                {
                        lookit(view_mtrx, f, sun_pos.x, sun_pos.y, sun_pos.z, NO_PITCH, 0.f);
                        translate(view_mtrx, -sun_pos.x, -sun_pos.y, -sun_pos.z);
                }
                else
                {
                        lookit(view_mtrx, f, moon_pos.x, moon_pos.y, moon_pos.z, NO_PITCH, 0.f);
                        translate(view_mtrx, -moon_pos.x, -moon_pos.y, -moon_pos.z);
                }

                // proj matrix
                float snear = (s == 0 ? 10.f : 80.f);
                float sfar = dist2sun + (s == 0 ? 9000.f : 72000.f);
                float mag = (s == 0 ? 3000.f : 24000.f);
                float x = 1.f / mag;
                float y = -1.f / mag;
                float z = -1.f / ((sfar - snear) / 2.f);
                float tz = -(sfar + snear) / (sfar - snear);
                float ortho_mtrx[] = {
                        x, 0, 0,  0,
                        0, y, 0,  0,
                        0, 0, z,  0,
                        0, 0, tz, 1,
                };

                float shadow_pv_mtrx[16];
                if (!lock_culling)
                        mat4_multiply(shadow_pv_mtrx, ortho_mtrx, view_mtrx);

                memcpy(main_ubo.proj, ortho_mtrx, sizeof(ortho_mtrx));
                memcpy(main_ubo.view, view_mtrx, sizeof(view_mtrx));
                //glUniform1i(glGetUniformLocation(shadow_prog_id, "tarray"), 0);
                main_ubo.bs = BS;

                float bias_mtrx[] = {
                        .5f,   0,   0, 1.f,
                          0, .5f,   0, 1.f,
                          0,   0, .5f, 1.f,
                        .5f, .5f, .5f, 1.f,
                };
                float tmp_mtrx[16];
                mat4_multiply(tmp_mtrx, ortho_mtrx, view_mtrx);

                if (s == 0)
                        mat4_multiply(main_ubo.shadow_space, bias_mtrx, tmp_mtrx);
                else
                        mat4_multiply(main_ubo.shadow2_space, bias_mtrx, tmp_mtrx);

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

        //glViewport(0, 0, screenw, screenh);
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

        sun_draw(proj_mtrx, view_mtrx, sun_pitch, sun_yaw, sun_roll, shadow_tex_id);

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
                main_ubo.glo_color[0] = 0.92f;
                main_ubo.glo_color[1] = 0.83f;
                main_ubo.glo_color[2] = 0.69f;
                main_ubo.fog_color[0] = fog_r;
                main_ubo.fog_color[1] = fog_g;
                main_ubo.fog_color[2] = fog_b;
                main_ubo.fog_lo = draw_dist * BS * 0.667f;
                main_ubo.fog_hi = draw_dist * BS * 1.000f;
        }

        // determine which chunks to send to gl
        TIMER(rings)
        int x0 = (eye0 - BS * CHUNKW2) / (BS * CHUNKW);
        int z0 = (eye2 - BS * CHUNKW2) / (BS * CHUNKD);
        CLAMP(x0, 0, VAOW - 2);
        CLAMP(z0, 0, VAOD - 2);
        int x1 = x0 + 1;
        int z1 = z0 + 1;

        int x0d = ((x0 * BS * CHUNKW + BS * CHUNKW2) - eye0);
        int x1d = ((x1 * BS * CHUNKW + BS * CHUNKW2) - eye0);
        int z0d = ((z0 * BS * CHUNKD + BS * CHUNKD2) - eye2);
        int z1d = ((z1 * BS * CHUNKD + BS * CHUNKD2) - eye2);

        // initialize with ring0 chunks
        struct qitem fresh[VAOW*VAOD] = { // chunkx, distance sq, chunkz
                {x0, (x0d * x0d + z0d * z0d), z0},
                {x0, (x0d * x0d + z1d * z1d), z1},
                {x1, (x1d * x1d + z0d * z0d), z0},
                {x1, (x1d * x1d + z1d * z1d), z1}
        };
        size_t fresh_len = 4;

        qsort(fresh, fresh_len, sizeof(struct qitem), sorter);

        #pragma omp critical
        {
                memcpy(fresh + fresh_len,
                                (struct qitem *)just_generated,
                                just_gen_len * sizeof *just_generated);
                fresh_len += just_gen_len;
                just_gen_len = 0;
        }

        // position within each ring that we're at this frame
	static struct qitem ringpos[VAOW + VAOD] = {0};
        for (int r = 1; r < VAOW + VAOD; r++)
        {
		// expand ring in all directions
		x0--; x1++; z0--; z1++;

                // freshen farther rings less and less often
                if (r >= 3 && r <= 6 && frame % 2 != r % 2)   continue;
                if (r >= 7 && r <= 14 && frame % 4 != r % 4)  continue;
                if (r >= 15 && r <= 30 && frame % 8 != r % 8) continue;
                if (r >= 31 && frame % 16 != r % 16)          continue;

                int *x = &ringpos[r].x;
                int *z = &ringpos[r].z;

                // move to next chunk, maybe on ring
                --(*x);

                // wrap around the ring
		int x_too_low = (*x < x0);
                if (x_too_low) { *x = x1; --(*z); }

                // reset if out of the ring
		int z_too_low = (*z < z0);
                if (z_too_low) { *x = x1; *z = z1; }

                // get out of the middle
		int is_on_ring = (*z == z0 || *z == z1 || *x == x1);
                if (!is_on_ring) { *x = x0; }

                // render if in bounds
                if (*x >= 0 && *x < VAOW && *z >= 0 && *z < VAOD)
                {
                        fresh[fresh_len].x = *x;
                        fresh[fresh_len].z = *z;
                        fresh_len++;
                }
        }

        // render non-fresh chunks
        TIMER(drawstale)
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
                int xd = ((i * BS * CHUNKW + BS * CHUNKW2) - eye0);
                int zd = ((j * BS * CHUNKD + BS * CHUNKD2) - eye2);
                stale[stale_len].y = (xd * xd + zd * zd);

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
        TIMER(buildvbo);
        for (size_t my = 0; my < fresh_len; my++)
        {
                int myx = fresh[my].x;
                int myz = fresh[my].z;
                int xlo = myx * CHUNKW;
                int xhi = xlo + CHUNKW;
                int zlo = myz * CHUNKD;
                int zhi = zlo + CHUNKD;
                int ungenerated = false;

                #pragma omp critical
                if (!AGEN_(myx, myz))
                {
                        ungenerated = true;
                }

                if (ungenerated)
                        continue; // don't bother with ungenerated chunks

                //glBindVertexArray(VAO_(myx, myz));
                //glBindBuffer(GL_ARRAY_BUFFER, VBO_(myx, myz));
                v = vbuf; // reset vertex buffer pointer
                w = wbuf; // same for water buffer

                TIMER(buildvbo);

                for (int z = zlo; z < zhi; z++) for (int y = 0; y < TILESH; y++) for (int x = xlo; x < xhi; x++)
                {
                        if (v >= v_limit) break; // out of vertex space, shouldnt reasonably happen

                        if (w >= w_limit) w -= 10; // just overwrite water if we run out of space

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
                                if (y == 0        || T_(x  , y-1, z  ) >= OPEN) *v++ = (struct vbufv){ 0,    UP, m, y, n, usw, use, unw, une, USW, USE, UNW, UNE, 1 };
                                if (z == 0        || T_(x  , y  , z-1) >= OPEN) *v++ = (struct vbufv){ 1, SOUTH, m, y, n, use, usw, dse, dsw, USE, USW, DSE, DSW, 1 };
                                if (z == TILESD-1 || T_(x  , y  , z+1) >= OPEN) *v++ = (struct vbufv){ 1, NORTH, m, y, n, unw, une, dnw, dne, UNW, UNE, DNW, DNE, 1 };
                                if (x == 0        || T_(x-1, y  , z  ) >= OPEN) *v++ = (struct vbufv){ 1,  WEST, m, y, n, usw, unw, dsw, dnw, USW, UNW, DSW, DNW, 1 };
                                if (x == TILESW-1 || T_(x+1, y  , z  ) >= OPEN) *v++ = (struct vbufv){ 1,  EAST, m, y, n, une, use, dne, dse, UNE, USE, DNE, DSE, 1 };
                                if (y <  TILESH-1 && T_(x  , y+1, z  ) >= OPEN) *v++ = (struct vbufv){ 2,  DOWN, m, y, n, dse, dsw, dne, dnw, DSE, DSW, DNE, DNW, 1 };
                        }
                        else if (t == DIRT || t == GRG1 || t == GRG2)
                        {
                                int u = (t == DIRT) ? 2 :
                                        (t == GRG1) ? 3 : 4;
                                if (y == 0        || T_(x  , y-1, z  ) >= OPEN) *v++ = (struct vbufv){ u,    UP, m, y, n, usw, use, unw, une, USW, USE, UNW, UNE, 1 };
                                if (z == 0        || T_(x  , y  , z-1) >= OPEN) *v++ = (struct vbufv){ 2, SOUTH, m, y, n, use, usw, dse, dsw, USE, USW, DSE, DSW, 1 };
                                if (z == TILESD-1 || T_(x  , y  , z+1) >= OPEN) *v++ = (struct vbufv){ 2, NORTH, m, y, n, unw, une, dnw, dne, UNW, UNE, DNW, DNE, 1 };
                                if (x == 0        || T_(x-1, y  , z  ) >= OPEN) *v++ = (struct vbufv){ 2,  WEST, m, y, n, usw, unw, dsw, dnw, USW, UNW, DSW, DNW, 1 };
                                if (x == TILESW-1 || T_(x+1, y  , z  ) >= OPEN) *v++ = (struct vbufv){ 2,  EAST, m, y, n, une, use, dne, dse, UNE, USE, DNE, DSE, 1 };
                                if (y <  TILESH-1 && T_(x  , y+1, z  ) >= OPEN) *v++ = (struct vbufv){ 2,  DOWN, m, y, n, dse, dsw, dne, dnw, DSE, DSW, DNE, DNW, 1 };
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
                                if (y == 0        || T_(x  , y-1, z  ) >= OPEN) *v++ = (struct vbufv){ f,    UP, m, y, n, usw, use, unw, une, USW, USE, UNW, UNE, 1 };
                                if (z == 0        || T_(x  , y  , z-1) >= OPEN) *v++ = (struct vbufv){ f, SOUTH, m, y, n, use, usw, dse, dsw, USE, USW, DSE, DSW, 1 };
                                if (z == TILESD-1 || T_(x  , y  , z+1) >= OPEN) *v++ = (struct vbufv){ f, NORTH, m, y, n, unw, une, dnw, dne, UNW, UNE, DNW, DNE, 1 };
                                if (x == 0        || T_(x-1, y  , z  ) >= OPEN) *v++ = (struct vbufv){ f,  WEST, m, y, n, usw, unw, dsw, dnw, USW, UNW, DSW, DNW, 1 };
                                if (x == TILESW-1 || T_(x+1, y  , z  ) >= OPEN) *v++ = (struct vbufv){ f,  EAST, m, y, n, une, use, dne, dse, UNE, USE, DNE, DSE, 1 };
                                if (y <  TILESH-1 && T_(x  , y+1, z  ) >= OPEN) *v++ = (struct vbufv){ f,  DOWN, m, y, n, dse, dsw, dne, dnw, DSE, DSW, DNE, DNW, 1 };
                        }
                        else if (t == WATR)
                        {
                                if (y == 0        || T_(x  , y-1, z  ) == OPEN)
                                {
                                        int f = 7 + (pframe / 10 + (x ^ z)) % 4;
                                        *w++ = (struct vbufv){ f,    UP, m, y+0.06f, n, usw, use, unw, une, USW, USE, UNW, UNE, 0.5f };
                                        *w++ = (struct vbufv){ f,  DOWN, m, y-0.94f, n, dse, dsw, dne, dnw, DSE, DSW, DNE, DNW, 0.5f };
                                }
                        }
                        else if (t == LITE)
                        {
                                *w++ = (struct vbufv){ 18, SOUTH, m     , y, n+0.5f, use, usw, dse, dsw, 1.3f, 1.3f, 1.3f, 1.3f, 1 };
                                *w++ = (struct vbufv){ 18, NORTH, m     , y, n-0.5f, unw, une, dnw, dne, 1.3f, 1.3f, 1.3f, 1.3f, 1 };
                                *w++ = (struct vbufv){ 18,  WEST, m+0.5f, y, n     , usw, unw, dsw, dnw, 1.3f, 1.3f, 1.3f, 1.3f, 1 };
                                *w++ = (struct vbufv){ 18,  EAST, m-0.5f, y, n     , une, use, dne, dse, 1.3f, 1.3f, 1.3f, 1.3f, 1 };
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
                                *w++ = (struct vbufv){ f,    UP, m, ty+0.99f, n, lit, lit, lit, lit, lit, lit, lit, lit, 1.f };
                                *w++ = (struct vbufv){ f,  DOWN, m, ty-0.01f, n, lit, lit, lit, lit, lit, lit, lit, lit, 1.f };
                        }
                }

                if (w - wbuf < v_limit - v) // room for water in vertex buffer?
                {
                        memcpy(v, wbuf, (w - wbuf) * sizeof *wbuf);
                        v += w - wbuf;
                }

                VBOLEN_(myx, myz) = v - vbuf;
                polys += VBOLEN_(myx, myz);
                TIMER(glBufferData)

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

                int buffer_idx = myx * VAOD + myz;
                int offset = myz * world_aligned_sz;
                void *data;
                vkMapMemory(vk.device, world_mem[myx], offset, world_aligned_sz, 0, &data);
                memcpy(data, vbuf, (v - vbuf) * sizeof *vbuf);
                vkUnmapMemory(vk.device, world_mem[myx]);

                //glBufferData(GL_ARRAY_BUFFER, VBOLEN_(myx, myz) * sizeof *vbuf, vbuf, GL_STATIC_DRAW);
                if (my < 4) // draw the newly buffered verts
                {
                        TIMER(glDrawArrays)
                        main_ubo.model[12] = myx * BS * CHUNKW;
                        main_ubo.model[13] = 0.f;
                        main_ubo.model[14] = myz * BS * CHUNKD;

                        // send UBO
                        void* data;
                        vkMapMemory(vk.device, main_memory, 0, sizeof main_ubo, 0, &data);
                        memcpy(data, &main_ubo, sizeof main_ubo);
                        vkUnmapMemory(vk.device, main_memory);

                        //glUniformMatrix4fv(glGetUniformLocation(prog_id, "model"), 1, GL_FALSE, model_mtrx);
                        //glDrawArrays(GL_POINTS, 0, VBOLEN_(myx, myz));
                }
        }

        // Render terrain
        VkCommandBuffer cmdbuf = vk.commandBuffers[vk.imageIndex];
        vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipelines[main_pipe].pipeline);

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
                        // Skip frustum/range check for now - coords don't match player position
                        // if (!chunk_in_frustum(proj_view_mtrx, i, j) || !chunk_in_range(i, j)) continue;

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

        TIMER(swapwindow);
        vulkan_submit();
        TIMER();
}

#endif // BLOCKO_DRAW_C_INCLUDED

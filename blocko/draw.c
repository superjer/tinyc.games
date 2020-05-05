#include "blocko.h"

int is_framebuffer_incomplete()
{
        int status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        switch (status)
        {
                case GL_FRAMEBUFFER_COMPLETE:
                        return 0;
                case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
                        printf("framebuffer status: %d incomplete attachment\n", status); return 1;
                case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
                        printf("framebuffer status: %d missing attachment\n", status); return 1;
                case GL_FRAMEBUFFER_UNSUPPORTED:
                        printf("framebuffer status: %d unsupported\n", status); return 1;
                default:
                        printf("framebuffer status: %d (unknown)\n", status); return 1;
        }
}

int sorter(const void * _a, const void * _b)
{
        const struct qitem *a = _a;
        const struct qitem *b = _b;
        return (a->y == b->y) ?  0 :
               (a->y <  b->y) ?  1 : -1;
}

//draw everything in the game on the screen
void draw_stuff()
{
        float identityM[] = {
                1, 0, 0, 0,
                0, 1, 0, 0,
                0, 0, 1, 0,
                0, 0, 0, 1,
        };

        float shadow_space[16];

        glDisable(GL_MULTISAMPLE);

        // make shadow map
        if (shadow_mapping)
        {
                glBindFramebuffer(GL_FRAMEBUFFER, shadow_fbo);
                if (is_framebuffer_incomplete()) goto fb_is_bad;

                glViewport(0, 0, SHADOW_SZ, SHADOW_SZ);
                glClear(GL_DEPTH_BUFFER_BIT);

                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glEnable(GL_DEPTH_TEST);
                glDepthFunc(GL_LEQUAL);
                glDepthMask(GL_TRUE);
                glEnable(GL_CULL_FACE);
                glCullFace(GL_FRONT);
                glEnable(GL_POLYGON_OFFSET_FILL);
                glPolygonOffset(4.f, 4.f);

                //render shadows here
                glUseProgram(shadow_prog_id);
                // view matrix
                float viewM[16];
                float f[3];

                // prevent shaking shadows by quantizing sun pitch
                float quantizer;
                float qbracket = sinf(sun_pitch);
                if      (qbracket > 0.8f) quantizer = 0.001f;
                else if (qbracket > 0.6f) quantizer = 0.0005f;
                else if (qbracket > 0.4f) quantizer = 0.00025f;
                else if (qbracket > 0.2f) quantizer = 0.000125f;
                else                      quantizer = 0.0000625f;

                float quantized_sun_pitch = roundf(sun_pitch / quantizer) * quantizer;
                float yaw = 3.1415926535 * -0.5f;
                float dist2sun = (TILESW / 4) * BS;
                sun_pos.x = roundf(camplayer.pos.x / BS) * BS + dist2sun * sinf(-yaw) * cosf(quantized_sun_pitch);
                sun_pos.y = 100 * BS - dist2sun * sinf(quantized_sun_pitch);
                sun_pos.z = roundf(camplayer.pos.z / BS) * BS + dist2sun * cosf(-yaw) * cosf(quantized_sun_pitch);
                //snprintf(alert, 300, "sun_pitch %0.4f, quant %0.5f\n", sun_pitch, quantizer);
                lookit(viewM, f, sun_pos.x, sun_pos.y, sun_pos.z, quantized_sun_pitch, yaw);
                translate(viewM, -sun_pos.x, -sun_pos.y, -sun_pos.z);

                // proj matrix
                float snear = 10.f; // TODO find closest possible block
                float sfar = dist2sun + 9000.f;
                float x = 1.f / (6000 / 2.f);
                float y = -1.f / (6000 / 2.f);
                float z = -1.f / ((sfar - snear) / 2.f);
                float tz = -(sfar + snear) / (sfar - snear);
                float orthoM[] = {
                        x, 0, 0,  0,
                        0, y, 0,  0,
                        0, 0, z,  0,
                        0, 0, tz, 1,
                };

                glUniformMatrix4fv(glGetUniformLocation(shadow_prog_id, "proj"), 1, GL_FALSE, orthoM);
                glUniformMatrix4fv(glGetUniformLocation(shadow_prog_id, "view"), 1, GL_FALSE, viewM);
                glUniformMatrix4fv(glGetUniformLocation(shadow_prog_id, "model"), 1, GL_FALSE, identityM);
                glUniform1i(glGetUniformLocation(shadow_prog_id, "tarray"), 0);
                glUniform1f(glGetUniformLocation(shadow_prog_id, "BS"), BS);

                float biasM[] = {
                        0.5,   0,   0, 1,
                          0, 0.5,   0, 1,
                          0,   0, 0.5, 1,
                        0.5, 0.5, 0.5, 1,
                };
                float tmpM[16];
                mat4_multiply(tmpM, orthoM, viewM);
                mat4_multiply(shadow_space, biasM, tmpM);

                int shadow_poly = 0;
                for (int i = 0; i < VAOW; i++) for (int j = 0; j < VAOD; j++)
                {
                        int myvbo = i * VAOD + j;
                        if (vbo_len[myvbo] < 1) continue;
                        glBindVertexArray(vao[myvbo]);
                        glDrawArrays(GL_POINTS, 0, vbo_len[myvbo]);
                        shadow_poly += vbo_len[myvbo];
                }
        }
        fb_is_bad:
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDisable(GL_POLYGON_OFFSET_FILL);

        float night_amt;
        if (sun_pitch < PI) // in the day, linearly change the sky color
        {
                night_amt = fmodf(sun_pitch + 3*PI2, TAU) / TAU;
                if (night_amt > 0.5f) night_amt = 1.f - night_amt;
                night_amt *= 2.f;
        }
        else // at night change via cubic-sine so that it's mostly dark all night
        {
                night_amt = 1.f + sinf(sun_pitch);  //  0 to  1
                night_amt *= night_amt * night_amt; //  0 to  1
                night_amt *= -0.5f;                 //-.5 to  0
                night_amt += 1.f;                   //  1 to .5
        }

        if (night_amt > 0.5f)
        {
                fog_r = lerp(2.f*(night_amt - 0.5f), FOG_DUSK_R, FOG_NIGHT_R);
                fog_g = lerp(2.f*(night_amt - 0.5f), FOG_DUSK_G, FOG_NIGHT_G);
                fog_b = lerp(2.f*(night_amt - 0.5f), FOG_DUSK_B, FOG_NIGHT_B);
        }
        else
        {
                fog_r = lerp(2.f*night_amt, FOG_DAY_R, FOG_DUSK_R);
                fog_g = lerp(2.f*night_amt, FOG_DAY_G, FOG_DUSK_G);
                fog_b = lerp(2.f*night_amt, FOG_DAY_B, FOG_DUSK_B);
        }

        glViewport(0, 0, screenw, screenh);
        glClearColor(fog_r, fog_g, fog_b, 1.f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        if (antialiasing)
                glEnable(GL_MULTISAMPLE);

        // compute proj matrix
        float near = 8.f;
        float far = 99999.f;
        float frustw = 4.5f * zoom_amt * screenw / screenh;
        float frusth = 4.5f * zoom_amt;
        float projM[] = {
                near/frustw,           0,                                  0,  0,
                          0, near/frusth,                                  0,  0,
                          0,           0,       -(far + near) / (far - near), -1,
                          0,           0, -(2.f * far * near) / (far - near),  0
        };

        // compute view matrix
        float eye0 = lerped_pos.x + PLYR_W / 2;
        float eye1 = lerped_pos.y + EYEDOWN * (camplayer.sneaking ? 2 : 1);
        float eye2 = lerped_pos.z + PLYR_W / 2;
        float f[3];
        float viewM[16];
        lookit(viewM, f, eye0, eye1, eye2, camplayer.pitch, camplayer.yaw);

        sun_draw(projM, viewM, sun_pitch, shadow_tex_id);

        // find where we are pointing at
        rayshot(eye0, eye1, eye2, f[0], f[1], f[2]);

        // translate by hand
        float translated_viewM[16];
        memcpy(translated_viewM, viewM, sizeof viewM);
        translate(translated_viewM, -eye0, -eye1, -eye2);

        static float pvM[16];
        if (!lock_culling)
                mat4_multiply(pvM, projM, translated_viewM);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glDepthMask(GL_TRUE);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);

        glUseProgram(prog_id);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D_ARRAY, material_tex_id);
        glUniform1i(glGetUniformLocation(prog_id, "tarray"), 0);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, shadow_tex_id);
        glUniform1i(glGetUniformLocation(prog_id, "shadow_map"), 1);
        glUniform1i(glGetUniformLocation(prog_id, "shadow_mapping"), shadow_mapping);

        glUniformMatrix4fv(glGetUniformLocation(prog_id, "proj"), 1, GL_FALSE, projM);
        glUniformMatrix4fv(glGetUniformLocation(prog_id, "view"), 1, GL_FALSE, translated_viewM);
        glUniformMatrix4fv(glGetUniformLocation(prog_id, "model"), 1, GL_FALSE, identityM);
        glUniformMatrix4fv(glGetUniformLocation(prog_id, "shadow_space"), 1, GL_FALSE, shadow_space);

        glUniform1f(glGetUniformLocation(prog_id, "BS"), BS);
        glUniform3f(glGetUniformLocation(prog_id, "light_pos"), sun_pos.x, sun_pos.y, sun_pos.z);
        glUniform3f(glGetUniformLocation(prog_id, "view_pos"), eye0, eye1, eye2);

        {
                float m = ICLAMP(night_amt * 2.f, 0.f, 1.f);
                glUniform1f(glGetUniformLocation(prog_id, "sharpness"), m*m*m*(m*(m*6.f-15.f)+10.f));

                float r = lerp(night_amt, DAY_R, NIGHT_R);
                float g = lerp(night_amt, DAY_G, NIGHT_G);
                float b = lerp(night_amt, DAY_B, NIGHT_B);
                glUniform3f(glGetUniformLocation(prog_id, "day_color"), r, g, b);
                glUniform3f(glGetUniformLocation(prog_id, "glo_color"), 0.92f, 0.83f, 0.69f);
                glUniform3f(glGetUniformLocation(prog_id, "fog_color"), fog_r, fog_g, fog_b);
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

                // skip chunks we can't see anyways
                if (frustum_culling)
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
                                mat4_f3_multiply(v, pvM,
                                                i*BS*CHUNKW + x*BS*CHUNKW,
                                                          0 + y*BS*TILESH,
                                                j*BS*CHUNKD + z*BS*CHUNKD);
                                if (v[0] < -v[3]) x_too_lo++;
                                if (v[0] >  v[3]) x_too_hi++;
                                if (v[1] < -v[3]) y_too_lo++;
                                if (v[1] >  v[3]) y_too_hi++;
                                if (v[2] < -v[3]) z_too_lo++;
                                if (v[2] >  v[3]) z_too_hi++;
                                if (v[3] <   0.f) w_too_lo++;
                        }

                        if (x_too_lo == 8 || x_too_hi == 8 ||
                            y_too_lo == 8 || y_too_hi == 8 ||
                            z_too_lo == 8 || z_too_hi == 8 ||
                            w_too_lo == 8)
                                goto skip;
                }

                stale_len++;

                skip: ;
        }

        qsort(stale, stale_len, sizeof(struct qitem), sorter);
        for (size_t my = 0; my < stale_len; my++)
        {
                int myvbo = stale[my].x * VAOD + stale[my].z;
                glBindVertexArray(vao[myvbo]);
                glDrawArrays(GL_POINTS, 0, vbo_len[myvbo]);
                polys += vbo_len[myvbo];
        }

        // package, ship and render fresh chunks (while the stales are rendering!)
        TIMER(buildvbo);
        for (size_t my = 0; my < fresh_len; my++)
        {
                int myx = fresh[my].x;
                int myz = fresh[my].z;
                int myvbo = myx * VAOD + myz;
                int xlo = myx * CHUNKW;
                int xhi = xlo + CHUNKW;
                int zlo = myz * CHUNKD;
                int zhi = zlo + CHUNKD;
                int ungenerated = false;

                #pragma omp critical
                if (!already_generated[myx][myz])
                {
                        ungenerated = true;
                }

                if (ungenerated)
                        continue; // don't bother with ungenerated chunks

                glBindVertexArray(vao[myvbo]);
                glBindBuffer(GL_ARRAY_BUFFER, vbo[myvbo]);
                v = vbuf; // reset vertex buffer pointer
                w = wbuf; // same for water buffer

                TIMECALL(recalc_corner_lighting, (xlo, xhi, zlo, zhi));
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
                        if (t == GRAS)
                        {
                                if (y == 0        || T_(x  , y-1, z  ) >= OPEN) *v++ = (struct vbufv){ 0,    UP, x, y, z, usw, use, unw, une, USW, USE, UNW, UNE, 1 };
                                if (z == 0        || T_(x  , y  , z-1) >= OPEN) *v++ = (struct vbufv){ 1, SOUTH, x, y, z, use, usw, dse, dsw, USE, USW, DSE, DSW, 1 };
                                if (z == TILESD-1 || T_(x  , y  , z+1) >= OPEN) *v++ = (struct vbufv){ 1, NORTH, x, y, z, unw, une, dnw, dne, UNW, UNE, DNW, DNE, 1 };
                                if (x == 0        || T_(x-1, y  , z  ) >= OPEN) *v++ = (struct vbufv){ 1,  WEST, x, y, z, usw, unw, dsw, dnw, USW, UNW, DSW, DNW, 1 };
                                if (x == TILESW-1 || T_(x+1, y  , z  ) >= OPEN) *v++ = (struct vbufv){ 1,  EAST, x, y, z, une, use, dne, dse, UNE, USE, DNE, DSE, 1 };
                                if (y <  TILESH-1 && T_(x  , y+1, z  ) >= OPEN) *v++ = (struct vbufv){ 2,  DOWN, x, y, z, dse, dsw, dne, dnw, DSE, DSW, DNE, DNW, 1 };
                        }
                        else if (t == DIRT || t == GRG1 || t == GRG2)
                        {
                                int u = (t == DIRT) ? 2 :
                                        (t == GRG1) ? 3 : 4;
                                if (y == 0        || T_(x  , y-1, z  ) >= OPEN) *v++ = (struct vbufv){ u,    UP, x, y, z, usw, use, unw, une, USW, USE, UNW, UNE, 1 };
                                if (z == 0        || T_(x  , y  , z-1) >= OPEN) *v++ = (struct vbufv){ 2, SOUTH, x, y, z, use, usw, dse, dsw, USE, USW, DSE, DSW, 1 };
                                if (z == TILESD-1 || T_(x  , y  , z+1) >= OPEN) *v++ = (struct vbufv){ 2, NORTH, x, y, z, unw, une, dnw, dne, UNW, UNE, DNW, DNE, 1 };
                                if (x == 0        || T_(x-1, y  , z  ) >= OPEN) *v++ = (struct vbufv){ 2,  WEST, x, y, z, usw, unw, dsw, dnw, USW, UNW, DSW, DNW, 1 };
                                if (x == TILESW-1 || T_(x+1, y  , z  ) >= OPEN) *v++ = (struct vbufv){ 2,  EAST, x, y, z, une, use, dne, dse, UNE, USE, DNE, DSE, 1 };
                                if (y <  TILESH-1 && T_(x  , y+1, z  ) >= OPEN) *v++ = (struct vbufv){ 2,  DOWN, x, y, z, dse, dsw, dne, dnw, DSE, DSW, DNE, DNW, 1 };
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
                                if (y == 0        || T_(x  , y-1, z  ) >= OPEN) *v++ = (struct vbufv){ f,    UP, x, y, z, usw, use, unw, une, USW, USE, UNW, UNE, 1 };
                                if (z == 0        || T_(x  , y  , z-1) >= OPEN) *v++ = (struct vbufv){ f, SOUTH, x, y, z, use, usw, dse, dsw, USE, USW, DSE, DSW, 1 };
                                if (z == TILESD-1 || T_(x  , y  , z+1) >= OPEN) *v++ = (struct vbufv){ f, NORTH, x, y, z, unw, une, dnw, dne, UNW, UNE, DNW, DNE, 1 };
                                if (x == 0        || T_(x-1, y  , z  ) >= OPEN) *v++ = (struct vbufv){ f,  WEST, x, y, z, usw, unw, dsw, dnw, USW, UNW, DSW, DNW, 1 };
                                if (x == TILESW-1 || T_(x+1, y  , z  ) >= OPEN) *v++ = (struct vbufv){ f,  EAST, x, y, z, une, use, dne, dse, UNE, USE, DNE, DSE, 1 };
                                if (y <  TILESH-1 && T_(x  , y+1, z  ) >= OPEN) *v++ = (struct vbufv){ f,  DOWN, x, y, z, dse, dsw, dne, dnw, DSE, DSW, DNE, DNW, 1 };
                        }
                        else if (t == WATR)
                        {
                                if (y == 0        || T_(x  , y-1, z  ) == OPEN)
                                {
                                        int f = 7 + (pframe / 10 + (x ^ z)) % 4;
                                        *w++ = (struct vbufv){ f,    UP, x, y+0.06f, z, usw, use, unw, une, USW, USE, UNW, UNE, 0.5f };
                                        *w++ = (struct vbufv){ f,  DOWN, x, y-0.94f, z, dse, dsw, dne, dnw, DSE, DSW, DNE, DNW, 0.5f };
                                }
                        }
                        else if (t == LITE)
                        {
                                *w++ = (struct vbufv){ 18, SOUTH, x     , y, z+0.5f, use, usw, dse, dsw, 1.3f, 1.3f, 1.3f, 1.3f, 1 };
                                *w++ = (struct vbufv){ 18, NORTH, x     , y, z-0.5f, unw, une, dnw, dne, 1.3f, 1.3f, 1.3f, 1.3f, 1 };
                                *w++ = (struct vbufv){ 18,  WEST, x+0.5f, y, z     , usw, unw, dsw, dnw, 1.3f, 1.3f, 1.3f, 1.3f, 1 };
                                *w++ = (struct vbufv){ 18,  EAST, x-0.5f, y, z     , une, use, dne, dse, 1.3f, 1.3f, 1.3f, 1.3f, 1 };
                        }

                        if (show_light_values && in_test_area(x, y, z))
                        {
                                int f = GLO_(x, y, z) + PNG0;
                                int ty = y;
                                float lit = 1.f;
                                if (IS_OPAQUE(x, y, z))
                                {
                                        ty = y - 1;
                                        lit = 0.1f;
                                }
                                *w++ = (struct vbufv){ f,    UP, x, ty+0.9f, z, lit, lit, lit, lit, lit, lit, lit, lit, 1.f };
                                *w++ = (struct vbufv){ f,  DOWN, x, ty-0.1f, z, lit, lit, lit, lit, lit, lit, lit, lit, 1.f };
                        }
                }

                if (w - wbuf < v_limit - v) // room for water in vertex buffer?
                {
                        memcpy(v, wbuf, (w - wbuf) * sizeof *wbuf);
                        v += w - wbuf;
                }

                vbo_len[myvbo] = v - vbuf;
                polys += vbo_len[myvbo];
                TIMER(glBufferData)
                glBufferData(GL_ARRAY_BUFFER, vbo_len[myvbo] * sizeof *vbuf, vbuf, GL_STATIC_DRAW);
                if (my < 4) // draw the newly buffered verts
                {
                        TIMER(glDrawArrays)
                        glDrawArrays(GL_POINTS, 0, vbo_len[myvbo]);
                }
        }

        debrief();

        TIMER(swapwindow);
        SDL_GL_SwapWindow(win);
        TIMER();
}


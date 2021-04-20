#include "oilchange.h"

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

        float modelM[16];
        memcpy(modelM, identityM, sizeof identityM);

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

                float moon_pitch = sun_pitch + PI;
                if (moon_pitch < 0) moon_pitch += TAU;

                float yaw = 3.1415926535 * -0.5f;
                sun_pos.x = 100;
                sun_pos.y = 100;
                sun_pos.z = 100;
                float pitch = -0.8f;

                lookit(viewM, f, sun_pos.x, sun_pos.y, sun_pos.z, pitch, yaw);
                translate(viewM, -sun_pos.x, -sun_pos.y, -sun_pos.z);

                // proj matrix
                float snear = 10.f; // TODO find closest possible block
                float sfar = 10.f + 9000.f;
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

                float shadow_pvM[16];
                if (!lock_culling)
                        mat4_multiply(shadow_pvM, orthoM, viewM);

                glUniformMatrix4fv(glGetUniformLocation(shadow_prog_id, "proj"), 1, GL_FALSE, orthoM);
                glUniformMatrix4fv(glGetUniformLocation(shadow_prog_id, "view"), 1, GL_FALSE, viewM);
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

                for (int i = 0; i < VAOW; i++) for (int j = 0; j < VAOD; j++)
                {
                        if (!VBOLEN_(i, j)) continue;
                        if (!frustum_culling || 1)
                        {
                                glBindVertexArray(VAO_(i, j));
                                modelM[12] = i * BS * CHUNKW;
                                modelM[14] = j * BS * CHUNKD;
                                glUniformMatrix4fv(glGetUniformLocation(shadow_prog_id, "model"), 1, GL_FALSE, modelM);
                                glDrawArrays(GL_POINTS, 0, VBOLEN_(i, j));
                                shadow_polys += VBOLEN_(i, j);
                        }
                }

                fb_is_bad:
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
                glDisable(GL_POLYGON_OFFSET_FILL);
        }

        fog_r = 0.8f;
        fog_g = 0.8f;
        fog_b = 0.8f;

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
        glUniformMatrix4fv(glGetUniformLocation(prog_id, "shadow_space"), 1, GL_FALSE, shadow_space);

        glUniform1f(glGetUniformLocation(prog_id, "BS"), BS);

        if (sun_pitch < PI)
                glUniform3f(glGetUniformLocation(prog_id, "light_pos"), sun_pos.x, sun_pos.y, sun_pos.z);
        else
                glUniform3f(glGetUniformLocation(prog_id, "light_pos"), moon_pos.x, moon_pos.y, moon_pos.z);

        glUniform3f(glGetUniformLocation(prog_id, "view_pos"), eye0, eye1, eye2);

        {
                float m = 0.8f;
                glUniform1f(glGetUniformLocation(prog_id, "sharpness"), m*m*m*(m*(m*6.f-15.f)+10.f));

                float r = 0.9f;
                float g = 0.9f;
                float b = 0.9f;
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

        //qsort(fresh, fresh_len, sizeof(struct qitem), sorter);

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

                stale_len++;

                skip: ;
        }

        for (size_t my = 0; my < stale_len; my++)
        {
                int myx = stale[my].x;
                int myz = stale[my].z;
                modelM[12] = myx * BS * CHUNKW;
                modelM[14] = myz * BS * CHUNKD;
                glUniformMatrix4fv(glGetUniformLocation(prog_id, "model"), 1, GL_FALSE, modelM);
                glBindVertexArray(VAO_(myx, myz));
                glDrawArrays(GL_POINTS, 0, VBOLEN_(myx, myz));
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

                glBindVertexArray(VAO_(myx, myz));
                glBindBuffer(GL_ARRAY_BUFFER, VBO_(myx, myz));
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
                                int f = GLO_(x, y, z) + PNG0;
                                int ty = y;
                                float lit = 1.f;
                                if (IS_OPAQUE(x, y, z))
                                {
                                        ty = y - 1;
                                        lit = 0.1f;
                                }
                                *w++ = (struct vbufv){ f,    UP, m, ty+0.9f, n, lit, lit, lit, lit, lit, lit, lit, lit, 1.f };
                                *w++ = (struct vbufv){ f,  DOWN, m, ty-0.1f, n, lit, lit, lit, lit, lit, lit, lit, lit, 1.f };
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
                glBufferData(GL_ARRAY_BUFFER, VBOLEN_(myx, myz) * sizeof *vbuf, vbuf, GL_STATIC_DRAW);
                if (my < 4) // draw the newly buffered verts
                {
                        TIMER(glDrawArrays)
                        modelM[12] = myx * BS * CHUNKW;
                        modelM[13] = 0.f;
                        modelM[14] = myz * BS * CHUNKD;
                        glUniformMatrix4fv(glGetUniformLocation(prog_id, "model"), 1, GL_FALSE, modelM);
                        glDrawArrays(GL_POINTS, 0, VBOLEN_(myx, myz));
                }
        }

        debrief();

        TIMER(swapwindow);
        SDL_GL_SwapWindow(win);
        TIMER();
}


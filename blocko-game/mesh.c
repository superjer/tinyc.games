#include "blocko.c"
#ifndef BLOCKO_MESH_C_INCLUDED
#define BLOCKO_MESH_C_INCLUDED

static int sorter(const void * _a, const void * _b)
{
        const struct qitem *a = _a;
        const struct qitem *b = _b;
        return (a->d == b->d) ?  0 :
               (a->d <  b->d) ? -1 : 1;  // closest first
}

// face visibility - draw a face only against a see-through neighbor
#define UP_VISIBLE(x, y, z)    (y == 0          || IS_SEE_THROUGH(T_(x, y-1, z)))
#define DOWN_VISIBLE(x, y, z)  (y+1 >= TILESH   || IS_SEE_THROUGH(T_(x, y+1, z)))
#define SOUTH_VISIBLE(x, y, z) (z == 0          || IS_SEE_THROUGH(T_(x, y, z-1)))
#define NORTH_VISIBLE(x, y, z) (z+1 >= TILESD   || IS_SEE_THROUGH(T_(x, y, z+1)))
#define WEST_VISIBLE(x, y, z)  (x == 0          || IS_SEE_THROUGH(T_(x-1, y, z)))
#define EAST_VISIBLE(x, y, z)  (x+1 >= TILESW   || IS_SEE_THROUGH(T_(x+1, y, z)))

void build_meshes()
{
        // Collect all dirty chunks with distances from current player position
        struct qitem fresh[VAOW*VAOD];
        size_t fresh_len = 0;

        // Player's chunk (avoid overflow by using chunk units, not world units)
        int player_chunk_x = (int)(peye0 / (BS * CHUNKW));
        int player_chunk_z = (int)(peye2 / (BS * CHUNKD));

        for (int i = 0; i < VAOW; i++) {
                for (int j = 0; j < VAOD; j++) {
                        unsigned light_stamp = LIGHTDIRTY_(i, j);
                        if (!DIRTY_(i, j))
                        {
                                if (!light_stamp) continue;
                                // light-only changes: wait for the light here
                                // to stop changing before remeshing
                                if ((unsigned)frame + 1 - light_stamp < (unsigned)remesh_debounce)
                                        continue;
                        }
                        if (!AGEN_(i, j)) continue;

                        int xd = i - player_chunk_x;
                        int zd = j - player_chunk_z;
                        fresh[fresh_len].x = i;
                        fresh[fresh_len].z = j;
                        fresh[fresh_len].d = xd * xd + zd * zd;
                        fresh_len++;
                }
        }

        // sort by distance (closest first)
        qsort(fresh, fresh_len, sizeof(struct qitem), sorter);

        // package & ship chunks to gpu
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
                        for (int x = xlo; x < xhi; x++) for (int y = 0; y < TILESH; y++)
                        {
                                if (tv >= tv_limit) break;

                                int t = T_(x, y, z);

                                if (t == OPEN)
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
                                int m = x & (CHUNKW-1);
                                int n = z & (CHUNKD-1);

                                if (t == GRAS)
                                {
                                        if ((face_mask & FACE_UP)    && UP_VISIBLE(x, y, z))    *tv++ = (struct vbufv){ 0,    UP, m, y, n, usw, use, unw, une, USW, USE, UNW, UNE, 1 };
                                        if ((face_mask & FACE_SOUTH) && SOUTH_VISIBLE(x, y, z)) *tv++ = (struct vbufv){ 1, SOUTH, m, y, n, use, usw, dse, dsw, USE, USW, DSE, DSW, 1 };
                                        if ((face_mask & FACE_NORTH) && NORTH_VISIBLE(x, y, z)) *tv++ = (struct vbufv){ 1, NORTH, m, y, n, unw, une, dnw, dne, UNW, UNE, DNW, DNE, 1 };
                                        if ((face_mask & FACE_WEST)  && WEST_VISIBLE(x, y, z))  *tv++ = (struct vbufv){ 1,  WEST, m, y, n, usw, unw, dsw, dnw, USW, UNW, DSW, DNW, 1 };
                                        if ((face_mask & FACE_EAST)  && EAST_VISIBLE(x, y, z))  *tv++ = (struct vbufv){ 1,  EAST, m, y, n, une, use, dne, dse, UNE, USE, DNE, DSE, 1 };
                                        if ((face_mask & FACE_DOWN)  && DOWN_VISIBLE(x, y, z))  *tv++ = (struct vbufv){ 2,  DOWN, m, y, n, dse, dsw, dne, dnw, DSE, DSW, DNE, DNW, 1 };
                                }
                                else if (t == DIRT)
                                {
                                        if ((face_mask & FACE_UP)    && UP_VISIBLE(x, y, z))    *tv++ = (struct vbufv){ 2,    UP, m, y, n, usw, use, unw, une, USW, USE, UNW, UNE, 1 };
                                        if ((face_mask & FACE_SOUTH) && SOUTH_VISIBLE(x, y, z)) *tv++ = (struct vbufv){ 2, SOUTH, m, y, n, use, usw, dse, dsw, USE, USW, DSE, DSW, 1 };
                                        if ((face_mask & FACE_NORTH) && NORTH_VISIBLE(x, y, z)) *tv++ = (struct vbufv){ 2, NORTH, m, y, n, unw, une, dnw, dne, UNW, UNE, DNW, DNE, 1 };
                                        if ((face_mask & FACE_WEST)  && WEST_VISIBLE(x, y, z))  *tv++ = (struct vbufv){ 2,  WEST, m, y, n, usw, unw, dsw, dnw, USW, UNW, DSW, DNW, 1 };
                                        if ((face_mask & FACE_EAST)  && EAST_VISIBLE(x, y, z))  *tv++ = (struct vbufv){ 2,  EAST, m, y, n, une, use, dne, dse, UNE, USE, DNE, DSE, 1 };
                                        if ((face_mask & FACE_DOWN)  && DOWN_VISIBLE(x, y, z))  *tv++ = (struct vbufv){ 2,  DOWN, m, y, n, dse, dsw, dne, dnw, DSE, DSW, DNE, DNW, 1 };
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
                                        if ((face_mask & FACE_UP)    && UP_VISIBLE(x, y, z))    *tv++ = (struct vbufv){ f,    UP, m, y, n, usw, use, unw, une, USW, USE, UNW, UNE, 1 };
                                        if ((face_mask & FACE_SOUTH) && SOUTH_VISIBLE(x, y, z)) *tv++ = (struct vbufv){ f, SOUTH, m, y, n, use, usw, dse, dsw, USE, USW, DSE, DSW, 1 };
                                        if ((face_mask & FACE_NORTH) && NORTH_VISIBLE(x, y, z)) *tv++ = (struct vbufv){ f, NORTH, m, y, n, unw, une, dnw, dne, UNW, UNE, DNW, DNE, 1 };
                                        if ((face_mask & FACE_WEST)  && WEST_VISIBLE(x, y, z))  *tv++ = (struct vbufv){ f,  WEST, m, y, n, usw, unw, dsw, dnw, USW, UNW, DSW, DNW, 1 };
                                        if ((face_mask & FACE_EAST)  && EAST_VISIBLE(x, y, z))  *tv++ = (struct vbufv){ f,  EAST, m, y, n, une, use, dne, dse, UNE, USE, DNE, DSE, 1 };
                                        if ((face_mask & FACE_DOWN)  && DOWN_VISIBLE(x, y, z))  *tv++ = (struct vbufv){ f,  DOWN, m, y, n, dse, dsw, dne, dnw, DSE, DSW, DNE, DNW, 1 };
                                }
                                else if (t == WATR)
                                {
                                        if (y == 0        || T_(x  , y-1, z  ) == OPEN)
                                        {
                                                                *tw++ = (struct vbufv){ 7,    UP, m, y+0.06f, n, usw, use, unw, une, USW, USE, UNW, UNE, 0.5f };
                                                *tw++ = (struct vbufv){ 7,  DOWN, m, y-0.94f, n, dse, dsw, dne, dnw, DSE, DSW, DNE, DNW, 0.5f };
                                        }
                                }
                                else if (t == LITE)
                                {
                                        *tw++ = (struct vbufv){ 18, SOUTH, m     , y, n+0.5f, use, usw, dse, dsw, 1.3f, 1.3f, 1.3f, 1.3f, 1 };
                                        *tw++ = (struct vbufv){ 18, NORTH, m     , y, n-0.5f, unw, une, dnw, dne, 1.3f, 1.3f, 1.3f, 1.3f, 1 };
                                        *tw++ = (struct vbufv){ 18,  WEST, m+0.5f, y, n     , usw, unw, dsw, dnw, 1.3f, 1.3f, 1.3f, 1.3f, 1 };
                                        *tw++ = (struct vbufv){ 18,  EAST, m-0.5f, y, n     , une, use, dne, dse, 1.3f, 1.3f, 1.3f, 1.3f, 1 };
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

                WBOSTART_(myx, myz) = v - vbuf;

                if (w - wbuf < v_limit - v) // room for water in vertex buffer?
                {
                        memcpy(v, wbuf, (w - wbuf) * sizeof *wbuf);
                        v += w - wbuf;
                }

                VBOLEN_(myx, myz) = v - vbuf;
                polys += VBOLEN_(myx, myz);
                TIMER(gpu_upload)

                void *data = WMAPPED_(myx, myz);
                memcpy(data, vbuf, (v - vbuf) * sizeof *vbuf);

                // Mark chunk as clean after mesh rebuild and store LOD state
                DIRTY_(myx, myz) = 0;
                LIGHTDIRTY_(myx, myz) = 0;
                FACES_(myx, myz) = face_mask;
                LOD_(myx, myz) = use_lod;
                meshes_built++;
                nr_meshes_built++;
        }
}

#endif // BLOCKO_MESH_C_INCLUDED

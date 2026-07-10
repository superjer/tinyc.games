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

// water face visibility - draw a side/bottom face wherever water meets air (or
// any see-through non-water neighbor), but never against solid or more water, so
// a body of water shows its exposed walls and floor with no internal faces.
// A side face is also skipped when the neighbor is unknowable: in an
// un-generated chunk (stale tiles would paint a spurious wall along the seam)
// or off the window edge. The solid macros above draw at the window edge
// instead, but a water wall there is a fake feature, and it outlives the edge:
// once the window scoots on, nothing re-dirties the chunk, so the baked wall
// would stand mid-ocean. (Within one chunk NBR_CHUNK_GEN is always true, so
// these gates only affect chunk boundaries.)
#define WATER_OPEN(x, y, z)      (IS_SEE_THROUGH(T_(x, y, z)) && T_(x, y, z) != WATR)
#define NBR_CHUNK_GEN(bx, bz)    AGEN_((bx) / CHUNKW, (bz) / CHUNKD)
#define W_SOUTH_VISIBLE(x, y, z) (z > 0          && WATER_OPEN(x, y, z-1) && NBR_CHUNK_GEN(x, z-1))
#define W_NORTH_VISIBLE(x, y, z) (z+1 < TILESD   && WATER_OPEN(x, y, z+1) && NBR_CHUNK_GEN(x, z+1))
#define W_WEST_VISIBLE(x, y, z)  (x > 0          && WATER_OPEN(x-1, y, z) && NBR_CHUNK_GEN(x-1, z))
#define W_EAST_VISIBLE(x, y, z)  (x+1 < TILESW   && WATER_OPEN(x+1, y, z) && NBR_CHUNK_GEN(x+1, z))
#define W_DOWN_VISIBLE(x, y, z)  (y+1 >= TILESH   || WATER_OPEN(x, y+1, z))

// Emit terrain vertices for an arbitrary box of cells into the global vbuf
// (opaque, v points past the end) and wbuf (water/glow, w past the end).
// Parameterized bounds let build_meshes mesh a whole chunk while the spike
// command (remote.c) measures the cost of smaller regions.
//
// pos_in (the per-face position baked into each vertex) is emitted relative to
// (origin_x, origin_z): build_meshes passes the chunk's (xlo, zlo) so pos_in is
// chunk-local (identical to the old x & (CHUNKW-1)); the patch mesh (patch.c)
// passes (0, 0) so pos_in is absolute window coords and can be drawn with a zero
// chunk origin over a box that straddles chunk seams.
void mesh_region(int xlo, int xhi, int ylo, int yhi, int zlo, int zhi, unsigned char face_mask,
                 int origin_x, int origin_z)
{
        v = vbuf; // reset vertex buffer pointer
        w = wbuf; // same for water buffer

        // Track per-thread vertex counts for merging
        int v_counts[MAX_MESH_THREADS] = {0};
        int w_counts[MAX_MESH_THREADS] = {0};
        int l_counts[MAX_MESH_THREADS] = {0};

        // carve the block being mined out of this mesh so a shaking stand-in
        // (mine.c) can take its place. Only a transient swap here - rayshot and
        // collision still see the block as solid. Gated on patch_meshing so only
        // the patch mesh (patch.c) carves: the big chunk buffers keep the block
        // solid and let the shader reject box hide it instead.
        int mine_carved = 0, mine_save = 0;
        if (patch_meshing && mine_hole && mine_x >= xlo && mine_x < xhi
                      && mine_y >= ylo && mine_y < yhi
                      && mine_z >= zlo && mine_z < zhi)
        {
                mine_save = T_(mine_x, mine_y, mine_z);
                if (mine_save != OPEN)
                {
                        T_(mine_x, mine_y, mine_z) = OPEN;
                        mine_carved = 1;
                }
        }

        #pragma omp parallel num_threads(mesh_threads)
        {
                // declared here (not in the for-init) for MSVC, whose
                // OpenMP 2.0 C mode rejects declarations in an omp for;
                // anything declared inside the parallel block is private
                int x, y, z;
                int tid = omp_get_thread_num();
                struct vbufv *tv = vbuf_mt[tid];
                struct vbufv *tv_start = tv;
                struct vbufv *tv_limit = tv + VERTEX_BUFLEN;
                struct vbufv *tw = wbuf_mt[tid];
                struct vbufv *tw_start = tw;
                struct vbufv *tw_limit = tw + VERTEX_BUFLEN;
                struct vbufv *tl = lbuf_mt[tid];
                struct vbufv *tl_start = tl;
                struct vbufv *tl_limit = tl + VERTEX_BUFLEN/8;

                #pragma omp for schedule(static)
                for (z = zlo; z < zhi; z++) {
                for (x = xlo; x < xhi; x++) for (y = ylo; y < yhi; y++)
                {
                        if (tv >= tv_limit || tw >= tw_limit) break;

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
                        int m = x - origin_x;
                        int n = z - origin_z;

                        if (t == GRAS)
                        {
                                if ((face_mask & FACE_UP)    && UP_VISIBLE(x, y, z))    *tv++ = (struct vbufv){ 0,    UP, m, y, n, usw, use, unw, une, USW, USE, UNW, UNE, 1 };
                                if ((face_mask & FACE_SOUTH) && SOUTH_VISIBLE(x, y, z)) *tv++ = (struct vbufv){ 1, SOUTH, m, y, n, use, usw, dse, dsw, USE, USW, DSE, DSW, 1 };
                                if ((face_mask & FACE_NORTH) && NORTH_VISIBLE(x, y, z)) *tv++ = (struct vbufv){ 1, NORTH, m, y, n, unw, une, dnw, dne, UNW, UNE, DNW, DNE, 1 };
                                if ((face_mask & FACE_WEST)  && WEST_VISIBLE(x, y, z))  *tv++ = (struct vbufv){ 1,  WEST, m, y, n, usw, unw, dsw, dnw, USW, UNW, DSW, DNW, 1 };
                                if ((face_mask & FACE_EAST)  && EAST_VISIBLE(x, y, z))  *tv++ = (struct vbufv){ 1,  EAST, m, y, n, une, use, dne, dse, UNE, USE, DNE, DSE, 1 };
                                if ((face_mask & FACE_DOWN)  && DOWN_VISIBLE(x, y, z))  *tv++ = (struct vbufv){ 2,  DOWN, m, y, n, dse, dsw, dne, dnw, DSE, DSW, DNE, DNW, 1 };
                        }
                        else if (t == MTGR)
                        {
                                if ((face_mask & FACE_UP)    && UP_VISIBLE(x, y, z))    *tv++ = (struct vbufv){ 37,    UP, m, y, n, usw, use, unw, une, USW, USE, UNW, UNE, 1 };
                                if ((face_mask & FACE_SOUTH) && SOUTH_VISIBLE(x, y, z)) *tv++ = (struct vbufv){ 38, SOUTH, m, y, n, use, usw, dse, dsw, USE, USW, DSE, DSW, 1 };
                                if ((face_mask & FACE_NORTH) && NORTH_VISIBLE(x, y, z)) *tv++ = (struct vbufv){ 38, NORTH, m, y, n, unw, une, dnw, dne, UNW, UNE, DNW, DNE, 1 };
                                if ((face_mask & FACE_WEST)  && WEST_VISIBLE(x, y, z))  *tv++ = (struct vbufv){ 38,  WEST, m, y, n, usw, unw, dsw, dnw, USW, UNW, DSW, DNW, 1 };
                                if ((face_mask & FACE_EAST)  && EAST_VISIBLE(x, y, z))  *tv++ = (struct vbufv){ 38,  EAST, m, y, n, une, use, dne, dse, UNE, USE, DNE, DSE, 1 };
                                if ((face_mask & FACE_DOWN)  && DOWN_VISIBLE(x, y, z))  *tv++ = (struct vbufv){  2,  DOWN, m, y, n, dse, dsw, dne, dnw, DSE, DSW, DNE, DNW, 1 };
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
                                 t == RLEF || t == YLEF || t == SLEF)
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
                                        (t == SLEF) ? 39 :
                                                       0 ;
                                // leaves collect apart so the merge lands them after
                                // all solid faces: shadow passes alpha-test just them.
                                // a full leaf buffer spills to solid (solid shadow)
                                int leaf = (t == RLEF || t == YLEF || t == SLEF) && tl < tl_limit;
                                struct vbufv *o = leaf ? tl : tv;
                                if ((face_mask & FACE_UP)    && UP_VISIBLE(x, y, z))    *o++ = (struct vbufv){ f,    UP, m, y, n, usw, use, unw, une, USW, USE, UNW, UNE, 1 };
                                if ((face_mask & FACE_SOUTH) && SOUTH_VISIBLE(x, y, z)) *o++ = (struct vbufv){ f, SOUTH, m, y, n, use, usw, dse, dsw, USE, USW, DSE, DSW, 1 };
                                if ((face_mask & FACE_NORTH) && NORTH_VISIBLE(x, y, z)) *o++ = (struct vbufv){ f, NORTH, m, y, n, unw, une, dnw, dne, UNW, UNE, DNW, DNE, 1 };
                                if ((face_mask & FACE_WEST)  && WEST_VISIBLE(x, y, z))  *o++ = (struct vbufv){ f,  WEST, m, y, n, usw, unw, dsw, dnw, USW, UNW, DSW, DNW, 1 };
                                if ((face_mask & FACE_EAST)  && EAST_VISIBLE(x, y, z))  *o++ = (struct vbufv){ f,  EAST, m, y, n, une, use, dne, dse, UNE, USE, DNE, DSE, 1 };
                                if ((face_mask & FACE_DOWN)  && DOWN_VISIBLE(x, y, z))  *o++ = (struct vbufv){ f,  DOWN, m, y, n, dse, dsw, dne, dnw, DSE, DSW, DNE, DNW, 1 };
                                if (leaf) tl = o; else tv = o;
                        }
                        else if (t == WATR)
                        {
                                // the water pipe is no-cull, so one quad per face is
                                // visible from both sides. the top layer (air above) has
                                // its surface recessed slightly below the block top; its
                                // faces are tagged with orient + 10 so the shader pulls
                                // the top EDGE down (top face drops, side walls shorten,
                                // bottom untouched). covered water (solid or more water
                                // above) stays full height - a recess there would leave a
                                // nonsense gap of air below the water above it.
                                int surf = (y == 0 || T_(x, y-1, z) == OPEN);
                                int rec = surf ? 10 : 0;
                                // top surface: only where air is directly above
                                if (surf)
                                        *tw++ = (struct vbufv){ 7,  UP+10, m, y, n, usw, use, unw, une, USW, USE, UNW, UNE, 0.5f };
                                // sides + bottom: the exposed walls and floor, so a body
                                // of water doesn't look like it's hovering
                                if (W_SOUTH_VISIBLE(x, y, z)) *tw++ = (struct vbufv){ 7, SOUTH+rec, m, y, n, use, usw, dse, dsw, USE, USW, DSE, DSW, 0.5f };
                                if (W_NORTH_VISIBLE(x, y, z)) *tw++ = (struct vbufv){ 7, NORTH+rec, m, y, n, unw, une, dnw, dne, UNW, UNE, DNW, DNE, 0.5f };
                                if (W_WEST_VISIBLE(x, y, z))  *tw++ = (struct vbufv){ 7,  WEST+rec, m, y, n, usw, unw, dsw, dnw, USW, UNW, DSW, DNW, 0.5f };
                                if (W_EAST_VISIBLE(x, y, z))  *tw++ = (struct vbufv){ 7,  EAST+rec, m, y, n, une, use, dne, dse, UNE, USE, DNE, DSE, 0.5f };
                                if (W_DOWN_VISIBLE(x, y, z))  *tw++ = (struct vbufv){ 7,      DOWN, m, y, n, dse, dsw, dne, dnw, DSE, DSW, DNE, DNW, 0.5f };
                        }
                        else if (t == LITE)
                        {
                                // a two-plane billboard (one quad per axis) crossed at
                                // the cell center. the water pipe is no-cull, so a single
                                // quad on each axis shows from both sides - emitting the
                                // opposite-facing twin too would just z-fight it.
                                *tw++ = (struct vbufv){ 18, SOUTH, m     , y, n+0.5f, use, usw, dse, dsw, 1.3f, 1.3f, 1.3f, 1.3f, 1 };
                                *tw++ = (struct vbufv){ 18,  WEST, m+0.5f, y, n     , usw, unw, dsw, dnw, 1.3f, 1.3f, 1.3f, 1.3f, 1 };
                        }
                        else if (t == TLGR || t == TMGR)
                        {
                                // tall grass: two crossed billboard planes like
                                // LITE, but the vertex shader rotates and jitters
                                // them per-cell (orient 20/21 = grass plane A/B).
                                // integer m,y,n so the shader can hash the cell;
                                // lit by the sky corners of the air cell it sits in.
                                // TLGR uses the lowland texture, TMGR the mountain one
                                int gtex = (t == TMGR) ? 41 : 40;
                                *tw++ = (struct vbufv){ gtex, 20, m, y, n, usw, use, unw, une, USW, USE, UNW, UNE, 1 };
                                *tw++ = (struct vbufv){ gtex, 21, m, y, n, usw, use, unw, une, USW, USE, UNW, UNE, 1 };
                        }

                }
                }

                // Store counts for this thread
                v_counts[tid] = tv - tv_start;
                w_counts[tid] = tw - tw_start;
                l_counts[tid] = tl - tl_start;
        }

        if (mine_carved) T_(mine_x, mine_y, mine_z) = mine_save;

        // Merge thread buffers into main buffer: all solid faces, then all
        // leaves, so the terrain section is [solid | leaves) split at
        // mesh_leaf_start (stored per chunk as LEAFSTART_)
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
        mesh_leaf_start = v - vbuf;
        for (int tid = 0; tid < MAX_MESH_THREADS; tid++)
                if (l_counts[tid] > 0 && v + l_counts[tid] <= v_limit) {
                        memcpy(v, lbuf_mt[tid], l_counts[tid] * sizeof *v);
                        v += l_counts[tid];
                }
}

// 2x2x2 shadow LOD mesh: downsample the chunk into coarse cells (solid only
// when all 8 blocks cast shadows - leaves count, water/lights/grass don't),
// then emit one face wherever a solid cell meets an empty one. The far and
// extreme shadow passes draw this section instead of the full mesh for
// distant chunks, with push.bs = 2*BS so the same shadow.vert scales the
// quads up (pos_in is in coarse-cell units).
//
// The all-8 rule matters: it under-approximates, so the coarse surface never
// pokes above the true surface. Any over-approximation (e.g. >=4/8) casts
// false shadows onto real terrain - shallow steps showed alternating dark
// bands wherever the rounded-up surface covered the lower step. The cost is
// the opposite, invisible error: shadows of distant features end up to one
// block short, and 1-block-thin features (trunks, thin roofs) stop casting
// at LOD distance.
#define TILESH2 (TILESH/2)
// occupancy with a 1-cell x/z ring from the neighbor chunks for face culling
static unsigned char lod_occ[CHUNKD2+2][CHUNKW2+2][TILESH2];
#define LODOCC_(cx,cz,cy) lod_occ[(cz)+1][(cx)+1][cy]

static struct vbufv *lod_region(struct vbufv *v, int xlo, int zlo)
{
        struct vbufv *start = v;

        #pragma omp parallel num_threads(mesh_threads)
        {
                int cx, cy, cz;
                #pragma omp for schedule(static)
                for (cz = -1; cz <= CHUNKD2; cz++) for (cx = -1; cx <= CHUNKW2; cx++)
                {
                        int bx = xlo + 2*cx;
                        int bz = zlo + 2*cz;
                        // ring cells outside the window: treat as empty (emit the
                        // face, matching the full mesh's window-edge convention).
                        // ring cells in an un-generated chunk: treat as solid
                        // (skip the face; the remesh when it generates fixes it)
                        if (bx < 0 || bx >= TILESW || bz < 0 || bz >= TILESD)
                        {
                                for (cy = 0; cy < TILESH2; cy++)
                                        LODOCC_(cx, cz, cy) = 0;
                                continue;
                        }
                        if (!NBR_CHUNK_GEN(bx, bz))
                        {
                                for (cy = 0; cy < TILESH2; cy++)
                                        LODOCC_(cx, cz, cy) = 1;
                                continue;
                        }
                        for (cy = 0; cy < TILESH2; cy++)
                        {
                                int by = 2*cy;
                                int n = 0;
                                for (int dz = 0; dz < 2; dz++)
                                for (int dx = 0; dx < 2; dx++)
                                for (int dy = 0; dy < 2; dy++)
                                {
                                        int t = T_(bx+dx, by+dy, bz+dz);
                                        if (t < OPEN || t == RLEF || t == YLEF || t == SLEF)
                                                n++;
                                }
                                LODOCC_(cx, cz, cy) = n == 8;
                        }
                }
        }

        for (int cz = 0; cz < CHUNKD2; cz++)
        for (int cx = 0; cx < CHUNKW2; cx++)
        for (int cy = 0; cy < TILESH2; cy++)
        {
                if (!LODOCC_(cx, cz, cy)) continue;
                if (v + 6 > v_limit) return start; // no room: draw full mesh instead
                // illum/glow are unused by the shadow pass; tex is never sampled
                // (the solid pipeline has no fragment shader)
                if (cy == 0          || !LODOCC_(cx, cz, cy-1)) *v++ = (struct vbufv){ 5,    UP, cx, cy, cz, 1,1,1,1, 0,0,0,0, 1 };
                if (cy+1 >= TILESH2  || !LODOCC_(cx, cz, cy+1)) *v++ = (struct vbufv){ 5,  DOWN, cx, cy, cz, 1,1,1,1, 0,0,0,0, 1 };
                if (!LODOCC_(cx, cz-1, cy)) *v++ = (struct vbufv){ 5, SOUTH, cx, cy, cz, 1,1,1,1, 0,0,0,0, 1 };
                if (!LODOCC_(cx, cz+1, cy)) *v++ = (struct vbufv){ 5, NORTH, cx, cy, cz, 1,1,1,1, 0,0,0,0, 1 };
                if (!LODOCC_(cx-1, cz, cy)) *v++ = (struct vbufv){ 5,  WEST, cx, cy, cz, 1,1,1,1, 0,0,0,0, 1 };
                if (!LODOCC_(cx+1, cz, cy)) *v++ = (struct vbufv){ 5,  EAST, cx, cy, cz, 1,1,1,1, 0,0,0,0, 1 };
        }

        return v;
}

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


                mesh_region(xlo, xhi, 0, TILESH, zlo, zhi, face_mask, xlo, zlo);

                LEAFSTART_(myx, myz) = mesh_leaf_start;
                WBOSTART_(myx, myz) = v - vbuf;

                if (w - wbuf < v_limit - v) // room for water in vertex buffer?
                {
                        memcpy(v, wbuf, (w - wbuf) * sizeof *wbuf);
                        v += w - wbuf;
                }

                VBOLEN_(myx, myz) = v - vbuf;

                v = lod_region(v, xlo, zlo);
                LODEND_(myx, myz) = v - vbuf;

                // world_buf is single-buffered and persistently mapped, but a
                // prior frame may still be reading this slot on the GPU. Wait for
                // the in-flight frames to drain before overwriting it in place,
                // otherwise the GPU tears the read and the tail of the chunk (the
                // +Z faces, written last) drops out for a frame. Only reached when
                // a chunk is actually remeshed (<= MAX_MESHES_PER_FRAME per frame).
                vkWaitForFences(vk.device, vk.maxFrames, vk.frontFences, VK_TRUE, UINT64_MAX);

                void *data = WMAPPED_(myx, myz);
                memcpy(data, vbuf, (v - vbuf) * sizeof *vbuf);

                // Mark chunk as clean after mesh rebuild and store LOD state
                DIRTY_(myx, myz) = 0;
                LIGHTDIRTY_(myx, myz) = 0;
                FACES_(myx, myz) = face_mask;
                LOD_(myx, myz) = use_lod;
                MESHGEN_SLOT(myx, myz).ax = myx - chunk_scootx; // the slot's mesh
                MESHGEN_SLOT(myx, myz).az = myz - chunk_scootz; // is now this chunk's
                meshes_built++;
                nr_meshes_built++;
        }
}

#endif // BLOCKO_MESH_C_INCLUDED

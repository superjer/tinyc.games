#include "blocko.c"
#ifndef BLOCKO_SIMAREA_C_INCLUDED
#define BLOCKO_SIMAREA_C_INCLUDED

// simarea.c - simulate around every player, not just the local window
//
// The server keeps a tiny terrain ring (struct warea, SIM_AREA_CHUNKS square)
// following each connected remote player, so mobs near them stand on real
// ground. Simulation reads the world through the sim_* accessors, never raw
// T_: main ring if the absolute chunk is stamp-resident, else the areas, else
// solid barrier - an entity outside all coverage stands on nothing rather
// than falling through the world. Generation is deterministic and edits write
// through to every copy, so overlapping copies stay byte-identical.
//
// Lifecycle runs on the main thread (sim_areas_update, once per frame);
// builders fill areas from chunker.c (area_builder_try) when the main ring
// has no work. Light never touches areas: light is render.

// defined later in the translation unit (net.c, edit.c)
int net_player_active(int i);
void net_player_anchor(int i, int *abx, int *abz);
void edit_apply_area_chunk(struct warea *a, int acx, int acz);

// area tile addressing by ABSOLUTE coords: slot = absolute masked, no scoot
#define AREA_T(a, ax, y, az)  (a)->tiles[((az) & (a)->maskd) * (a)->pitchz + ((ax) & (a)->maskw) * (a)->pitchx + (y)]
#define AREA_GNDH(a, ax, az)  (a)->gndh[ ((az) & (a)->maskd) * ((a)->maskw + 1) + ((ax) & (a)->maskw)]

// floored block-to-chunk for coords of any sign (B2C truncates toward zero)
#define B2CFLOOR(b) (((b) & ~(CHUNKW-1)) / CHUNKW)

// the active area holding a generated copy of absolute chunk (acx,acz), or
// NULL. Stamps outlive scoots until their slot is rewritten, so a just
// scrolled-out chunk keeps answering - that's valid data, bonus coverage
struct warea *sim_area_with_chunk(int acx, int acz)
{
        for (int i = 0; i < NR_PLAYERS; i++)
        {
                struct warea *a = &sim_area[i];
                if (!a->active) continue;
                volatile struct chunk_stamp *st =
                        &a->stamp[acz & SIM_AREA_CMASK][acx & SIM_AREA_CMASK];
                if (st->ax == acx && st->az == acz) return a;
        }
        return NULL;
}

int sim_tile(int wx, int y, int wz)
{
        int cx = B2CFLOOR(wx), cz = B2CFLOOR(wz);
        if (AGEN_(cx, cz))
                return T_(wx, y, wz);
        struct warea *a = sim_area_with_chunk(cx - chunk_scootx, cz - chunk_scootz);
        if (a)
                return AREA_T(a, wx - scootx, y, wz - scootz);
        return BARR;
}

int sim_gndh(int wx, int wz)
{
        int cx = B2CFLOOR(wx), cz = B2CFLOOR(wz);
        if (AGEN_(cx, cz))
                return GNDH_(wx, wz);
        struct warea *a = sim_area_with_chunk(cx - chunk_scootx, cz - chunk_scootz);
        if (a)
                return AREA_GNDH(a, wx - scootx, wz - scootz);
        return 0; // ground at the world top: no headroom, nothing spawns
}

// read a tile by ABSOLUTE coords, or -1 if no copy of that chunk is held
// anywhere (for inspection commands that must tell "air" from "not here")
int sim_tile_abs(int ax, int ay, int az)
{
        if (ay < 0 || ay >= TILESH) return -1;
        int wx = ax + scootx, wz = az + scootz;
        if (wx >= 0 && wx < TILESW && wz >= 0 && wz < TILESD
                        && AGEN_(B2C(wx), B2C(wz)))
                return T_(wx, ay, wz);
        struct warea *a = sim_area_with_chunk(B2CFLOOR(ax), B2CFLOOR(az));
        if (a)
                return AREA_T(a, ax, ay, az);
        return -1;
}

static void area_recalc_gndh(struct warea *a, int ax, int az)
{
        int y;
        for (y = 0; y < TILESH-1; y++)
                if (AREA_T(a, ax, y, az) != OPEN)
                        break;
        AREA_GNDH(a, ax, az) = y;
}

// write one tile into area a's copy (ABSOLUTE coords) and keep the area's
// gndheight consistent - the same ground rules as tile_light_update, minus
// the light (areas have none). Also the overlay-replay primitive (edit.c)
void sim_area_set(struct warea *a, int ax, int ay, int az, int t)
{
        int old = AREA_T(a, ax, ay, az);
        if (old == t) return;
        AREA_T(a, ax, ay, az) = t;
        if (t < LASTSOLID)
        {
                if (AREA_GNDH(a, ax, az) > ay)
                        AREA_GNDH(a, ax, az) = ay;
        }
        else if (old < LASTSOLID && AREA_GNDH(a, ax, az) == ay)
                area_recalc_gndh(a, ax, az);
}

// write an edit through to every area holding its chunk (ABSOLUTE coords).
// Ungenerated copies are skipped: the overlay replays into them when their
// chunk comes out of the builder. Called wherever the main ring gets edits
// (set_tile, edit_apply_remote); a no-op unless areas are active (server)
void sim_area_write(int ax, int ay, int az, int t)
{
        int acx = B2CFLOOR(ax), acz = B2CFLOOR(az);
        for (int i = 0; i < NR_PLAYERS; i++)
        {
                struct warea *a = &sim_area[i];
                if (!a->active) continue;
                #pragma omp critical (chunks)
                {
                        volatile struct chunk_stamp *st =
                                &a->stamp[acz & SIM_AREA_CMASK][acx & SIM_AREA_CMASK];
                        if (st->ax == acx && st->az == acz)
                                sim_area_set(a, ax, ay, az, t);
                }
        }
}

static void sim_area_alloc(struct warea *a)
{
        if (!area_sun_scratch)
                area_sun_scratch = malloc(SIM_AREA_W * SIM_AREA_W * TILESH);
        a->tiles = malloc(SIM_AREA_W * SIM_AREA_W * TILESH);
        a->sun = area_sun_scratch;
        a->gndh = malloc(SIM_AREA_W * SIM_AREA_W);
        a->maskw = SIM_AREA_W - 1;
        a->maskd = SIM_AREA_W - 1;
        a->pitchx = TILESH;
        a->pitchz = SIM_AREA_W * TILESH;
}

// main thread, once per frame: follow each remote player with an area, and
// replay the edit overlay onto area chunks fresh out of the builders
void sim_areas_update()
{
        for (int i = 0; i < NR_PLAYERS; i++)
        {
                struct warea *a = &sim_area[i];
                int on = net_mode == NET_SERVER && net_player_active(i);
                if (on)
                {
                        int abx, abz;
                        net_player_anchor(i, &abx, &abz);
                        // anchor chunk sits second from the low corner, so
                        // coverage is guaranteed +-CHUNKW blocks around it
                        int cx0 = B2CFLOOR(abx) - 1;
                        int cz0 = B2CFLOOR(abz) - 1;
                        if (!a->active)
                        {
                                if (!a->tiles)
                                        sim_area_alloc(a);
                                #pragma omp critical (chunks)
                                {
                                        // stale stamps lie: edits while
                                        // inactive never wrote through
                                        for (int z = 0; z < SIM_AREA_CHUNKS; z++)
                                        for (int x = 0; x < SIM_AREA_CHUNKS; x++)
                                        {
                                                a->stamp[z][x].ax = INT_MIN;
                                                a->stamp[z][x].az = INT_MIN;
                                                a->estamp[z][x].ax = INT_MIN;
                                                a->estamp[z][x].az = INT_MIN;
                                        }
                                        a->cx0 = cx0;
                                        a->cz0 = cz0;
                                        a->active = 1;
                                        a->epoch++;
                                }
                        }
                        else if (cx0 != a->cx0 || cz0 != a->cz0)
                        {
                                #pragma omp critical (chunks)
                                {
                                        a->cx0 = cx0;
                                        a->cz0 = cz0;
                                        a->epoch++; // mid-job window mappings went stale
                                }
                        }
                }
                else if (a->active)
                {
                        #pragma omp critical (chunks)
                        {
                                a->active = 0;
                                a->epoch++;
                        }
                }
        }

        // adopt fresh area chunks: replay recorded edits into them
        struct area_fresh fresh[sizeof area_fresh / sizeof *area_fresh];
        int n;
        #pragma omp critical (chunks)
        {
                n = area_fresh_len;
                for (int i = 0; i < n; i++)
                        fresh[i] = area_fresh[i];
                area_fresh_len = 0;
        }
        for (int i = 0; i < n; i++)
                edit_apply_area_chunk(fresh[i].a, fresh[i].acx, fresh[i].acz);
}

#endif // BLOCKO_SIMAREA_C_INCLUDED

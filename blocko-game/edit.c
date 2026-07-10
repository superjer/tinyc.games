#include "blocko.c"
#ifndef BLOCKO_EDIT_C_INCLUDED
#define BLOCKO_EDIT_C_INCLUDED

// edit.c - the one way to change a block, and the overlay that remembers it
//
// Every block edit goes through set_tile, which writes the tile, records the
// edit in an overlay hash table keyed by ABSOLUTE tile coords, and handles the
// ground-height/lighting/patch consequences. Freshly (re)generated chunks
// replay their overlay entries (edit_apply_chunk, called from draw.c as chunks
// come out of the builders), so edits now survive scooting out of the window
// and back - terrain regenerates from the seed, then the overlay reapplies
// what the player changed. seed + overlay = the whole world, which is also
// the future save/network story.
//
// Main thread only: edits are recorded during update_player and replayed in
// draw, both on the main thread. Builder threads never touch the overlay, so
// no locking.

static struct edit { int x, y, z, used; unsigned char tile; } *edit_tab;
static int edit_cap; // power of 2; 0 until the first edit
int edit_len;

static unsigned edit_hash(int x, int y, int z)
{
        unsigned h = 2166136261u;
        h = (h ^ (unsigned)x) * 16777619u;
        h = (h ^ (unsigned)y) * 16777619u;
        h = (h ^ (unsigned)z) * 16777619u;
        return h;
}

static struct edit *edit_slot(struct edit *tab, int cap, int x, int y, int z)
{
        unsigned i = edit_hash(x, y, z) & (cap - 1);
        while (tab[i].used && (tab[i].x != x || tab[i].y != y || tab[i].z != z))
                i = (i + 1) & (cap - 1);
        return &tab[i];
}

static void edit_grow()
{
        int ncap = edit_cap ? edit_cap * 2 : 1024;
        struct edit *ntab = calloc(ncap, sizeof *ntab);
        for (int i = 0; i < edit_cap; i++)
        {
                if (!edit_tab[i].used) continue;
                *edit_slot(ntab, ncap, edit_tab[i].x, edit_tab[i].y, edit_tab[i].z)
                        = edit_tab[i];
        }
        free(edit_tab);
        edit_tab = ntab;
        edit_cap = ncap;
}

// remember an edit at ABSOLUTE tile coords; a newer edit to the same cell
// replaces the older one, so the overlay holds only each cell's final value
void edit_record(int x, int y, int z, int tile)
{
        if (edit_len * 3 >= edit_cap * 2)
                edit_grow();
        struct edit *e = edit_slot(edit_tab, edit_cap, x, y, z);
        if (!e->used)
        {
                e->x = x; e->y = y; e->z = z;
                e->used = 1;
                edit_len++;
        }
        e->tile = tile;
}

void edit_clear()
{
        free(edit_tab);
        edit_tab = NULL;
        edit_cap = 0;
        edit_len = 0;
}

// walk the overlay: start with *it = 0; returns 1 and fills the entry while
// entries remain. Used to stream the whole overlay to a joining client.
int edit_next(int *it, int *x, int *y, int *z, int *tile)
{
        while (edit_tab && *it < edit_cap)
        {
                struct edit *e = &edit_tab[(*it)++];
                if (!e->used) continue;
                *x = e->x; *y = e->y; *z = e->z; *tile = e->tile;
                return 1;
        }
        return 0;
}

// ground-height and lighting consequences of one tile change (window coords).
// shared by live edits (set_tile) and overlay replay (edit_apply_chunk).
// light is a pure function of the ground line, so only an edit that moves it
// changes anything - and then only the four corner columns touching it
static void tile_light_update(int x, int y, int z, int old, int t)
{
        if (t < LASTSOLID) // now solid: maybe a new highest block
        {
                if (!ABOVE_GROUND(x, y, z))
                        return;
                GNDH_(x, z) = y;
        }
        else if (old < LASTSOLID) // was solid: maybe broke the ground
        {
                if (!AT_GROUND(x, y, z))
                        return;
                recalc_gndheight(x, z);
        }
        else
                return; // open <-> open (e.g. water/air): ground line unmoved

        recalc_corners_at(x, z);
}

// change one block (window tile coords): write the tile, record it in the
// overlay, update ground height + lighting, and show the edit instantly via
// the reject+patch (patch.c). Gameplay effects (item drops, cooldowns, hand
// swings) stay with the callers.
void set_tile(int x, int y, int z, int t)
{
        int old = T_(x, y, z);
        if (old == t)
                return;

        T_(x, y, z) = t;
        edit_record(x - scootx, y, z - scootz, t);
        sim_area_write(x - scootx, y, z - scootz, t);
        tile_light_update(x, y, z, old, t);
        patch_edit(x, y, z);
        net_send_edit(x - scootx, y, z - scootz, t);
}

// land an edit that arrived from the network: record it, and if its chunk is
// already generated, apply it in place like a local edit (minus re-sending -
// the server relays for us). An ungenerated chunk needs only the record; the
// replay at generation time picks it up.
void edit_apply_remote(int ax, int ay, int az, int tile)
{
        edit_record(ax, ay, az, tile);
        sim_area_write(ax, ay, az, tile);

        int x = ax + scootx, z = az + scootz;
        if (x < 0 || x >= TILESW || z < 0 || z >= TILESD) return;
        if (!AGEN_(B2C(x), B2C(z))) return;

        int old = T_(x, ay, z);
        if (old == tile) return;
        T_(x, ay, z) = tile;
        tile_light_update(x, ay, z, old, tile);
        patch_edit(x, ay, z);
}

// replay the overlay onto a freshly generated chunk (ABSOLUTE chunk coords).
// The chunk is already marked dirty for meshing, so no patch: just the tiles
// and their light consequences
void edit_apply_chunk(int acx, int acz)
{
        if (!edit_len)
                return;

        for (int i = 0; i < edit_cap; i++)
        {
                struct edit *e = &edit_tab[i];
                if (!e->used) continue;
                if ((e->x & ~(CHUNKW-1)) != acx * CHUNKW) continue;
                if ((e->z & ~(CHUNKD-1)) != acz * CHUNKD) continue;

                int x = e->x + scootx, z = e->z + scootz; // absolute -> window
                if (x < 0 || x >= TILESW || z < 0 || z >= TILESD) continue;

                int old = T_(x, e->y, z);
                if (old == e->tile) continue;
                T_(x, e->y, z) = e->tile;
                tile_light_update(x, e->y, z, old, e->tile);
        }
}

// replay the overlay onto a freshly generated sim-area chunk (ABSOLUTE chunk
// coords): tiles and the area's gndheight only - light is render
void edit_apply_area_chunk(struct warea *a, int acx, int acz)
{
        if (!edit_len)
                return;

        for (int i = 0; i < edit_cap; i++)
        {
                struct edit *e = &edit_tab[i];
                if (!e->used) continue;
                if ((e->x & ~(CHUNKW-1)) != acx * CHUNKW) continue;
                if ((e->z & ~(CHUNKD-1)) != acz * CHUNKD) continue;
                sim_area_set(a, e->x, e->y, e->z, e->tile);
        }
}

#endif // BLOCKO_EDIT_C_INCLUDED

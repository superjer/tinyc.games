#ifndef BLOCKO_PMODEL_C_INCLUDED
#define BLOCKO_PMODEL_C_INCLUDED
#include "blocko.c"

// Player models: up to 20 rectangular prisms, each 1-16 px per axis, living in
// a 16x16x16 px local space. A piece parents to another piece or to the player
// center box (a 16^3 space centered on the physical hitbox); its origin point
// is pinned to the attach point in the parent's space and it rotates there.
// The whole definition - geometry, 16-color palette, and 16x16 face tiles in
// a fixed box unwrap (4-bit indices, two texels per byte) - is one flat
// integer struct (defs.c) that travels the net as raw bytes: each instance
// randomizes its own model at startup and peers exchange them via MSG_PMODEL
// (net.c).
//
// Face tiles ride as extra layers of the terrain texture array (16x16 RGBA,
// expanded from the palette), one 120-layer range per player slot, so
// pmodel.vert can share main.frag: lighting, fog, near-shadow sampling and
// the a<0.5 discard all come free. A model arriving at runtime re-uploads
// just its slot's layer range (update_texture_layers).

// world units per model px, chosen so the default head center (28 px above the
// feet: 12 leg + 12 body + half of the 8 head) sits exactly at the camera eye
#define PM_SCALE ((PLYR_H - EYEDOWN) / 28.f)

// default model: head, body, 2 arms, 2 legs, Minecraft-ish proportions.
// local spaces are y-down like the world; the body carries everything and
// hangs from the center box such that the feet land exactly on the ground
// (the box anchor is snapped to the px grid in pm_resolve to keep this integer)
enum { PM_BODY, PM_HEAD, PM_ARM_L, PM_ARM_R, PM_LEG_L, PM_LEG_R };
static struct pmodel pm_default = {
        .nr_pieces = 6,
        .piece = {
        //        dims       corner    origin     attach    parent   type
        [PM_BODY]  = { {8,12,4}, {4,2,6}, {8,8,8},  {8,5,8},   -1,      PM_T_TORSO },
        [PM_HEAD]  = { {8,8,8},  {4,4,4}, {8,12,8}, {8,2,8},   PM_BODY, PM_T_HEAD },
        [PM_ARM_L] = { {4,12,4}, {6,2,6}, {8,3,8},  {2,3,8},   PM_BODY, PM_T_ARM_L },
        [PM_ARM_R] = { {4,12,4}, {6,2,6}, {8,3,8},  {14,3,8},  PM_BODY, PM_T_ARM_R },
        [PM_LEG_L] = { {4,12,4}, {6,2,6}, {8,2,8},  {6,14,8},  PM_BODY, PM_T_LEG1 },
        [PM_LEG_R] = { {4,12,4}, {6,2,6}, {8,2,8},  {10,14,8}, PM_BODY, PM_T_LEG2 },
        },
};

// how many px each face spans in u and v, by orient code (1..6)
static void pm_face_extent(struct pm_piece *p, int orient, int *eu, int *ev)
{
        switch (orient) {
        case UP:   case DOWN:  *eu = p->dims[0]; *ev = p->dims[2]; break;
        case EAST: case WEST:  *eu = p->dims[2]; *ev = p->dims[1]; break;
        default:               *eu = p->dims[0]; *ev = p->dims[1]; break; // N/S
        }
}

// which texel axis spans world axis a on each face, signed by direction
// (pmodel.vert mirrors u on UP/NORTH/WEST so paint reads unflipped).
// 0 = that's the face's normal axis; +-1 = u, +-2 = v.
static const signed char pm_face_uvmap[PM_FACES][3] = {
        [UP - 1]    = { -1,  0, +2 },
        [EAST - 1]  = {  0, +2, +1 },
        [NORTH - 1] = { -1, +2,  0 },
        [WEST - 1]  = {  0, +2, -1 },
        [SOUTH - 1] = { +1, +2,  0 },
        [DOWN - 1]  = { +1,  0, +2 },
};

// insert or delete one row/column of texels at one end of a face's visible
// window, so paint follows a resize. Faces sample texels [0..eu)x[0..ev)
// anchored at (0,0), so an edit at the index-0 end shifts the window and an
// edit at the far end appends/drops in place. t = the face's 16x16 tile;
// along_u: the edit runs along u (else v); e = the extent along the edited
// axis BEFORE the resize. The edge row rides along with the moving face: a
// grow duplicates the row just behind it into the gap, a shrink destroys
// that row and keeps the edge. Under 3 rows there is no "behind", so the
// edge row itself gets duplicated (grow) or lost (shrink).
static void pm_texel_resize(unsigned char *t, int along_u, int at_zero,
                int grow, int e)
{
        int stride = along_u ? 1 : PM_TILE;
        int deep = e >= 3;
        for (int w = 0; w < PM_TILE; w++)
        {
                int r = along_u ? w * PM_TILE : w;
                if (grow && at_zero)
                        for (int i = e; i >= 1 + deep; i--)
                                PM_TEXSET(t, r + i * stride,
                                        PM_TEXGET(t, r + (i - 1) * stride));
                else if (grow)
                {
                        PM_TEXSET(t, r + e * stride,
                                PM_TEXGET(t, r + (e - 1) * stride));
                        if (deep) PM_TEXSET(t, r + (e - 1) * stride,
                                PM_TEXGET(t, r + (e - 2) * stride));
                }
                else if (at_zero)
                        for (int i = deep; i < e - 1; i++)
                                PM_TEXSET(t, r + i * stride,
                                        PM_TEXGET(t, r + (i + 1) * stride));
                else if (deep)
                        PM_TEXSET(t, r + (e - 2) * stride,
                                PM_TEXGET(t, r + (e - 1) * stride));
        }
}

// grow or shrink piece pi's prism 1px at the given face (orient code), so
// the new layer looks purely added: every other face, the joint and the
// children hanging elsewhere stay visually put, while children whose attach
// sits on or past the moved face's plane RIDE it - the arm socketed off the
// east side follows the east face out and back in. The four lateral faces'
// texel windows gain or lose a row at the moved end (pm_texel_resize); when
// the growing side is already at the 16^3 wall the content instead slides
// within the space, and origin + the non-riding children's attach shift
// along, each clamping at its own wall (a joint can't sit more than 16px
// from any prism wall). Returns whether anything changed.
static int pm_piece_resize(struct pmodel *mo, int pi, int face, int grow)
{
        // face orient code -> world axis, and which side of it moved
        static const signed char axis_of[7] = { 0, 1, 0, 2, 0, 2, 1 };
        static const signed char side_of[7] = { 0, -1, 1, 1, -1, -1, 1 };
        struct pm_piece *p = &mo->piece[pi];
        int a = axis_of[face], side = side_of[face];
        int e = p->dims[a]; // extent along the axis before the change
        // the moved face's plane before the change, for spotting the riders
        int plane = side > 0 ? p->corner[a] + e : p->corner[a];
        int at_wall = 0;

        if (grow)
        {
                if (e >= PM_TILE) return 0;
                at_wall = side > 0 ? p->corner[a] + e >= PM_TILE
                                   : p->corner[a] <= 0;
                p->dims[a]++;
                if (side > 0 ? at_wall : !at_wall) p->corner[a]--;
                if (at_wall) // content slides off the growing face
                        p->origin[a] = ICLAMP(p->origin[a] - side, 0, PM_TILE);
        }
        else
        {
                if (e <= 1) return 0;
                p->dims[a]--;
                if (side < 0) p->corner[a]++;
        }

        // at the wall the plane stays put in the local space and the content
        // slides -side under it, so NON-riders shift along to stay visually
        // put (the riders' world spot moves with the origin, out with the
        // face); everywhere else the plane itself moves and only the riders
        // follow it
        for (int j = 0; j < mo->nr_pieces; j++)
        {
                if (mo->piece[j].parent != pi) continue;
                int rides = side > 0 ? mo->piece[j].attach[a] >= plane
                                     : mo->piece[j].attach[a] <= plane;
                int d = grow && at_wall ? (rides ? 0 : -side)
                                        : (rides ? (grow ? side : -side) : 0);
                if (!d) continue;
                int at = mo->piece[j].attach[a] + d;
                mo->piece[j].attach[a] = ICLAMP(at, 0, 2 * PM_TILE);
        }

        for (int f = 0; f < PM_FACES; f++)
        {
                int m = pm_face_uvmap[f][a];
                if (!m) continue;
                pm_texel_resize(mo->texel[pi][f], abs(m) == 1,
                                (side > 0) != (m > 0), grow, e);
        }
        return 1;
}

// delete piece pi and its whole subtree, compacting the piece and texel
// arrays and remapping the survivors' parent links (a survivor's parent is
// always a survivor: a piece whose parent died would have died with it).
// Refuses to empty the model. Returns the number of pieces removed.
static int pm_piece_delete(struct pmodel *mo, int pi)
{
        int nr = mo->nr_pieces;
        char kill[PM_MAX_PIECES] = {0};
        kill[pi] = 1;
        for (int pass = 0; pass < nr; pass++)
                for (int i = 0; i < nr; i++)
                        if (!kill[i] && mo->piece[i].parent >= 0
                                        && kill[(int)mo->piece[i].parent])
                                kill[i] = 1;
        int left = 0;
        for (int i = 0; i < nr; i++) left += !kill[i];
        if (!left) return 0;

        signed char map[PM_MAX_PIECES];
        int n = 0;
        for (int i = 0; i < nr; i++)
        {
                map[i] = kill[i] ? -1 : n;
                if (kill[i]) continue;
                if (n != i)
                {
                        mo->piece[n] = mo->piece[i];
                        memcpy(mo->texel[n], mo->texel[i], sizeof mo->texel[n]);
                }
                n++;
        }
        mo->nr_pieces = n;
        for (int i = 0; i < n; i++)
                if (mo->piece[i].parent >= 0)
                        mo->piece[i].parent = map[(int)mo->piece[i].parent];
        return nr - n;
}

#define PM_RGB(r, g, b) (0xff000000u | ((b) << 16) | ((g) << 8) | (r))

// the 16 palette slots: 0 transparent, three random base/dark/accent trios
// at 1..9 the pieces share round-robin, white/black starters at 10..11, and
// editor starters at 12..15 - the NEW PART checkerboard grays and the
// classic paint red/blue. Only slot 0 is special: every other slot is the
// user's to recolor in the editor's palette panel, so the starters are
// stamped only on a fresh random coat, never on an existing model
#define PMEDIT_GRAY_A 12
#define PMEDIT_GRAY_B 13
#define PMEDIT_RED    14
#define PMEDIT_BLUE   15

static void pm_starter_colors(struct pmodel *mo)
{
        mo->palette[PMEDIT_GRAY_A] = PM_RGB(105, 105, 105);
        mo->palette[PMEDIT_GRAY_B] = PM_RGB(165, 165, 165);
        mo->palette[PMEDIT_RED]    = PM_RGB(220, 40, 40);
        mo->palette[PMEDIT_BLUE]   = PM_RGB(45, 80, 230);
}

// paint a model's tiles: fill the whole palette, then a random pattern per
// piece in its trio's colors, with a darker border around the used region of
// each face so articulation and UV extents stay visible. Deterministic in
// the seed; the painted texels are what travels the net, not the seed.
static void pm_paint(struct pmodel *mo, unsigned seed)
{
        mo->palette[0] = 0; // transparent
        mo->palette[10] = PM_RGB(230, 230, 230);
        mo->palette[11] = PM_RGB(25, 25, 25);
        pm_starter_colors(mo);
        for (int t = 0; t < 3; t++)
        {
                int base = 1 + t * 3;
                int r = 64 + dumb_rand(&seed) % 192;
                int g = 64 + dumb_rand(&seed) % 192;
                int b = 64 + dumb_rand(&seed) % 192;
                mo->palette[base]     = PM_RGB(r, g, b);
                mo->palette[base + 1] = PM_RGB(r * 2 / 3, g * 2 / 3, b * 2 / 3);
                mo->palette[base + 2] = PM_RGB(64 + dumb_rand(&seed) % 192,
                                               64 + dumb_rand(&seed) % 192,
                                               64 + dumb_rand(&seed) % 192);
        }
        for (int i = 0; i < mo->nr_pieces; i++)
        {
                int base = 1 + i % 3 * 3, dark = base + 1, accent = base + 2;
                int pattern = dumb_rand(&seed) % 4;

                for (int f = 0; f < PM_FACES; f++)
                {
                        int eu, ev;
                        pm_face_extent(&mo->piece[i], f + 1, &eu, &ev);
                        for (int v = 0; v < PM_TILE; v++)
                        for (int u = 0; u < PM_TILE; u++)
                        {
                                int c = base;
                                switch (pattern) {
                                case 1: c = ((u / 2 + v / 2) & 1) ? accent : base; break;
                                case 2: c = (v / 2 & 1) ? accent : base; break;
                                case 3: c = (dumb_rand(&seed) % 5 == 0) ? accent : base; break;
                                }
                                if (u < eu && v < ev &&
                                    (u == 0 || v == 0 || u == eu - 1 || v == ev - 1))
                                        c = dark;
                                PM_TEXSET(mo->texel[i][f], v * PM_TILE + u, c);
                        }
                }
        }
}

// an even random size in [lo, hi] so halved dims stay on the integer px grid
static int pm_even(unsigned *seed, int lo, int hi)
{
        return lo + 2 * (dumb_rand(seed) % ((hi - lo) / 2 + 1));
}

// random model: the default 6-piece skeleton with randomized prism sizes, the
// connection points re-derived so limbs stay attached and feet stay on the
// ground. (Head height varies, so only the default model's head sits exactly
// at the camera eye.) Then random colors and patterns on top.
static void pmodel_randomize(struct pmodel *mo, unsigned seed)
{
        *mo = pm_default;
        struct pm_piece *pc = mo->piece;

        int hw = pm_even(&seed, 6, 12), hh = pm_even(&seed, 6, 12), hd = pm_even(&seed, 6, 12);
        int bw = pm_even(&seed, 6, 10), bh = pm_even(&seed, 8, 14), bd = pm_even(&seed, 2, 6);
        int aw = pm_even(&seed, 2, 4),  ah = pm_even(&seed, 8, 14);
        int lw = pm_even(&seed, 2, 4),  lh = pm_even(&seed, 8, 14);

        // body hangs from the center box so the legs' feet land on the ground:
        // box px p is 8 + K - p px above the feet, K = the box anchor height
        int K = (int)roundf((PLYR_H / 2) / PM_SCALE);
        pc[PM_BODY] = (struct pm_piece){ {bw,bh,bd}, {8-bw/2, 8-bh/2, 8-bd/2},
                {8,8,8}, {8, 8 + K - lh - bh/2, 8}, -1, PM_T_TORSO };
        pc[PM_HEAD] = (struct pm_piece){ {hw,hh,hd}, {8-hw/2, 8-hh/2, 8-hd/2},
                {8, 8+hh/2, 8}, {8, 8-bh/2, 8}, PM_BODY, PM_T_HEAD };
        pc[PM_ARM_L] = (struct pm_piece){ {aw,ah,aw}, {8-aw/2, 8-ah/2, 8-aw/2},
                {8, 8-ah/2+1, 8}, {8-bw/2-aw/2, 8-bh/2+1, 8}, PM_BODY, PM_T_ARM_L };
        pc[PM_ARM_R] = pc[PM_ARM_L];
        pc[PM_ARM_R].attach[0] = 8 + bw/2 + aw/2;
        pc[PM_ARM_R].type = PM_T_ARM_R;
        pc[PM_LEG_L] = (struct pm_piece){ {lw,lh,lw}, {8-lw/2, 8-lh/2, 8-lw/2},
                {8, 8-lh/2, 8}, {8-lw/2, 8+bh/2, 8}, PM_BODY, PM_T_LEG1 };
        pc[PM_LEG_R] = pc[PM_LEG_L];
        pc[PM_LEG_R].attach[0] = 8 + lw/2;
        pc[PM_LEG_R].type = PM_T_LEG2;

        pm_paint(mo, dumb_rand(&seed));
}

// force any model's bytes into a valid shape (net arrivals, disk loads)
static void pm_sanitize(struct pmodel *mo)
{
        if (mo->nr_pieces > PM_MAX_PIECES) mo->nr_pieces = PM_MAX_PIECES;
        if (!mo->nr_pieces) { *mo = pm_default; }
        if (mo->style > PM_STYLE_FLAIL) mo->style = PM_STYLE_WALK;
        for (int i = 0; i < mo->nr_pieces; i++)
        {
                struct pm_piece *p = &mo->piece[i];
                for (int a = 0; a < 3; a++)
                {
                        p->dims[a] = ICLAMP(p->dims[a], 1, PM_TILE);
                        p->corner[a] = ICLAMP(p->corner[a], 0, PM_TILE - p->dims[a]);
                        p->origin[a] = ICLAMP(p->origin[a], 0, PM_TILE);
                        p->attach[a] = ICLAMP(p->attach[a], 0, PM_TILE);
                        p->rest[a] = ICLAMP(p->rest[a], -25, 25);
                }
                if (p->parent < 0 || p->parent >= mo->nr_pieces || p->parent == i)
                        p->parent = -1;
                if (p->type >= PM_T_COUNT) p->type = PM_T_FIXED;
        }

        // parents may point at ANY other piece (the editor rewires them), so
        // cut cycles: anything that can't reach the center box in nr_pieces
        // hops gets re-hung from it
        for (int i = 0; i < mo->nr_pieces; i++)
        {
                int j = i, hops = 0;
                while (j >= 0 && hops++ < mo->nr_pieces)
                        j = mo->piece[j].parent;
                if (j >= 0) mo->piece[i].parent = -1;
        }
}

static unsigned pm_checksum(struct pmodel *mo) // debug: FNV-1a over the struct
{
        unsigned h = 2166136261u;
        const unsigned char *p = (const unsigned char *)mo;
        for (size_t i = 0; i < sizeof *mo; i++) { h ^= p[i]; h *= 16777619u; }
        return h;
}

// ---- texture array layers: one 120-layer range per player slot -------------

static int pm_slot_layers() { return PM_MAX_PIECES * PM_FACES; }

static void pm_expand(struct pmodel *mo, unsigned char *rgba)
{
        unsigned *out = (unsigned *)rgba;
        for (int i = 0; i < PM_MAX_PIECES; i++)
                for (int f = 0; f < PM_FACES; f++)
                        for (int t = 0; t < PM_TILE * PM_TILE; t++)
                                *out++ = mo->palette[PM_TEXGET(mo->texel[i][f], t)];
}

// my model persists as numbered snapshots, one per editor session that
// changed something: 00001.model, 00002.model, ... The newest (highest
// number) loads at startup; a fresh install starts from the default asset.
// Every file is the exact MSG_PMODEL packet the model travels the net as:
// [u8 owner id][raw struct pmodel] - the id byte is ignored on load
#define PM_HIST_DIR "save-data/blocko/player-models"
#define PM_HIST_FMT PM_HIST_DIR "/%05d.model"
#define PM_DEFAULT_FILE "blocko-game/assets/models/player-default.model"

// the highest snapshot number on disk, 0 if none
static int pm_hist_newest()
{
        int newest = 0, count = 0;
        char **names = SDL_GlobDirectory(PM_HIST_DIR, "*.model", 0, &count);
        for (int i = 0; names && i < count; i++)
        {
                char *end;
                long n = strtol(names[i], &end, 10);
                if (n > newest && !strcmp(end, ".model")) newest = n;
        }
        SDL_free(names);
        return newest;
}

static void pm_hist_write(int n)
{
        SDL_CreateDirectory(PM_HIST_DIR); // missing parents too
        char path[64];
        sprintf(path, PM_HIST_FMT, n);
        FILE *f = fopen(path, "wb");
        if (!f) { fprintf(stderr, "pmodel: can't write %s\n", path); return; }
        unsigned char id = my_player;
        fwrite(&id, 1, 1, f);
        fwrite(&pm_models[my_player], sizeof(struct pmodel), 1, f);
        fclose(f);
}

static int pmodel_load(struct pmodel *mo, const char *path)
{
        FILE *f = fopen(path, "rb");
        if (!f) return 0;
        // the +2 makes an overlong file read as "too big" instead of
        // exactly matching the expected size
        static unsigned char buf[sizeof(struct pmodel) + 3];
        size_t n = fread(buf, 1, sizeof buf, f);
        fclose(f);
        if (n != 1 + sizeof *mo)
        {
                fprintf(stderr, "pmodel: %s wrong size, ignoring\n", path);
                return 0;
        }
        memcpy(mo, buf + 1, sizeof *mo);
        pm_sanitize(mo);
        return 1;
}

// roll this instance's model. Runs from main BEFORE any networking: the join
// handshake blocks pre-vksetup, so models must exist (and mine must be
// randomized) by the time WELCOME triggers the MSG_PMODEL exchange
void pmodel_init()
{
        pm_paint(&pm_default, 12345); // a fixed, recognizable default coat
        for (int i = 0; i < NR_PLAYERS; i++)
                pm_models[i] = pm_default;
        char path[64] = PM_DEFAULT_FILE;
        int n = pm_hist_newest(), ok = 0;
        if (n)
        {
                sprintf(path, PM_HIST_FMT, n);
                ok = pmodel_load(&pm_models[my_player], path);
        }
        if (!ok)
        {
                strcpy(path, PM_DEFAULT_FILE);
                ok = pmodel_load(&pm_models[my_player], path);
        }
        if (ok)
                fprintf(stderr, "pmodel: loaded %s, checksum %08x\n", path,
                        pm_checksum(&pm_models[my_player]));
        else
        {
                unsigned seed = (unsigned)SDL_GetPerformanceCounter();
                pmodel_randomize(&pm_models[my_player], seed);
                fprintf(stderr, "pmodel: my model seed %u checksum %08x\n",
                        seed, pm_checksum(&pm_models[my_player]));
        }
        pmodel_have[my_player] = 1;
}

// solid-color layers appended after every slot's tiles, for the model editor:
// white (the selection outline), six cycling hues (the joint gizmo), then
// dark green (the ground quad) and bright pink (the parent highlight)
#define PM_LAYER_WHITE (pmodel_tex_base + NR_PLAYERS * pm_slot_layers())
#define PM_LAYER_HUES  (PM_LAYER_WHITE + 1)
#define PM_NR_HUES 6
#define PM_LAYER_DKGREEN (PM_LAYER_HUES + PM_NR_HUES)
#define PM_LAYER_PINK (PM_LAYER_DKGREEN + 1)

// model-picker thumbnails: PM_NR_PREVIEW more slots (each a full model's worth
// of face-tile layers) after the editor solids, filled on demand from disk when
// the picker opens or pages. One 3x2 grid page - the layer budget (2048 min)
// only stretches to a handful past the 8 player slots
#define PM_NR_PREVIEW 6
#define PM_LAYER_PREVIEW (PM_LAYER_PINK + 1)

// all slots' face tiles for the texture array, plus the editor's solid-color
// layers at the end. By vksetup time the join handshake may already have
// filled pm_models with remote players' real models (their pre-texture
// uploads were skipped), so expand, don't reset
unsigned char *pmodel_make_tiles(int *nr_layers)
{
        static unsigned char rgba[(NR_PLAYERS * PM_MAX_PIECES * PM_FACES + 3 + PM_NR_HUES
                        + PM_NR_PREVIEW * PM_MAX_PIECES * PM_FACES)
                * PM_TILE * PM_TILE * 4];
        for (int i = 0; i < NR_PLAYERS; i++)
                pm_expand(&pm_models[i], rgba + i * pm_slot_layers() * PM_TILE * PM_TILE * 4);

        unsigned *solid = (unsigned *)(rgba
                + NR_PLAYERS * pm_slot_layers() * PM_TILE * PM_TILE * 4);
        unsigned colors[3 + PM_NR_HUES] = { PM_RGB(255, 255, 255),
                PM_RGB(255, 60, 60), PM_RGB(255, 230, 60), PM_RGB(60, 255, 60),
                PM_RGB(60, 230, 255), PM_RGB(60, 60, 255), PM_RGB(255, 60, 255),
                PM_RGB(25, 95, 35), PM_RGB(255, 70, 160) };
        for (int i = 0; i < 3 + PM_NR_HUES; i++)
                for (int t = 0; t < PM_TILE * PM_TILE; t++)
                        *solid++ = colors[i];

        *nr_layers = NR_PLAYERS * pm_slot_layers() + 3 + PM_NR_HUES
                        + PM_NR_PREVIEW * pm_slot_layers();
        return rgba;
}

// re-upload one slot's layer range after its model changed at runtime
static void pmodel_upload(int slot)
{
        static unsigned char rgba[PM_MAX_PIECES * PM_FACES * PM_TILE * PM_TILE * 4];
        pm_expand(&pm_models[slot], rgba);
        update_texture_layers(pmodel_tex_base + slot * pm_slot_layers(), pm_slot_layers(), rgba);
}

// the model picker's whole page, in one shot: the preview slots are contiguous
// and update_texture_layers idles the device, so batch all PM_NR_PREVIEW rather
// than uploading per cell. loaded[c] == 0 leaves that slot transparent (an empty
// cell on the last page), so the previous page never bleeds through
static void pmodel_upload_previews(struct pmodel *models, int *loaded)
{
        static unsigned char rgba[PM_NR_PREVIEW * PM_MAX_PIECES * PM_FACES
                        * PM_TILE * PM_TILE * 4];
        int span = pm_slot_layers() * PM_TILE * PM_TILE * 4;
        memset(rgba, 0, sizeof rgba);
        for (int c = 0; c < PM_NR_PREVIEW; c++)
                if (loaded[c]) pm_expand(&models[c], rgba + c * span);
        update_texture_layers(PM_LAYER_PREVIEW, PM_NR_PREVIEW * pm_slot_layers(), rgba);
}

// a model arrived over the net: sanitize it (remote bytes!), store, upload
void pmodel_net_recv(int slot, const unsigned char *data, int len)
{
        if (slot < 0 || slot >= NR_PLAYERS || slot == my_player) return;
        if (len < (int)sizeof(struct pmodel)) return;
        struct pmodel *mo = &pm_models[slot];
        memcpy(mo, data, sizeof *mo);
        pm_sanitize(mo);

        pmodel_have[slot] = 1;
        pmodel_upload(slot);
        fprintf(stderr, "pmodel: got player %d's model, checksum %08x\n",
                slot, pm_checksum(mo));
}

// the client's slot changed at WELCOME: carry the local model to its new home
void pmodel_local_moved(int old_slot)
{
        if (old_slot == my_player) return;
        pm_models[my_player] = pm_models[old_slot];
        pm_models[old_slot] = pm_default;
        pmodel_have[my_player] = 1;
        pmodel_have[old_slot] = 0;
        pmodel_upload(my_player);
}

// ---- transforms: column-major mat4s, storage as in vector.c ----

static void pm_mat_ident(float *m)
{
        memset(m, 0, 16 * sizeof *m);
        m[0] = m[5] = m[10] = m[15] = 1;
}

static void pm_mat_translate(float *m, float x, float y, float z)
{
        pm_mat_ident(m);
        m[12] = x; m[13] = y; m[14] = z;
}

// yaw about y, spinning x toward z like mob.vert's heading spin
static void pm_mat_yaw(float *m, float a)
{
        pm_mat_ident(m);
        float s = sinf(a), c = cosf(a);
        m[0] = c; m[2] = s; m[8] = -s; m[10] = c;
}

static void pm_mat_roll(float *m, float a) // around z: lateral raises
{
        float c = cosf(a), s = sinf(a);
        pm_mat_ident(m);
        m[0] = c; m[1] = s; m[4] = -s; m[5] = c;
}

static void pm_mat_pitch(float *m, float a)
{
        pm_mat_ident(m);
        float s = sinf(a), c = cosf(a);
        m[5] = c; m[6] = s; m[9] = -s; m[10] = c;
}

// whether pm_resolve applies the pieces' resting angles. The world emit
// (pm_emit) always poses; the editor poses only in RESTING ANGLE mode and
// ANIMATE - its geometry modes (DETACH's offset folding, flush placement,
// RESIZE face picking, the JOINT gizmo) assume the standing pose is pure
// translation and must resolve unposed
static int pm_rest_apply;

// resolve the model's piece transforms (px in each piece's 16^3 space -> world)
// for a player whose hitbox top corner is at (x,y,z), facing model-yaw `yaw`.
// an drives the articulation; NULL means a plain standing pose (the model
// editor's preview). Motion dispatches on each piece's TYPE; the FLAIL
// style waves the limbs and head around instead, everything else still
// animating as in WALK.
// root_out, when non-NULL, gets the center box's own px -> world matrix
// (the editor needs it: a parent of -1 attaches into that space).
static void pm_resolve(struct pmodel *mo, float x, float y, float z, float yaw,
                struct pm_anim *an, float space[][16], float geom[][16], float *root_out)
{
        float tmp[16], tmp2[16], rot[16];

        // the center box is centered on the hitbox, but its anchor snaps to the
        // px grid counted from the feet so integer attach points can put the
        // feet exactly on the ground AND the head center exactly at eye height
        float feet = y + PLYR_H;
        float anchor = feet - roundf((PLYR_H / 2) / PM_SCALE) * PM_SCALE;
        float root[16] = {
                PM_SCALE, 0, 0, 0,
                0, PM_SCALE, 0, 0,
                0, 0, PM_SCALE, 0,
                -8 * PM_SCALE, -8 * PM_SCALE, -8 * PM_SCALE, 1, // box center px (8,8,8) -> anchor
        };
        pm_mat_yaw(tmp, yaw);
        mat4_multiply(tmp2, tmp, root);
        pm_mat_translate(tmp, x + PLYR_W / 2, anchor, z + PLYR_W / 2);
        mat4_multiply(root, tmp, tmp2);
        if (root_out) memcpy(root_out, root, sizeof root);

        // pieces may parent to ANY other piece (the editor's PARENT mode can
        // point at a later index), so resolve in dependency order: sweep
        // until everything whose parent is ready is done. The sanitizer cuts
        // cycles, but if one sneaks through, the final sweep hangs its pieces
        // from the center box rather than leaving garbage matrices.
        unsigned done = 0;
        for (int pass = 0; pass <= mo->nr_pieces; pass++)
        for (int i = 0; i < mo->nr_pieces; i++)
        {
                struct pm_piece *p = &mo->piece[i];
                if (done & 1u << i) continue;

                float *par;
                int pi = p->parent;
                if (pi < 0 || pi >= mo->nr_pieces || pi == i)
                        par = root;
                else if (done & 1u << pi)
                        par = space[pi];
                else if (pass < mo->nr_pieces)
                        continue; // parent not resolved yet: next sweep
                else
                        par = root; // unreachable (cycle): hang from the box

                // torso forward-tilt at full crouch; legs counter it (and a
                // bit more, so knees lead) and the head cancels it entirely
                #define PM_CROUCH_TILT 0.3f
                float pitch = 0, pyaw = 0, roll = 0, dy = 0;
                int leg = p->type == PM_T_LEG1 || p->type == PM_T_LEG2;
                int limb_or_head = leg || p->type == PM_T_HEAD
                                || p->type == PM_T_ARM_L
                                || p->type == PM_T_ARM_R;
                if (an && an->style == PM_STYLE_FLAIL && limb_or_head)
                {
                        // FLAIL: wave the limbs and head around, index-seeded;
                        // legs keep to pitch only so they read as walking.
                        // Every other type animates as in WALK
                        pitch = sinf(an->t * (1.1f + 0.2f * i) + i) * 0.9f;
                        if (!leg)
                                pyaw = sinf(an->t * (0.7f + 0.3f * i) + 2 * i) * 0.9f;
                }
                else if (an) switch (p->type)
                {
                        // WALK: per-type motion; stride is paced by distance
                        // walked so feet never skate. Arms swing gently
                        // opposite the same-side leg (LEG 1 pairs with the
                        // left by default - if a model reads wrong, swap its
                        // leg types)
                        case PM_T_LEG1:
                                pitch =  sinf(an->walk_phase) * 0.9f * an->speed
                                       - an->crouch * PM_CROUCH_TILT * 1.5f;
                                break;
                        case PM_T_LEG2:
                                pitch = -sinf(an->walk_phase) * 0.9f * an->speed
                                       - an->crouch * PM_CROUCH_TILT * 1.5f;
                                break;
                        case PM_T_ARM_L:
                                // walking swing fades out while falling; the
                                // arms raise to the sides instead, T-pose, wheee
                                pitch = -sinf(an->walk_phase) * 0.45f
                                                * an->speed * (1 - an->fall);
                                roll = -an->fall * (PI / 2);
                                break;
                        case PM_T_ARM_R:
                                pitch =  sinf(an->walk_phase) * 0.45f
                                                * an->speed * (1 - an->fall);
                                // mining/attacking: match the first-person
                                // hand's swing (hand.c) - a 9-tick half-sine
                                // arc, cocked up between swings, chopping
                                // down-and-forward at mid-arc (forward-up is
                                // negative pitch). Wins over the T-pose raise
                                // or they gimbal-fight
                                {
                                float arc = sinf(PI * fmodf(an->t / 0.45f, 1.f));
                                pitch = pitch * (1 - an->mine)
                                      - (1.25f - arc * 0.85f) * an->mine;
                                }
                                roll = an->fall * (PI / 2) * (1 - an->mine);
                                break;
                        case PM_T_HEAD:
                                // track the real look; yaw is body-relative
                                // and already limited (the body drags along).
                                // cancels the crouch tilt it inherits
                                pitch = an->look_pitch - an->crouch * PM_CROUCH_TILT;
                                pyaw = -an->look_yaw;
                                break;
                        case PM_T_TORSO:
                                // crouching: sink a little and tilt forward
                                // (the tilt brings the head down, so only half
                                // the dip is needed); bob down a touch at full
                                // stride spread. The whole rig follows
                                pitch = an->crouch * PM_CROUCH_TILT;
                                dy = an->crouch * 1.25f
                                   + fabsf(sinf(an->walk_phase)) * 0.6f * an->speed;
                                break;
                        case PM_T_TAIL:
                                // lazy idle sway crossfading into a wag locked
                                // to the stride (one sway per stride, in step
                                // with the legs); lifts a little on the move
                                // (lift is negative pitch, the arms' forward-up)
                                pyaw = sinf(an->tail_phase) * 0.25f * (1 - an->speed)
                                     + sinf(an->walk_phase) * 0.45f * an->speed;
                                pitch = -0.4f * an->speed;
                                break;
                        case PM_T_JIGGLE:
                                // the damped spring (landings) plus a pulse
                                // locked to the torso's walk bob, so it
                                // bounces in step instead of ringing free
                                pitch = an->bounce + sinf(an->walk_phase * 2)
                                        * 0.25f * an->speed;
                                break;
                        default: break;
                }
                // a long fall blends every limb into the panic flail (the
                // FLAIL-style sines, plus yaw on the legs - it's a panic)
                if (an && an->style == PM_STYLE_WALK && an->flail > 0.001f
                                && (p->type == PM_T_LEG1 || p->type == PM_T_LEG2
                                 || p->type == PM_T_ARM_L || p->type == PM_T_ARM_R))
                {
                        float pt = an->t * 2.f; // panic runs double speed
                        float fp = sinf(pt * (1.1f + 0.2f * i) + i) * 0.9f;
                        float fy = sinf(pt * (0.7f + 0.3f * i) + 2 * i) * 0.9f;
                        pitch += (fp - pitch) * an->flail;
                        pyaw  += (fy - pyaw)  * an->flail;
                }
                // the resting pose: the anim's angles swing on top
                if (pm_rest_apply)
                {
                        pitch += p->rest[0] * (PI / 180);
                        pyaw  += p->rest[1] * (PI / 180);
                        roll  += p->rest[2] * (PI / 180);
                }
                pm_mat_yaw(tmp, pyaw);
                pm_mat_pitch(tmp2, pitch);
                mat4_multiply(rot, tmp, tmp2);
                if (roll != 0)
                {
                        pm_mat_roll(tmp, roll);
                        mat4_multiply(tmp2, rot, tmp);
                        memcpy(rot, tmp2, sizeof rot);
                }

                // parent space -> this piece's space: pin origin to attach
                // (plus any vertical offset the animation asked for), rotate there
                pm_mat_translate(tmp, p->attach[0], p->attach[1] + dy, p->attach[2]);
                mat4_multiply(tmp2, par, tmp);
                mat4_multiply(tmp, tmp2, rot);
                pm_mat_translate(tmp2, -p->origin[0], -p->origin[1], -p->origin[2]);
                mat4_multiply(space[i], tmp, tmp2);

                // geometry runs 0..dims from the prism's min corner
                pm_mat_translate(tmp, p->corner[0], p->corner[1], p->corner[2]);
                mat4_multiply(geom[i], space[i], tmp);

                done |= 1u << i;
        }
}

// ---- per-frame build + render, mob.c-style ----

static struct allocation pm_alloc[MAX_FRAMES_IN_FLIGHT];
// world models for every slot (each plus a possible held-item cube) + one
// more model's worth for the editor preview, plus its selection-outline hull,
// a re-drawn parent (MOVE PART mode) and the joint gizmo (cube + 3 ruled
// axes of 40 one-texel segments each)
static struct pmvert pmbuf[(NR_PLAYERS + 1) * (PM_MAX_PIECES + 1) * PM_FACES + 128 * PM_FACES];
static int pm_count;  // total face instances this frame
static int pm_remote; // instances the main camera sees (the local player's own
                      // model is shadow-only until 2nd/3rd person cameras exist)

// per-slot animation accumulators, fed from player[] each frame. Remote
// players' pos/yaw/pitch sync over the net, and MSG_PLAYER's flags byte
// mirrors ground/sneaking/breaking/building/wet/noclip, so everyone
// animates from the same truth.
static struct pm_state {
        float px, py, pz;           // last frame's position
        float walk_phase, speed, crouch, body_yaw, fall, mine, flail;
        float pdy, jp, jv;          // jiggle spring: prev dy, position, velocity
        float tailp;                // accumulated tail-sway phase
        int airborne, fallt;
        int seen;
} pm_state[NR_PLAYERS];

#define PM_HEAD_LIM (PI / 3) // how far the head turns before dragging the body

static float pm_angwrap(float a)
{
        while (a >  PI) a -= 2 * PI;
        while (a < -PI) a += 2 * PI;
        return a;
}

static struct pm_anim *pm_animate(int slot, struct player *pl, float t)
{
        static struct pm_anim an[NR_PLAYERS];
        struct pm_anim *a = &an[slot];
        struct pm_state *s = &pm_state[slot];

        float dx = pl->pos.x - s->px, dz = pl->pos.z - s->pz;
        float dist = sqrtf(dx * dx + dz * dz);
        // a jump bigger than any legal move is a teleport or a world scoot:
        // no stride for those
        if (!s->seen) s->body_yaw = pl->yaw;
        if (!s->seen || dist > PLYR_SPD_R * 2) dist = 0;
        s->px = pl->pos.x; s->pz = pl->pos.z; s->seen = 1;

        s->walk_phase += dist * (2 * PI / 2000.f); // a full stride per 2 blocks
        float norm = dist / PLYR_SPD_R;            // 1.0 at running speed
        if (norm > 1) norm = 1;
        s->speed += (norm - s->speed) * 0.2f;
        s->crouch += ((pl->sneaking ? 1.f : 0.f) - s->crouch) * 0.25f;

        // freefall raises the arms - jumping up counts too. The physics
        // ground flag rides MSG_PLAYER now, so remote players are as
        // truthful as the local one (a few frames stale at worst)
        float dy = pl->pos.y - s->py;
        s->py = pl->pos.y;
        if (fabsf(dy) > BS) dy = 0; // teleport
        s->airborne = !pl->ground && !pl->noclip && !pl->wet;
        // the raise drifts up slowly; settling back is 4x quicker
        float ft = (float)s->airborne;
        s->fall += (ft - s->fall) * (ft > s->fall ? 0.02f : 0.08f);

        // a LONG fall (2s+) goes to full panic: arms and legs flail
        if (s->airborne) s->fallt++; else s->fallt = 0;
        s->flail += ((s->fallt > 120 ? 1.f : 0.f) - s->flail) * 0.1f;

        // JIGGLE pieces ride a damped spring kicked by vertical jolts
        // (landings above all - dy collapsing to zero is a big jerk). The
        // walking wobble is NOT fed through here: it's a direct bob-locked
        // pulse in pm_resolve, so it stays in step with the torso
        s->jv += (s->pdy - dy) * 0.003f;
        s->pdy = dy;
        s->jv -= s->jp * 0.2f;  // spring toward rest
        s->jv *= 0.9f;          // damping
        s->jp += s->jv;
        if (s->jp >  0.7f) s->jp =  0.7f;
        if (s->jp < -0.7f) s->jp = -0.7f;

        // idle tail-sway phase (walking hands the wag to walk_phase instead);
        // accumulated, never t * rate - that teleports when the rate moves
        s->tailp += 1.2f * 0.05f;

        // the item arm hammers away while breaking or building
        s->mine += ((pl->breaking || pl->building ? 1.f : 0.f) - s->mine) * 0.3f;

        // the body faces the direction walked - flipped 180 when that's
        // behind you (S, S+A, S+D back up rather than moonwalk) - and eases
        // there; standing still it holds
        if (dist > 1.f)
        {
                float move = atan2f(dx, dz);
                if (fabsf(pm_angwrap(move - pl->yaw)) > PI / 2 + 0.001f)
                        move += PI;
                s->body_yaw += pm_angwrap(move - s->body_yaw) * 0.25f;
        }
        // the head only turns PM_HEAD_LIM past the body before dragging it
        float d = pm_angwrap(pl->yaw - s->body_yaw);
        if (d >  PM_HEAD_LIM) s->body_yaw += d - PM_HEAD_LIM;
        if (d < -PM_HEAD_LIM) s->body_yaw += d + PM_HEAD_LIM;
        s->body_yaw = pm_angwrap(s->body_yaw);

        *a = (struct pm_anim){
                .walk_phase = s->walk_phase,
                .speed = s->speed,
                .body_yaw = s->body_yaw,
                .look_pitch = pl->pitch,
                .look_yaw = pm_angwrap(pl->yaw - s->body_yaw),
                .crouch = s->crouch,
                .fall = s->fall,
                .mine = s->mine,
                .flail = s->flail,
                .bounce = s->jp,
                .tail_phase = s->tailp,
                .t = t,
                .style = pm_models[slot].style,
        };
        return a;
}

// EYES pieces blink by simply not being drawn for ~0.08s every 3-5s. The
// cycle is offset and stretched per slot so a crowd never blinks in unison.
// (t advances 3.0 per second - see pmodel_build)
static int pm_blinking(int slot, float t)
{
        float per = 9.f + slot * 5 % 7; // 3..5s in t-units
        return fmodf(t + slot * 1.7f, per) < 0.25f;
}

static struct pmvert *pm_emit(struct pmvert *b, int slot,
                float x, float y, float z, float yaw, struct pm_anim *an)
{
        struct pmodel *mo = &pm_models[slot];
        float space[PM_MAX_PIECES][16], geom[PM_MAX_PIECES][16];
        pm_rest_apply = 1; // the world always shows the resting pose
        pm_resolve(mo, x, y, z, yaw, an, space, geom, NULL);
        pm_rest_apply = 0;

        // light the whole body from the block the hitbox center is in
        float il = 0.4f, gl = 0.f;
        int bx = (x + PLYR_W / 2) / BS, by = (y + PLYR_H / 2) / BS, bz = (z + PLYR_W / 2) / BS;
        if (legit_tile(bx, by, bz))
        {
                il = CORN_(bx, by, bz);
                gl = KORN_(bx, by, bz);
        }

        for (int i = 0; i < mo->nr_pieces; i++)
        {
                if (mo->piece[i].type == PM_T_EYES && an
                                && pm_blinking(slot, an->t))
                        continue;
                float *m = geom[i];
                for (int f = 0; f < PM_FACES; f++)
                {
                        *b = (struct pmvert){
                                .r0 = { m[0], m[4], m[8],  m[12] },
                                .r1 = { m[1], m[5], m[9],  m[13] },
                                .r2 = { m[2], m[6], m[10], m[14] },
                                .dims = { mo->piece[i].dims[0], mo->piece[i].dims[1], mo->piece[i].dims[2] },
                                .orient = f + 1,
                                .tex = pmodel_tex_base + slot * pm_slot_layers() + i * PM_FACES + f,
                                .illum = il, .glow = gl,
                        };
                        b++;
                }
                polys += PM_FACES;
        }

        // the held block rides in the RIGHT ARM's hand: a small cube at the
        // arm's far (y-down: bottom) end, in the arm's own space so it swings
        // with mining, raises with the T-pose, flails with the panic. Terrain
        // layers share the texture array, so tile_face_tex works as-is.
        int held = slot == my_player ? held_tile : pm_held[slot];
        if (held) for (int i = 0; i < mo->nr_pieces; i++)
        {
                struct pm_piece *p = &mo->piece[i];
                if (p->type != PM_T_ARM_R) continue;
                float hm[16], tr[16];
                pm_mat_translate(tr, p->corner[0] + p->dims[0] / 2.f - 3,
                                     p->corner[1] + p->dims[1] - 4,
                                     p->corner[2] + p->dims[2] / 2.f - 3);
                mat4_multiply(hm, space[i], tr);
                for (int f = 0; f < PM_FACES; f++)
                        *b++ = (struct pmvert){
                                .r0 = { hm[0], hm[4], hm[8],  hm[12] },
                                .r1 = { hm[1], hm[5], hm[9],  hm[13] },
                                .r2 = { hm[2], hm[6], hm[10], hm[14] },
                                .dims = { 6, 6, 6 },
                                .orient = f + 1,
                                .tex = tile_face_tex(held, f + 1),
                                .illum = il, .glow = gl,
                        };
                polys += PM_FACES;
                break; // one held item, first right arm gets it
        }
        return b;
}

void pmodel_build()
{
        static float t;
        t += 0.05f; // wall clock for idle motion (and the FLAIL style)

        struct pmvert *b = pmbuf;

        // remote players first: the main pass draws only [0, pm_remote)
        for (int i = 0; net_mode != NET_OFF && i < NR_PLAYERS; i++)
        {
                if (!net_player_active(i)) continue;
                struct player *r = &player[i];
                struct pm_anim *a = pm_animate(i, r, t);
                // a facing of (sin yaw, cos yaw) maps to model yaw PI - yaw;
                // the model turns with the BODY, the head makes up the look
                b = pm_emit(b, i, r->pos.x, r->pos.y, r->pos.z,
                                PI - a->body_yaw, a);
        }
        pm_remote = b - pmbuf;

        // the local player: drawn from lerped_pos, the same smoothed position
        // the camera rides - camplayer.pos is extrapolated a whole tick ahead
        // and stutters against it in third person. While the editor is open
        // the model leaves the world entirely (main AND shadow passes) - the
        // editor "grabbed" it
        if (!pmedit_on)
        {
                struct player lp = camplayer;
                lp.pos.x = lerped_pos.x;
                lp.pos.y = lerped_pos.y;
                lp.pos.z = lerped_pos.z;
                struct pm_anim *a = pm_animate(my_player, &lp, t);
                b = pm_emit(b, my_player, lp.pos.x, lp.pos.y, lp.pos.z,
                                PI - a->body_yaw, a);
        }
        pm_count = b - pmbuf;

        // the model editor's preview rides in the same buffer, past pm_count
        // so neither the main pass nor the shadow passes draw it
        if (pmedit_on)
                b = pmedit_emit(b);

        int fr = vk.currentFrame;
        if (!pm_alloc[fr].buf)
                vulkan_allocate_vertex_buffer(sizeof pmbuf, &pm_alloc[fr]);
        vulkan_populate_vertex_buffer(pmbuf, (b - pmbuf) * sizeof *pmbuf, &pm_alloc[fr]);
}

void pmodel_render(VkCommandBuffer cmdbuf, int pipe, float *pv)
{
        // the main camera skips your own model only in first person; the
        // shadow passes always draw everyone
        int count = pipe == pmodel_pipe && cam_view == CAM_FIRST
                        ? pm_remote : pm_count;
        if (!count) return;

        vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipelines[pipe].pipeline);

        struct { float pv[16]; } push;
        memcpy(push.pv, pv, sizeof push.pv);
        vkCmdPushConstants(cmdbuf, vk.pipelines[pipe].layout,
                        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                        0, sizeof push, &push);

        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(cmdbuf, 0, 1, &pm_alloc[vk.currentFrame].buf, &offset);
        vkCmdDraw(cmdbuf, 4, count, 0, 0);
}

#endif // BLOCKO_PMODEL_C_INCLUDED

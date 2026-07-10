#include "blocko.c"
#ifndef BLOCKO_DEFS_C_INCLUDED
#define BLOCKO_DEFS_C_INCLUDED

#define STON 34
#define ORE  35
#define OREH 36
#define HARD 37
#define GRAN 38

#define SAND 41
#define DIRT 42
#define WOOD 43

#define GRG1 45
#define GRG2 46
#define GRAS 47
#define MTGR 48            // mountain grass

#define RLEF 60
#define YLEF 61
#define SLEF 62            // spruce leaves

#define BARR 64

#define LASTSOLID (BARR+1) // everything less than here is solid
#define OPEN 75            // empty space

// Blocks that faces should be drawn against (transparent or non-solid)
#define IS_SEE_THROUGH(t) ((t) >= OPEN || (t) == RLEF || (t) == YLEF || (t) == SLEF)
#define WATR 76
#define LITE 77
#define TLGR 78            // tall grass: non-solid billboard growing on GRAS
#define TMGR 79            // tall mountain grass: non-solid billboard growing on MTGR


#ifndef TERRAIN_THREAD
#ifdef NO_OMPH // without OpenMP the worker sections in main() never run
#define TERRAIN_THREAD 0
#else
#define TERRAIN_THREAD 1           // whether to put terrain generation in its own thread
#endif
#endif

#define WINW 1920                  // window width, height
#define WINH 1000                  // ^
#define CHUNKW 64                  // chunk size (vao size)
#define CHUNKD 64                  // ^
#define CHUNKW2 (CHUNKW/2)
#define CHUNKD2 (CHUNKD/2)
#define VAOW 16                    // how many VAOs wide
#define VAOD 16                    // how many VAOs deep
#define VAOS (VAOW*VAOD)           // total nr of vbos
#define MAX_MESHES_PER_FRAME 1     // max dirty chunks to rebuild per frame
#define TILESW (CHUNKW*VAOW)       // total level width, height
#define TILESH 256                 // ^
#define SEA_LEVEL (TILESH - 80)    // y of the water surface; keeps 80 blocks of sea-floor depth
#define TERRAIN_VSCALE 160         // blocks per 1.0 of terrain height value
#define TILESD (CHUNKD*VAOD)       // ^
#define BS 1000                    // block size
#define BS2 (BS/2)                 // block size in half
#define PLYR_W 700                 // physical width and height of the player
#define PLYR_H 1800                // ^
#define PLYR_SPD 100                 // units per frame
#define PLYR_SPD_R 200              // units per frame (running)
#define PLYR_SPD_S 50               // units per frame (sneaking)
#define PLYR_ACCEL 17               // acceleration per frame (scaled for BS=1000)
#define EYEDOWN 500                 // how far down are the eyes from the top of the head
#define STARTPX (553*BS)           // starting position
#define STARTPY 0                  // ^
#define STARTPZ (222*BS)           // ^ (coastal; 733 is open ocean now)
#define NR_PLAYERS 8
#define JUMP_BUFFER_FRAMES 6
#define GRAV_JUMP 0
#define GRAV_EXIT 2                 // upward vel for hopping out of water onto land
#define GRAV_SWIM 3                 // upward vel to swim toward (damped while wet)
#define GRAV_FLOAT GRAV_ZERO
#define GRAV_ZERO 14
#define GRAV_WET_MAX 28             // terminal sinking vel in water
#define GRAV_MAX 49

#define UP    1
#define EAST  2
#define NORTH 3
#define WEST  4
#define SOUTH 5
#define DOWN  6

// Face bitmask for LOD culling
#define FACE_UP    (1 << 0)
#define FACE_DOWN  (1 << 1)
#define FACE_NORTH (1 << 2)
#define FACE_SOUTH (1 << 3)
#define FACE_EAST  (1 << 4)
#define FACE_WEST  (1 << 5)
#define FACE_ALL   0x3F

// LOD settings
#define LOD_DIST_THRESHOLD 160.0f  // distance in blocks before backface culling LOD kicks in
#define LOD_ANGLE_SIN 0.259f       // sin(15°) - angular tolerance for face culling

#define VERTEX_BUFLEN (CHUNKW*CHUNKD*32) // scales with chunk area (131072 at 64x64)
#define MAX_MESH_THREADS 16
// Mesh rebuilds are memory-bandwidth bound: measured, the sweet spot is
// ~cores-4 threads (enough to hide memory latency, but leaving cores for the
// two terrain workers + main; beyond that, fork/join overhead wins). Set from
// the core count in startup(); tunable at runtime via the meshthr command.
// (MAX_MESH_THREADS only sizes the per-thread scratch buffers.)
int mesh_threads = 8;
#define SUNQLEN 64000
#define GLOQLEN 64000

// One shadow cascade: a ±10 block bubble around the player, fading to lit
// at its edge (see main.frag)
#define SHADOW_NEAR   0
#define SHADOW_COUNT  1
const int shadow_sz[SHADOW_COUNT] = { 2048 };

#define ICLAMP(v, l, u) ((v < l) ? l : (v > u) ? u : v)
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

#define true 1
#define false 0

#define PI (3.1415926535)
#define PI2 (PI/2.f)
#define TAU (PI*2.f)

#define PNG0 19

#define DAY_R 1.f
#define DAY_G 1.f
#define DAY_B 1.f
#define NIGHT_R 0.32f
#define NIGHT_G 0.28f
#define NIGHT_B 0.41f
#define DUSK_R 0.8f
#define DUSK_G 0.5f
#define DUSK_B 0.4f
// tile pos-to-mem-location macros
#define T_(x,y,z)    tiles[    ((z - scootz) & (TILESD-1)) * (TILESH+0) * (TILESW+0) + ((x - scootx) & (TILESW-1)) * (TILESH+0) + (y)]
#define SUN_(x,y,z)  sunlight[ ((z - scootz) & (TILESD-1)) * (TILESH+0) * (TILESW+0) + ((x - scootx) & (TILESW-1)) * (TILESH+0) + (y)]
#define GLO_(x,y,z)  glolight[ ((z - scootz) & (TILESD-1)) * (TILESH+0) * (TILESW+0) + ((x - scootx) & (TILESW-1)) * (TILESH+0) + (y)]
#define CORN_(x,y,z) cornlight[((z - scootz) & (TILESD-1)) * (TILESH+1) * (TILESW+1) + ((x - scootx) & (TILESW-1)) * (TILESH+1) + (y)]
#define KORN_(x,y,z) kornlight[((z - scootz) & (TILESD-1)) * (TILESH+1) * (TILESW+1) + ((x - scootx) & (TILESW-1)) * (TILESH+1) + (y)]
#define GNDH_(x,z)   gndheight[((z - scootz) & (TILESD-1))              * (TILESW+0) + ((x - scootx) & (TILESW-1))                   ]

// for terrain/worker. tiles/sun/gndheight writes go through the thread's
// claimed destination window (gen_area) so the same gen code can fill the
// main ring or a per-player sim area; light stays main-ring (light = render)
#define TT_(x,y,z)    gen_area->tiles[((z - tscootz) & gen_area->maskd) * gen_area->pitchz + ((x - tscootx) & gen_area->maskw) * gen_area->pitchx + (y)]
#define TSUN_(x,y,z)  gen_area->sun[  ((z - tscootz) & gen_area->maskd) * gen_area->pitchz + ((x - tscootx) & gen_area->maskw) * gen_area->pitchx + (y)]
#define TGLO_(x,y,z)  glolight[ ((z - tscootz) & (TILESD-1)) * (TILESH+0) * (TILESW+0) + ((x - tscootx) & (TILESW-1)) * (TILESH+0) + (y)]
#define TCORN_(x,y,z) cornlight[((z - tscootz) & (TILESD-1)) * (TILESH+1) * (TILESW+1) + ((x - tscootx) & (TILESW-1)) * (TILESH+1) + (y)]
#define TKORN_(x,y,z) kornlight[((z - tscootz) & (TILESD-1)) * (TILESH+1) * (TILESW+1) + ((x - tscootx) & (TILESW-1)) * (TILESH+1) + (y)]
#define TGNDH_(x,z)   gen_area->gndh[ ((z - tscootz) & gen_area->maskd) * (gen_area->maskw + 1) + ((x - tscootx) & gen_area->maskw)]

// chunk pos-to-mem-location macros
// a chunk is "generated" iff its ring slot is stamped with the absolute chunk
// coords the window currently maps there - so scooting self-invalidates slots
#define AGEN_SLOT(x,z)  chunk_stamp[(z - chunk_scootz) & (VAOD-1)][(x - chunk_scootx) & (VAOW-1)]
#define AGEN_(x,z)   (AGEN_SLOT(x, z).ax == (x) - chunk_scootx && AGEN_SLOT(x, z).az == (z) - chunk_scootz)
#define DIRTY_(x,z)  chunk_dirty[(z - chunk_scootz) & (VAOD-1)][(x - chunk_scootx) & (VAOW-1)]
#define LIGHTDIRTY_(x,z) chunk_lightdirty[(z - chunk_scootz) & (VAOD-1)][(x - chunk_scootx) & (VAOW-1)]
#define DIRTY_LIGHT(x,z) (LIGHTDIRTY_(x, z) = frame + 1)
#define FACES_(x,z)  chunk_faces[(z - chunk_scootz) & (VAOD-1)][(x - chunk_scootx) & (VAOW-1)]
#define LOD_(x,z)    chunk_lod[(z - chunk_scootz) & (VAOD-1)][(x - chunk_scootx) & (VAOW-1)]
#define VAO_(x,z)    vbo[    ((z - chunk_scootz) & (VAOD-1)) * (VAOW) + ((x - chunk_scootx) & (VAOW-1))]
#define VBO_(x,z)    vao[    ((z - chunk_scootz) & (VAOD-1)) * (VAOW) + ((x - chunk_scootx) & (VAOW-1))]
#define VBOLEN_(x,z) vbo_len[((z - chunk_scootz) & (VAOD-1)) * (VAOW) + ((x - chunk_scootx) & (VAOW-1))]
#define WBOSTART_(x,z) wbo_start[((z - chunk_scootz) & (VAOD-1)) * (VAOW) + ((x - chunk_scootx) & (VAOW-1))]
#define LEAFSTART_(x,z) leaf_start[((z - chunk_scootz) & (VAOD-1)) * (VAOW) + ((x - chunk_scootx) & (VAOW-1))]
// the mesh's own identity: which absolute chunk the slot's GPU buffers hold.
// regen_world invalidates chunk_stamp but not this, so the old mesh keeps
// drawing in place while its chunk regenerates (meshes are chunk-relative,
// positioned per draw, so a same-chunk mesh is never misplaced); a slot
// reused for a different chunk (scoot) mismatches and hides as before
#define MESHGEN_SLOT(x,z) mesh_stamp[(z - chunk_scootz) & (VAOD-1)][(x - chunk_scootx) & (VAOW-1)]
#define MESHGEN_(x,z) (MESHGEN_SLOT(x, z).ax == (x) - chunk_scootx && MESHGEN_SLOT(x, z).az == (z) - chunk_scootz)
// GPU vertex buffers live in the ring too, so meshes follow their chunk
#define WBUF_(x,z)    world_buf[((x - chunk_scootx) & (VAOW-1)) * VAOD + ((z - chunk_scootz) & (VAOD-1))]
#define WMAPPED_(x,z) ((char *)world_mapped[(x - chunk_scootx) & (VAOW-1)] + ((z - chunk_scootz) & (VAOD-1)) * world_aligned_sz)

// for terrain/worker
#define TAGEN_SLOT(x,z) chunk_stamp[(z - tchunk_scootz) & (VAOD-1)][(x - tchunk_scootx) & (VAOW-1)]
#define TAGEN_(x,z)   (TAGEN_SLOT(x, z).ax == (x) - tchunk_scootx && TAGEN_SLOT(x, z).az == (z) - tchunk_scootz)
#define TEDGE_SLOT(x,z) chunk_estamp[(z - tchunk_scootz) & (VAOD-1)][(x - tchunk_scootx) & (VAOW-1)]
#define TEDGE_(x,z)   (TEDGE_SLOT(x, z).ax == (x) - tchunk_scootx && TEDGE_SLOT(x, z).az == (z) - tchunk_scootz)

// helper macros
#define IS_OPAQUE(x,y,z) (T_(x, y, z) < LASTSOLID)
#define IS_SOLID(x,y,z) (T_(x, y, z) < LASTSOLID)
#define ABOVE_GROUND(x,y,z) (GNDH_(x, z) >  y)
#define AT_GROUND(x,y,z)    (GNDH_(x, z) == y)
#define BELOW_GROUND(x,y,z) (GNDH_(x, z) <  y)

// helper macros for terrain/worker
#define TIS_OPAQUE(x,y,z) (TT_(x, y, z) < LASTSOLID)
#define TIS_SOLID(x,y,z) (TT_(x, y, z) < LASTSOLID)

#define QITEM(x,y,z) ((struct qitem){x, y, z})
#define DIST_SQ(dx, dy, dz) ((dx)*(dx) + (dy)*(dy) + (dz)*(dz))
#define DIST(dx, dy, dz) (sqrt(DIST_SQ(dx, dy, dz)))

#define B2P(b) ((b)*BS)
#define P2B(p) ((p)/BS)
#define C2B(c) ((c)*CHUNKW)
#define C2P(c) ((c)*BS*CHUNKW)
#define B2C(b) ((b)/CHUNKW)
#define P2C(p) (((p)/CHUNKW)/BS)

// dumb rand -- for simple deterministic rand
unsigned dumb_rand(unsigned *seed) { return (*seed = (1103515245 * *seed + 12345) % 2147483648); }
// helpers for dumb rand, must have local var called seed for all of these
#define RAND (dumb_rand(&seed))
// random float in the range 0-1
#define RAND01 ((double)RAND / 2147483648.0)
// random int in the range lo to hi
// the (int) cast matters: RAND is unsigned, so a negative result would
// otherwise wrap to a huge value in float contexts (e.g. point structs)
#define RANDI(lo,hi) ((int)(RAND % (1 + (hi) - (lo))) + (lo))
// random float in the range lo to hi
#define RANDF(lo,hi) (RAND01 * ((hi) - (lo)) + (lo))
// random true/false, true pct percent of the time
#define RANDP(pct) (RAND01 * 100.0 <= (double)(pct))
// randomly true or false 50/50
#define RANDBOOL (RAND % 2 == 0)
// helpers for deterministically setting seed from several values, plus world_seed
#define SEED1(a)       (world_seed ^ ((a) << 4))
#define SEED2(a,b)     (world_seed ^ ((a) << 4) ^ ((b) << 8))
#define SEED3(a,b,c)   (world_seed ^ ((a) << 4) ^ ((b) << 8) ^ ((c) << 12))
#define SEED4(a,b,c,d) (world_seed ^ ((a) << 4) ^ ((b) << 8) ^ ((c) << 12) ^ ((d) << 16))

#define ALIGN_UP(n, alignment) (((n) + (alignment) - 1) & ~((alignment) - 1))

struct allocation {
        VkDeviceMemory mem;
        VkBuffer buf;
        VkBufferCreateInfo buf_info;
};

VkBuffer world_buf[VAOS];
VkBufferCreateInfo world_buf_info;
VkDeviceMemory world_mem[VAOW];
void *world_mapped[VAOW];  // persistently mapped pointers
VkMemoryAllocateInfo world_mem_info;
size_t world_aligned_sz;
#define MAX_FRAMES_IN_FLIGHT 4
VkBuffer main_buffer[MAX_FRAMES_IN_FLIGHT];
VkDeviceMemory main_memory[MAX_FRAMES_IN_FLIGHT];
VkDescriptorSetLayout main_descriptor_set_layout;
VkDescriptorPool descriptor_pool;
VkDescriptorSet main_descriptor_set[MAX_FRAMES_IN_FLIGHT];

// Texture array
VkImage texture_image;
VkDeviceMemory texture_memory;
VkImageView texture_image_view;
VkSampler texture_sampler;

// Visible chunk info for single-pass frustum culling
struct visible_chunk {
        int x, z;           // chunk indices
        unsigned char shadow_mask;  // bitmask: which shadow cascades see this chunk
        unsigned char camera_visible;  // true if visible to main camera
        float dist_sq;      // squared distance to camera (for front-to-back sorting)
};
struct visible_chunk visible_chunks[VAOW * VAOD];
int visible_chunk_count = 0;

float peye0, peye1, peye2;
float proj_view_mtrx[16];

float dist2sun = TILESW * BS;
float shadow_target[3];

// Shadow mapping resources
struct shadow_cascade {
    VkImage image;
    VkDeviceMemory memory;
    VkImageView image_view;
    VkFramebuffer framebuffer;
    float matrix[16];      // stored PV matrix
};
struct shadow_cascade shadow[SHADOW_COUNT];

VkSampler shadow_sampler;
VkRenderPass shadow_render_pass;
int shadow_pipe;
int shadow_solid_pipe; // no-op fragment stage: fast depth path, leaves solid

// Light too low to matter: skip the whole shadow pipeline (set per-frame)
int shadow_idle;

struct main_ubo {
    float model[16];              // mat4 - offset 0
    float view[16];               // mat4 - offset 64
    float proj[16];               // mat4 - offset 128
    float shadow_space[16];       // mat4 - offset 192 (the one near cascade)
    float bs;                     // float - offset 256
    float padding1[3];            // Padding to align day_color to 16 bytes

    float day_color[3];   // vec3 - offset 272
    float padding2;       // Padding
    float glo_color[3];   // vec3 - offset 288
    float fog_lo;         // float - offset 300
    float fog_hi;         // float - offset 304
    float padding3[3];    // Padding to align light_pos to 16 bytes
    float light_pos[3];   // vec3 - offset 320
    float padding4;       // Padding
    float view_pos[3];    // vec3 - offset 336
    float sharpness;      // float - offset 348
    int shadow_mapping;   // bool (as int) - offset 352
    float sun_strength;   // float - offset 356
    float sun_warmth;     // float - offset 360
    int water_frame;           // int   - offset 364
    float underwater;          // float - offset 368 (camera eye is in water)
    float scootx;              // float - offset 372 (window->world block offset x)
    float scootz;              // float - offset 376 (window->world block offset z)
    float padding5;            // Padding to align sun_dir to 16 bytes
    float sun_dir[3];          // vec3  - offset 384 (unit vector toward the sun)
    float night_amt;           // float - offset 396 (0 day, 0.5 dusk, 1 night)
    float shadow_fade;         // float - offset 400 (1 full shadows, ->0 eases contrast out before the idle cutoff)
} main_ubo;

unsigned int vbo[VAOS], vao[VAOS];
size_t vbo_len[VAOS];
size_t wbo_start[VAOS];
size_t leaf_start[VAOS]; // terrain section is [solid | leaves): leaves start
                         // here, so shadows can draw solids without alpha test

struct vbufv {
    float tex;     // Location 0
    float orient;  // Location 1
    float x, y, z;  // Location 2
    float illum0, illum1, illum2, illum3; // Location 3
    float glow0, glow1, glow2, glow3;  // Location 4
    float alpha;   // Location 5
} ShaderInput;

struct vbufv vbuf[VERTEX_BUFLEN + 1000]; // vertex buffer + padding
struct vbufv *v_limit = vbuf + VERTEX_BUFLEN;
struct vbufv *v = vbuf;

struct vbufv wbuf[VERTEX_BUFLEN + 1000]; // water buffer
struct vbufv *w_limit = wbuf + VERTEX_BUFLEN;
struct vbufv *w = wbuf;

// Per-thread buffers for parallel mesh building (each needs full size)
struct vbufv vbuf_mt[MAX_MESH_THREADS][VERTEX_BUFLEN + 1000];
struct vbufv wbuf_mt[MAX_MESH_THREADS][VERTEX_BUFLEN + 1000];
// leaves collect separately so the terrain section lands [solid | leaves);
// sized smaller - canopies are a fraction of a chunk (overflow spills to solid)
struct vbufv lbuf_mt[MAX_MESH_THREADS][VERTEX_BUFLEN/8 + 1000];
size_t mesh_leaf_start; // solid face count of the last mesh_region build

float night_amt;

unsigned char *tiles;
unsigned char *sunlight;
unsigned char *glolight;
unsigned char gndheight[TILESW * TILESD];
float *cornlight;
float *kornlight;
struct chunk_stamp { int ax, az; };                  // absolute chunk coords a ring
// a windowed piece of world: a power-of-2 ring of tile columns addressed by
// absolute coords masked. The main render ring is one (main_area, aliasing
// tiles/sunlight/gndheight); per-player sim areas are small ones. Gen writes
// go through the thread-local gen_area so one set of macros fits all sizes.
#define SIM_AREA_CHUNKS 4                     // sim area ring size in chunks
#define SIM_AREA_W (SIM_AREA_CHUNKS * CHUNKW) // ^ in blocks
#define SIM_AREA_CMASK (SIM_AREA_CHUNKS - 1)
struct warea {
        unsigned char *tiles;
        unsigned char *sun;   // sim areas: gen scratch only (light = render)
        unsigned char *gndh;
        int maskw, maskd;     // block coord masks (TILESW-1 etc for main)
        int pitchx, pitchz;   // array elements per block step in x / z
        // sim-area ring state; main_area keeps all of this in the chunk_*
        // globals instead. Stamps hold absolute chunk coords at slot
        // [az & cmask][ax & cmask], so scoots self-invalidate exactly like
        // the main ring. Lifecycle fields change only inside the (chunks)
        // critical; a builder that sees epoch move abandons its job.
        volatile struct chunk_stamp stamp[SIM_AREA_CHUNKS][SIM_AREA_CHUNKS];  // pass 2 done
        volatile struct chunk_stamp estamp[SIM_AREA_CHUNKS][SIM_AREA_CHUNKS]; // pass 1 done
        int cx0, cz0;         // absolute chunk coords of the coverage low corner
        volatile int active;  // following a connected remote player (server only)
        volatile int busy;    // a builder is filling it (one at a time per area)
        volatile int epoch;   // bumped on activate/scoot/deactivate/regen
};
struct warea main_area; // filled in startup() once the buffers exist
_Thread_local struct warea *gen_area = &main_area;
struct warea sim_area[NR_PLAYERS]; // [i] follows remote player i (server only)
unsigned char *area_sun_scratch;   // one shared write-only sun buffer: gen never
                                   // reads sun, and areas skip light entirely
// freshly generated area chunks awaiting main-thread edit-overlay replay
// (the area cousin of just_generated); guarded by the (chunks) critical
struct area_fresh { struct warea *a; int acx, acz; } area_fresh[256];
int area_fresh_len;
volatile struct chunk_stamp chunk_stamp[VAOD][VAOW]; // slot holds (INT_MIN = never)
// which chunk the slot's uploaded MESH is of (main thread only, see MESHGEN_)
struct chunk_stamp mesh_stamp[VAOD][VAOW];
// pass 1 (chunk edge columns) stamps: a chunk's interior can generate only
// after its own and all 8 neighbors' edges exist, so nothing ever reads or
// writes outside its own chunk
volatile struct chunk_stamp chunk_estamp[VAOD][VAOW];
// transient marks so builder threads never work the same slot at once,
// guarded by the (chunks) critical section in chunk_builder
volatile char chunk_claim1[VAOD][VAOW]; // pass 1 running
volatile char chunk_claim2[VAOD][VAOW]; // pass 2 running
// bumped by regen_world so mid-job builders abandon instead of stamping
// stale data or waiting forever on invalidated edges; only touched inside
// the (chunks) critical section
volatile int regen_epoch;
volatile char chunk_dirty[VAOW][VAOD];
volatile unsigned chunk_lightdirty[VAOW][VAOD]; // frame+1 of last light change (0 = clean)
int remesh_debounce = 15; // remesh light-dirty chunks only after this many quiet frames
unsigned char chunk_faces[VAOW][VAOD];  // bitmask of faces included in mesh
unsigned char chunk_lod[VAOW][VAOD];    // 0=full detail, 1=LOD mode (backface culled)

int future_scootx, future_scootz; // pending global map offset
int scootx, scootz;               // actual global map offset
int chunk_scootx, chunk_scootz;   //  ^ in chunks
// each builder thread's private copies of the scoot vars: a thread's window
// mapping must hold still for its whole iteration even if another thread
// applies a newer scoot (ring slots are absolute-derived, so slots agree)
_Thread_local int tscootx, tscootz;
_Thread_local int tchunk_scootx, tchunk_scootz;

struct box { float x, y, z, w, h ,d; };
struct point { float x, y, z; };
struct qchunk { int x, y, z, sqdist; };
struct qitem { int x; union { int y; int d; }; int z; };

struct qitem sunq0_[SUNQLEN+1];
struct qitem sunq1_[SUNQLEN+1];
struct qitem *sunq_curr = sunq0_;
struct qitem *sunq_next = sunq1_;
size_t sq_curr_len;
size_t sq_next_len;

struct qitem gloq0_[GLOQLEN+1];
struct qitem gloq1_[GLOQLEN+1];
struct qitem *gloq_curr = gloq0_;
struct qitem *gloq_next = gloq1_;
size_t gq_curr_len;
size_t gq_next_len;

struct qcave { int x, y, z; int radius_sq; };

struct player {
        struct box pos;
        struct point vel;
        float yaw;
        float pitch;
        int wet;
        int submerged;
        int cooldownf;
        int runningf;
        int goingf;
        int goingb;
        int goingl;
        int goingr;
        int jumping;
        int jump_held;
        int sneaking;
        int running;
        int breaking;
        int building;
        int lighting;
        int cooldown;
        int mine_progress; // frames held on the current target block
        int fvel;
        int rvel;
        int grav;
        int ground;
        int noclip; // fly through solids, no gravity; jump=up, sneak=down
};

struct player player[NR_PLAYERS] = {{
        .pos.x = STARTPX,
        .pos.y = STARTPY,
        .pos.z = STARTPZ,
        .pos.w = PLYR_W,
        .pos.h = PLYR_H,
        .pos.d = PLYR_W,
        .yaw = PI * 0.5f,
        .grav = GRAV_ZERO,
}};
int my_player; // the slot this instance controls (0 until netplay assigns ids)
struct player camplayer;

#define NR_MOBS 32
struct mob {
        struct box pos;
        struct box prev;          // last tick, for smooth drawing in >60FPS
        struct point vel;
        int alive;
        int grav;
        int ground;
        int wet;
        float bob;                // vertical velocity while floating in water
        int hop_cooldown;         // frames of rest before the next hop
        int bonk_cooldown;        // frames until it can hit the player again
        int dying;                // death-animation frames remaining
        float yaw;                // facing angle (radians); front points here
        float prev_yaw;           // last tick's yaw, for smooth drawing
        float target_yaw;         // where it wants to be facing
};
struct mob mob[NR_MOBS];
int mob_enable = 1;
int mob_kills;
float mob_lerp_t; // how far past the last physics tick the frame is drawn

struct point lerped_pos;
struct point sun_pos;
struct point moon_pos;

SDL_Event event;

unsigned int material_tex_id;
unsigned int shadow_tex_id;
unsigned int shadow2_tex_id;
unsigned int shadow_fbo;
unsigned int shadow2_fbo;

unsigned int prog_id;
unsigned int shadow_prog_id;

//globals
int frame = 0;
int pframe = 0;
int help_layer = 0;
float cull_mtrx[16]; // camera frustum used for chunk culling
float cull_x, cull_z; // camera position for range culling
float draw_dist = 640.f;
int zooming = false;
float zoom_amt = 1.f;
float fast = 1.f;
int headless; // dedicated server: no window, no vulkan, no input
int shadow_mapping = true;
float sun_pitch = .3f; //.6f; // 0 = east, PI/2 = up, PI = west, 3PI/2 = down
float moon_pitch;
float sun_yaw = .3f;
float sun_roll = -1.3f;
void game_shutdown(int code);
int main_pipe;       // main terrain rendering pipeline
int water_pipe;      // transparent water rendering pipeline (near, back-to-front)
int water_solid_pipe;// opaque water pipeline (far chunks: no blend, back-face
                     // culled, front-to-back for early-Z - see draw.c water pass)
int mob_pipe;        // mob rendering pipeline (mob.vert + shared main.frag)
int mob_shadow_pipe; // mob shadow-cast pipeline (mob_shadow.vert + shadow.frag)

int mouselook = false; // start with the mouse free; click the window to capture
int target_x, target_y, target_z;
int place_x, place_y, place_z;
int held_tile = HARD; // the block right-click places; mouse wheel cycles it (hand.c)
int hand_swing_kick;  // set on a successful place; makes the held block swing (hand.c)
// hold-to-break: mining the target block takes MINE_TIME held frames, so a
// quick click punches mobs (see mob.c) without chipping the terrain
#define MINE_TIME 45
int mine_x = -1, mine_y = -1, mine_z = -1; // block progress is accumulating on
int mine_tile = -1; // that block's tile type, for the shaking stand-in
int mine_hole;      // carve the block out of its chunk mesh while mining
float mine_frac;    // 0..1 mining progress, shown on the crosshair
// reject+patch: a block edit shows instantly by rejecting the stale faces in a
// small box (in the vertex shader) and drawing a tiny corrected "patch" mesh
// over it, while the big chunk buffer folds the edit in on a debounced rebuild
int patch_active;               // a persistent edit patch is pending a rebuild
int patch_lo[3], patch_hi[3];   // union box of pending edits, ABSOLUTE tiles, inclusive
int patch_vert_count;           // opaque verts staged for this frame's patch draw
int patch_water_count;          // water/glow verts staged for this frame's patch draw
// the effective reject/patch box for THIS frame = union of the persistent edit
// box (break/place, above) and the transient mining box (the block being dug,
// still solid in tiles but shown as a hole). Recomputed each frame in patch_update
int patch_box_on;
int patch_box_lo[3], patch_box_hi[3]; // ABSOLUTE tiles, inclusive
int patch_meshing;              // set only while the patch calls mesh_region, so the
                                // mine_hole carve happens for the patch, not for chunks
int patch_tint;                 // debug viz: tint the patch mesh red (socket `tint`).
                                // rides the unused reject_lo.w slot, read in main.frag
int screenw = WINW;
int screenh = WINH;
volatile struct qitem just_generated[VAOW*VAOD];
volatile size_t just_gen_len;

int cave_enable = 1;
int tree_enable = 1;
int flat_world = 0; // force a perfectly flat world (debug: isolates seams)

// world-gen knobs: each row of gen_knobs.h becomes a live float variable
// (terrain.c expands the common rows the same way)
#define TWEAK(name, def, lo, hi, fl, desc) float name = def;
#define TWEAK_VAR(name, def, lo, hi, fl, desc) // defined elsewhere
#define TWEAK_SECTION(title)
#include "gen_knobs.h"
#undef TWEAK
#undef TWEAK_VAR
#undef TWEAK_SECTION

// vksetup.c protos
int check_program_errors(unsigned int shader, char *name);
unsigned int file2shader(unsigned int type, char *filename);

// font.c protos
void font_frame_reset();
void font_begin(int w, int h);
void font_add_text(char *s, int inx, int iny, float scale);
void font_end(float r, float g, float b);

// cursor.c protos
VkBuffer vulkan_fill_buffer(void *buf, size_t sz);
void cursor(VkCommandBuffer cmdbuf);

// atmosphere.c protos
void do_atmos_colors();
void sun_draw(VkCommandBuffer cmdbuf, float *proj, float *view, float pitch, float yaw, float roll);
void sky_draw(VkCommandBuffer cmdbuf, float *proj, float *view);

// player.c protos
void lerp_camera(float t, struct player *a, struct player *b);
void update_player(struct player * p, int real);
int move_box(struct box *pos, int velx, int vely, int velz);
int move_player(struct player *p, int velx, int vely, int velz);
void mine_heal();

// mob.c protos
void update_mobs();
void mob_kill(int i);
void mob_build();
void mob_render(VkCommandBuffer cmdbuf, int pipe, float *pv);
void mob_scoot(int dx, int dz);
int mob_spawn(int bx, int bz);
int mob_spawn_at(int bx, int by, int bz);

// mine.c protos
void mine_overlay_build();
void mine_overlay_render(VkCommandBuffer cmdbuf, int pipe, float *pv);

// hand.c protos
void held_cycle(int dir);
void hand_animate(struct player *p);
void hand_build();
void hand_render(VkCommandBuffer cmdbuf, int pipe, float *pv);

// item.c protos
void item_spawn(int x, int y, int z, int tile);
void update_items();
void item_scoot(int dx, int dz);
void item_build();
void item_render(VkCommandBuffer cmdbuf, int pipe, float *pv);

// patch.c protos
void patch_edit(int wx, int wy, int wz);
void patch_update();
void patch_reject_box(float lo[4], float hi[4]);
void patch_render(VkCommandBuffer cmdbuf, int pipe, float *pv);
void patch_render_water(VkCommandBuffer cmdbuf, int pipe, float *pv);

// edit.c protos
void set_tile(int x, int y, int z, int t);
void edit_record(int x, int y, int z, int tile);
void edit_apply_chunk(int acx, int acz);
void edit_apply_remote(int ax, int ay, int az, int tile);
void edit_clear();
int edit_next(int *it, int *x, int *y, int *z, int *tile);
extern int edit_len;

// net.c protos
#define NET_PORT 26262 // default TCP port for --serve/--connect
enum { NET_OFF, NET_SERVER, NET_CLIENT };
extern int net_mode;
void net_poll();
void net_send_edit(int x, int y, int z, int tile);
int net_serve(int port);
int net_connect(const char *host, int port);
int net_describe(char *out, int outsz);
int net_player_active(int i);
void net_send_punch(int slot);
void net_send_bonk(int pi, float nx, float nz);
void net_send_chat(const char *text);
void chat_add(const char *s); // console.c: append a line to the on-screen chat
void regen_world(); // blocko.c: invalidate all chunk stamps

// collision.c protos
int collide(struct box l, struct box r);
int world_collide(struct box box, int wet);

// test.c protos
void debrief();

// blocklight.c protos
void sun_enqueue(int x, int y, int z, unsigned char incoming_light);
void glo_enqueue(int x, int y, int z, unsigned char incoming_light);
int step_sunlight();
int step_glolight();
void remove_sunlight(int px, int py, int pz);
void remove_glolight(int px, int py, int pz);

// mesh.c protos
void mesh_region(int xlo, int xhi, int ylo, int yhi, int zlo, int zhi, unsigned char face_mask,
                 int origin_x, int origin_z);

// draw.c protos
int chunk_in_frustum(float *matrix, int chunk_x, int chunk_z);
int chunk_in_range(int chunk_x, int chunk_z);

// main.c protos
void recalc_corner_lighting(int xlo, int xhi, int zlo, int zhi);
void set_sunlight(int xlo, int ylo, int zlo, int light);
void set_glolight(int xlo, int ylo, int zlo, int light);
void rayshot(float eye0, float eye1, float eye2, float f0, float f1, float f2);
void move_to_ground(float *inout, int x, int y, int z);
void recalc_gndheight(int x, int z);
void scoot(int x, int z);
void auto_scoot();
void apply_scoot();

#endif // BLOCKO_DEFS_C_INCLUDED

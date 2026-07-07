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

#define RLEF 60
#define YLEF 61

#define BARR 64

#define LASTSOLID (BARR+1) // everything less than here is solid
#define OPEN 75            // empty space

// Blocks that faces should be drawn against (transparent or non-solid)
#define IS_SEE_THROUGH(t) ((t) >= OPEN || (t) == RLEF || (t) == YLEF)
#define WATR 76
#define LITE 77


#ifndef TERRAIN_THREAD
#define TERRAIN_THREAD 1           // whether to put terrain generation in its own thread
#endif

#define W 1920                     // window width, height
#define H 1000                     // ^
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
#define NR_PLAYERS 1
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
#define SUNQLEN 64000
#define GLOQLEN 64000

#define SHADOW_SZ 4096

// Shadow cascade indices
#define SHADOW_NEAR   0
#define SHADOW_MID    1
#define SHADOW_FAR_A  2
#define SHADOW_FAR_B  3
#define SHADOW_EXT_A  4
#define SHADOW_EXT_B  5
#define SHADOW_COUNT  6

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
// Fog colors match sky horizon colors from sky.frag
#define FOG_DAY_R 0.8f
#define FOG_DAY_G 0.85f
#define FOG_DAY_B 0.95f
#define FOG_DUSK_R 1.0f
#define FOG_DUSK_G 0.5f
#define FOG_DUSK_B 0.3f
#define FOG_NIGHT_R 0.05f
#define FOG_NIGHT_G 0.05f
#define FOG_NIGHT_B 0.1f

// tile pos-to-mem-location macros
#define T_(x,y,z)    tiles[    ((z - scootz) & (TILESD-1)) * (TILESH+0) * (TILESW+0) + ((x - scootx) & (TILESW-1)) * (TILESH+0) + (y)]
#define SUN_(x,y,z)  sunlight[ ((z - scootz) & (TILESD-1)) * (TILESH+0) * (TILESW+0) + ((x - scootx) & (TILESW-1)) * (TILESH+0) + (y)]
#define GLO_(x,y,z)  glolight[ ((z - scootz) & (TILESD-1)) * (TILESH+0) * (TILESW+0) + ((x - scootx) & (TILESW-1)) * (TILESH+0) + (y)]
#define CORN_(x,y,z) cornlight[((z - scootz) & (TILESD-1)) * (TILESH+1) * (TILESW+1) + ((x - scootx) & (TILESW-1)) * (TILESH+1) + (y)]
#define KORN_(x,y,z) kornlight[((z - scootz) & (TILESD-1)) * (TILESH+1) * (TILESW+1) + ((x - scootx) & (TILESW-1)) * (TILESH+1) + (y)]
#define GNDH_(x,z)   gndheight[((z - scootz) & (TILESD-1))              * (TILESW+0) + ((x - scootx) & (TILESW-1))                   ]

// for terrain/worker
#define TT_(x,y,z)    tiles[    ((z - tscootz) & (TILESD-1)) * (TILESH+0) * (TILESW+0) + ((x - tscootx) & (TILESW-1)) * (TILESH+0) + (y)]
#define TSUN_(x,y,z)  sunlight[ ((z - tscootz) & (TILESD-1)) * (TILESH+0) * (TILESW+0) + ((x - tscootx) & (TILESW-1)) * (TILESH+0) + (y)]
#define TGLO_(x,y,z)  glolight[ ((z - tscootz) & (TILESD-1)) * (TILESH+0) * (TILESW+0) + ((x - tscootx) & (TILESW-1)) * (TILESH+0) + (y)]
#define TCORN_(x,y,z) cornlight[((z - tscootz) & (TILESD-1)) * (TILESH+1) * (TILESW+1) + ((x - tscootx) & (TILESW-1)) * (TILESH+1) + (y)]
#define TKORN_(x,y,z) kornlight[((z - tscootz) & (TILESD-1)) * (TILESH+1) * (TILESW+1) + ((x - tscootx) & (TILESW-1)) * (TILESH+1) + (y)]
#define TGNDH_(x,z)   gndheight[((z - tscootz) & (TILESD-1))              * (TILESW+0) + ((x - tscootx) & (TILESW-1))                   ]

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
    float matrix[16];      // stored PV matrix (for A/B temporal blending)
    int slot;              // quantized slot (-1 = uninitialized)
    int polys;             // polygon counter per cascade
};
struct shadow_cascade shadow[SHADOW_COUNT];

VkSampler shadow_sampler;
VkRenderPass shadow_render_pass;
int shadow_pipe;

// Which shadow map to render this frame (0=A, 1=B, -1=skip)
int shadow_far_render_ab = 0;
int shadow_ext_render_ab = 0;

struct main_ubo {
    float model[16];              // mat4 - offset 0
    float view[16];               // mat4 - offset 64
    float proj[16];               // mat4 - offset 128
    float shadow_space[6][16];    // mat4[6] - offset 192 (near, mid, far_a, far_b, ext_a, ext_b)
    float bs;                     // float - offset 576
    float shadow_far_blend;       // float - offset 580 (far: 0=A, 1=B)
    float shadow_ext_blend;       // float - offset 584 (extreme: 0=A, 1=B)
    float padding1;               // Padding to align day_color to 16 bytes

    float day_color[3];   // vec3 - offset 592
    float padding2;       // Padding
    float glo_color[3];   // vec3 - offset 608
    float padding3;       // Padding
    float fog_color[3];   // vec3 - offset 624
    float fog_lo;         // float - offset 636
    float fog_hi;         // float - offset 640
    float padding4[3];    // Padding to align light_pos to 16 bytes
    float light_pos[3];   // vec3 - offset 656
    float padding5;       // Padding
    float view_pos[3];    // vec3 - offset 672
    float sharpness;      // float - offset 684
    int shadow_mapping;   // bool (as int) - offset 688
    float sun_strength;   // float - offset 692
    float sun_warmth;     // float - offset 696
    float outside_cascade_lit; // float - offset 700
    int water_frame;           // int   - offset 704
    float underwater;          // float - offset 708 (camera eye is in water)
} main_ubo;

unsigned int vbo[VAOS], vao[VAOS];
size_t vbo_len[VAOS];
size_t wbo_start[VAOS];

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

float night_amt;
float fog_r, fog_g, fog_b;

unsigned char *tiles;
unsigned char *sunlight;
unsigned char *glolight;
unsigned char gndheight[TILESW * TILESD];
float *cornlight;
float *kornlight;
struct chunk_stamp { int ax, az; };                  // absolute chunk coords a ring
volatile struct chunk_stamp chunk_stamp[VAOD][VAOW]; // slot holds (INT_MIN = never)
// pass 1 (chunk edge columns) stamps: a chunk's interior can generate only
// after its own and all 8 neighbors' edges exist, so nothing ever reads or
// writes outside its own chunk
volatile struct chunk_stamp chunk_estamp[VAOD][VAOW];
// transient marks so builder threads never work the same slot at once,
// guarded by the (chunks) critical section in chunk_builder
volatile char chunk_claim1[VAOD][VAOW]; // pass 1 running
volatile char chunk_claim2[VAOD][VAOW]; // pass 2 running
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
        int fvel;
        int rvel;
        int grav;
        int ground;
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
struct player camplayer;
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
unsigned world_seed = 60659;
int noisy = false;
int show_shadow_map = false;
int help_layer = 0;
int polys = 0;
int shadow_polys = 0;
int sunq_outta_room = 0;
int gloq_outta_room = 0;
int omp_threads = 0;
int lock_culling = false;
float cull_mtrx[16]; // camera frustum used for chunk culling - F2 freezes it
float cull_x, cull_z; // camera position for range culling, frozen with it
float draw_dist = 640.f;
int zooming = false;
float zoom_amt = 1.f;
float fast = 1.f;
int regulated = true;
int shadow_mapping = true;
int speedy_sun = false;
int reverse_sun = false;
float sun_pitch = .3f; //.6f; // 0 = east, PI/2 = up, PI = west, 3PI/2 = down
float moon_pitch;
float sun_yaw = .3f;
float sun_roll = -1.3f;
char alert[800]; // only for debugging

// test lock: automated test runs disable all input except the tilde
// console (unlock by typing "lock 0" there) and show a banner
int test_lock;
char test_lock_msg[256];
int remote_want_quit; // quit after the current reply is flushed
void game_shutdown(int code);
int main_pipe;     // main terrain rendering pipeline
int water_pipe;    // transparent water rendering pipeline

// GPU timestamp queries
#define GPU_TIMESTAMP_COUNT 8
VkQueryPool gpu_timestamp_pool;
uint64_t gpu_timestamps[GPU_TIMESTAMP_COUNT];
float gpu_timestamp_period;  // nanoseconds per tick
int gpu_timestamps_valid = 0;  // whether we have valid results from previous frame
// Indices: 0=frame_start, 1=shadow_end, 2=terrain_end, 3=frame_end
enum {
    GPU_TS_FRAME_START = 0,
    GPU_TS_SHADOW_N_END,
    GPU_TS_SHADOW_M_END,
    GPU_TS_SHADOW_F_END,
    GPU_TS_SHADOW_X_END,
    GPU_TS_TERRAIN_END,
    GPU_TS_FRAME_END,
    GPU_TS_COUNT
};

int mouselook = false; // start with the mouse free; click the window to capture
int target_x, target_y, target_z;
int place_x, place_y, place_z;
int screenw = W;
int screenh = H;
volatile struct qitem just_generated[VAOW*VAOD];
volatile size_t just_gen_len;

int nr_chunks_generated = 0;
int nr_meshes_built = 0;
int chunk_gen_ticks = 0;
int cave_enable = 1;
int tree_enable = 1;
// per-pass gen_chunk wall time, reported by the fps socket command
enum { GEN_HMAP, GEN_SOIL, GEN_CAVES, GEN_WATER, GEN_TREES, GEN_LIGHT, GEN_CORNERS, GEN_PASSES };
int gen_pass_ms[GEN_PASSES];

// glsetup.c protos
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

// collision.c protos
int collide(struct box l, struct box r);
int world_collide(struct box box, int wet);

// test.c protos
void debrief();

// blocklight.c protos
void sun_enqueue(int x, int y, int z, int base, unsigned char incoming_light);
void glo_enqueue(int x, int y, int z, int base, unsigned char incoming_light);
int step_sunlight();
int step_glolight();
void remove_sunlight(int px, int py, int pz);
void remove_glolight(int px, int py, int pz);

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
void remote_init();
void remote_poll();

#endif // BLOCKO_DEFS_C_INCLUDED

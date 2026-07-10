// ============================================================================
// terrain_knobs.h — the big table of runtime-tunable terrain knobs
// ============================================================================
//
// Every tunable in the terrain pipeline is one row here:
//
//     TWEAK(name, default, lo, hi, flags, "what it does")
//
// The list expands in two places: terrain.c defines one float variable per
// row (so the generator reads live values), and blocko's tweak.c builds the
// in-game tweaker panel and the `tweak` socket command from the same rows.
// Seeds, salts, cache sizes and array dimensions stay compile-time in
// terrain_config.c / warp_config.c / ledge_config.c.
//
// The pipeline: get_height() builds a base height field, then presses
// plateaus and oceans into it. terrain_raw_height() domain-warps the lookup
// and adds, in order: peaks, mounds, lumps, pits, a smoothing pass, the broad
// mountain ranges, and a ceiling bounce that folds over-tall crests into
// calderas. get_filtered_height() lays ledges on top.
//
// The game maps height 0.5 to sea level and ~1.56 to just below Y=0, so
// height knobs read against that scale.
//
// No include guard on the rows: the includer defines TWEAK/TWEAK_SECTION
// (and TWEAK_VAR for gen_knobs.h) and may include this file more than once.
//
// flags: TW_INT = whole numbers, TW_LOG = multiplicative stepping in the UI

#ifndef TW_INT
#define TW_INT 1
#define TW_LOG 2
#endif

// --- base height field (get_height) -----------------------------------------
// Five noise octaves summed with a constant bias make the raw continent shape.
// Weights roughly halve each step: O1 sets the broad shape, O5 the fine grain.
TWEAK_SECTION("base height field")
TWEAK(BASE_BIAS,   0.05f, -0.5f,   0.5f, 0,             "constant floor added to every sample")
TWEAK(BASE_O1_SZ,  1300,   50, 20000, TW_INT|TW_LOG,    "octave 1 feature size in blocks (broad shape)")
TWEAK(BASE_O1_WT,  0.60f,  0.f,   1.5f, 0,              "octave 1 weight")
TWEAK(BASE_O1_OCT, 2,      1,     8,   TW_INT,          "octave 1 internal fBm octaves")
TWEAK(BASE_O2_SZ,  800,    50, 20000, TW_INT|TW_LOG,    "octave 2 feature size")
TWEAK(BASE_O2_WT,  0.30f,  0.f,   1.5f, 0,              "octave 2 weight")
TWEAK(BASE_O2_OCT, 1,      1,     8,   TW_INT,          "octave 2 internal fBm octaves")
TWEAK(BASE_O3_SZ,  400,    50, 20000, TW_INT|TW_LOG,    "octave 3 feature size")
TWEAK(BASE_O3_WT,  0.15f,  0.f,   1.5f, 0,              "octave 3 weight")
TWEAK(BASE_O3_OCT, 1,      1,     8,   TW_INT,          "octave 3 internal fBm octaves")
TWEAK(BASE_O4_SZ,  200,    25, 20000, TW_INT|TW_LOG,    "octave 4 feature size")
TWEAK(BASE_O4_WT,  0.08f,  0.f,   1.5f, 0,              "octave 4 weight")
TWEAK(BASE_O4_OCT, 1,      1,     8,   TW_INT,          "octave 4 internal fBm octaves")
TWEAK(BASE_O5_SZ,  100,    25, 20000, TW_INT|TW_LOG,    "octave 5 feature size (fine grain)")
TWEAK(BASE_O5_WT,  0.04f,  0.f,   1.5f, 0,              "octave 5 weight")
TWEAK(BASE_O5_OCT, 1,      1,     8,   TW_INT,          "octave 5 internal fBm octaves")

// --- plateau / shelf pass (get_height) --------------------------------------
// Inside plateau regions the smooth height is quantized onto evenly spaced
// flat shelves joined by short cliffs (mesa/terrace country). Terracing is
// average-preserving; a low-frequency phase drifts shelf heights region to
// region and a medium-frequency jitter bends the cliff lines.
TWEAK_SECTION("plateaus / mesas")
TWEAK(PLATEAU_ENABLE,     1,      0,     1,    TW_INT,        "0 skips the whole plateau/shelf pass")
TWEAK(PLATEAU_MASK_SZ,    2500,   200, 20000, TW_INT|TW_LOG,  "size of a plateau region in blocks")
TWEAK(PLATEAU_MASK_LO,    0.45f,  0.f,   1.f,  0,             "mask <= this -> no terracing")
TWEAK(PLATEAU_MASK_HI,    0.52f,  0.f,   1.f,  0,             "mask >= this -> full terracing")
TWEAK(PLATEAU_STEP,       0.075f, 0.01f, 0.3f, 0,             "shelf spacing in height units (0.075 ~ 12 blocks)")
TWEAK(PLATEAU_RISER,      0.35f,  0.05f, 1.f,  0,             "cliff fraction of each step; smaller = crisper cliffs")
TWEAK(PLATEAU_PHASE_SZ,   1500,   200, 20000, TW_INT|TW_LOG,  "shelf-height drift region size (large = flat mesas)")
TWEAK(PLATEAU_PHASE_LO,   0.32f,  0.f,   1.f,  0,             "phase remap window low (noise 5th percentile)")
TWEAK(PLATEAU_PHASE_HI,   0.68f,  0.f,   1.f,  0,             "phase remap window high")
TWEAK(PLATEAU_JITTER_SZ,  440,    50,  5000,  TW_INT|TW_LOG,  "cliff-line bend size; between tread width and PHASE_SZ")
TWEAK(PLATEAU_JITTER_OCT, 2,      1,     4,    TW_INT,        "jitter octaves: a big bend plus a finer wobble")
TWEAK(PLATEAU_JITTER_AMP, 0.40f,  0.f,   2.f,  0,             "cliff-line wander in steps (0 = straight bands)")

// --- big oceans (get_height) ------------------------------------------------
// A very low-frequency mask presses lowlands below sea level (0.5); mountain
// passes run afterward and can still raise islands out of the water.
TWEAK_SECTION("oceans")
TWEAK(OCEAN_MASK_SZ,      9000,  500, 40000, TW_INT|TW_LOG,   "ocean region size in blocks")
TWEAK(OCEAN_MASK_LO,      0.53f, 0.f,   1.f, 0,               "mask <= this -> dry land")
TWEAK(OCEAN_MASK_HI,      0.61f, 0.f,   1.f, 0,               "mask >= this -> full ocean")
TWEAK(OCEAN_FLOOR_BASE,   0.38f, 0.f,   0.5f, 0,              "deepest the sea bed is pressed to")
TWEAK(OCEAN_FLOOR_RELIEF, 0.10f, 0.f,   1.f, 0,               "how much original relief survives on the bed")

// --- peaks: isolated blobby mountains + calderas + stacks -------------------
// A mid-scale mask raises scattered massifs; above THRESH the excess is
// scaled by GAIN into height. Calderas bite dimples, stacks add spikes.
TWEAK_SECTION("peaks / massifs")
TWEAK(PEAK_MASK_SZ,       1200,  100, 10000, TW_INT|TW_LOG,   "massif region size in blocks")
TWEAK(PEAK_THRESH,        0.7f,  0.5f,  1.f, 0,               "mask must exceed this to raise a peak")
TWEAK(PEAK_GAIN,          3.0f,  0.f,  10.f, 0,               "multiplies the excess above THRESH")
TWEAK(PEAK_CALD_SZ,       140,   25,  2000, TW_INT|TW_LOG,    "caldera dimple size")
TWEAK(PEAK_CALD_THRESH,   0.59f, 0.5f,  1.f, 0,               "caldera bites in past this (both polarities)")
TWEAK(PEAK_STACK_SZ,      205,   25,  2000, TW_INT|TW_LOG,    "spiky stack size")
TWEAK(PEAK_STACK_THRESH,  0.61f, 0.5f,  1.f, 0,               "spiky over-height appears past this")

// --- mounds / lumps / pits: small threshold-noise bumps ---------------------
// Where a noise field exceeds THRESH, the excess is added (mounds, lumps) or
// subtracted (pits), dotting the surface with rises and hollows.
TWEAK_SECTION("mounds / lumps / pits")
TWEAK(MOUND_SZ,     290,   25,  5000, TW_INT|TW_LOG,          "mound feature size")
TWEAK(MOUND_THRESH, 0.65f, 0.5f,  1.f, 0,                     "mound noise threshold (higher = fewer)")
TWEAK(LUMP_SZ,      175,   25,  5000, TW_INT|TW_LOG,          "lump feature size")
TWEAK(LUMP_THRESH,  0.65f, 0.5f,  1.f, 0,                     "lump noise threshold")
TWEAK(PIT_SZ,       430,   25,  5000, TW_INT|TW_LOG,          "pit feature size")
TWEAK(PIT_THRESH,   0.65f, 0.5f,  1.f, 0,                     "pit noise threshold")

// --- smoothouts: local box blur ---------------------------------------------
// Where the mask exceeds THRESH, height becomes the average of four samples
// offset by up to MAX_SPREAD blocks, softening terrain into rolling basins.
TWEAK_SECTION("smoothouts")
TWEAK(SMOOTH_SZ,         990,    50, 10000, TW_INT|TW_LOG,    "smoothed-region size")
TWEAK(SMOOTH_THRESH,     0.65f,  0.5f,  1.f, 0,               "smoothing kicks in past this")
TWEAK(SMOOTH_MAX_SPREAD, 1000.f, 0.f, 5000.f, 0,              "sample offset in blocks at full strength")

// --- mountain ranges: broad linear cordilleras ------------------------------
// The RANGE_MASK picks range regions (shared with the ledge pass, so ledges
// land on the ranges). Inside, two octaves of folded ridged noise carve
// crests and valleys; amplitude rides the mask so ranges cross oceans as
// island chains.
TWEAK_SECTION("mountain ranges")
TWEAK(RANGE_MASK_SZ,  4000,  500, 20000, TW_INT|TW_LOG,       "range-region size (shared gate for ledges)")
TWEAK(RANGE_MASK_LO,  0.60f, 0.f,   1.f, 0,                   "mask <= this -> no range here")
TWEAK(RANGE_MASK_HI,  0.72f, 0.f,   1.f, 0,                   "mask >= this -> full range strength")
TWEAK(RANGE_R1_SZ,    700,   50, 10000, TW_INT|TW_LOG,        "broad ridgeline feature size")
TWEAK(RANGE_R2_SZ,    250,   25, 10000, TW_INT|TW_LOG,        "fine ridge detail feature size")
TWEAK(RANGE_AMP_BASE, 0.06f, 0.f,  0.5f, 0,                   "minimum lift anywhere in a range")
TWEAK(RANGE_R1_WT,    0.85f, 0.f,   2.f, 0,                   "weight of the broad ridgelines")
TWEAK(RANGE_R2_WT,    0.30f, 0.f,   2.f, 0,                   "weight of the finer ridge detail")

// --- ceiling bounce: fold over-tall crests into calderas --------------------
// Crests rising past a gently wandering ceiling fold back down (a raised rim
// ringing a sunken crater) instead of pancaking flat.
TWEAK_SECTION("ceiling bounce")
TWEAK(CEIL_BASE,       1.53f, 0.5f,  2.f, 0,                  "ceiling height (~Y10); 1.56 is just below Y=0")
TWEAK(CEIL_WANDER_AMP, 0.11f, 0.f,  0.5f, 0,                  "how far the ceiling wanders")
TWEAK(CEIL_WANDER_SZ,  300,   50,  5000, TW_INT|TW_LOG,       "ceiling wander feature size")
TWEAK(CEIL_MAX_CRATER, 0.38f, 0.f,   1.f, 0,                  "fold depth clamp so floors stay up the mountain")

// --- scattered domain warp (terrain_raw_height) ------------------------------
// Spirals twist the sampling grid into whorls, bubbles bulge or pinch it.
// Cells of WARP_CELL blocks each hold up to WARP_MAX key points, gated by a
// density field. INVARIANT: WARP_R_MAX must stay below WARP_CELL (1024) or
// warps clip at cell borders.
TWEAK_SECTION("domain warps")
TWEAK(WARP_DENSITY_SZ,      8000,  500, 40000, TW_INT|TW_LOG, "warped/calm region size")
TWEAK(WARP_DENSITY_LO,      0.48f, 0.f,   1.f, 0,             "density <= this -> cell gets no warps")
TWEAK(WARP_DENSITY_HI,      0.70f, 0.f,   1.f, 0,             "density >= this -> cell fills all slots")
TWEAK(WARP_R_MIN,           250.f, 10.f, 900.f, 0,            "smallest warp radius in blocks")
TWEAK(WARP_R_MAX,           900.f, 10.f, 1000.f, 0,           "largest warp radius (KEEP < 1024 = WARP_CELL)")
TWEAK(WARP_BUBBLE_FRAC,     0.35f, 0.f,   1.f, 0,             "share of key points that are bubbles (rest spirals)")
TWEAK(WARP_SPIRAL_STR_MIN,  0.4f,  0.f,   3.f, 0,             "spiral twist strength low end (radians-ish)")
TWEAK(WARP_SPIRAL_STR_MAX,  0.9f,  0.f,   3.f, 0,             "spiral twist strength high end")
TWEAK(WARP_SPIRAL_ANG,      10.f,  0.f,  30.f, 0,             "spiral angle-noise remaps to 0..this")
TWEAK(WARP_SPIRAL_NOISE_SZ, 1080,  50, 10000, TW_INT|TW_LOG,  "spiral angle-variation noise size")
TWEAK(WARP_BUBBLE_STR_MIN,  3.f,   0.f,  20.f, 0,             "bubble bulge/pinch strength low end")
TWEAK(WARP_BUBBLE_STR_MAX,  7.f,   0.f,  20.f, 0,             "bubble bulge/pinch strength high end")

// --- ledges & plateaus on the ranges (get_filtered_height) -------------------
// Scattered key points inside mountain ranges pull the ground toward the key
// point's natural height, carving flat standable shelves. INVARIANT: reach =
// LEDGE_R_MAX * (LEDGE_RAD_BASE + LEDGE_WOB_MAX) must stay below LEDGE_CELL
// (192) or ledges clip at cell borders (80 * (0.25 + 2.1) = 188 < 192).
TWEAK_SECTION("mountain ledges")
TWEAK(LEDGE_DENSITY_SZ, 5000,  500, 40000, TW_INT|TW_LOG,     "dense/bare ledge region size")
TWEAK(LEDGE_DENSITY_LO, 0.45f, 0.f,   1.f, 0,                 "density <= this -> cell gets no ledges")
TWEAK(LEDGE_DENSITY_HI, 0.68f, 0.f,   1.f, 0,                 "density >= this -> cell fills all slots")
TWEAK(LEDGE_R_MIN,      5.f,   1.f,  80.f, 0,                 "smallest ledge radius in blocks")
TWEAK(LEDGE_R_MAX,      80.f,  1.f,  80.f, 0,                 "largest ledge radius (KEEP reach < 192, see above)")
TWEAK(LEDGE_CORE_MIN,   0.30f, 0.f,   1.f, 0,                 "smallest flat-top fraction of the radius")
TWEAK(LEDGE_CORE_MAX,   0.70f, 0.f,   1.f, 0,                 "largest flat-top fraction")
TWEAK(LEDGE_ASPECT_MIN, 1.0f,  1.f,   8.f, 0,                 "min stretch: 1 = round")
TWEAK(LEDGE_ASPECT_MAX, 3.5f,  1.f,   8.f, 0,                 "max stretch: long narrow ridge")
TWEAK(LEDGE_WOB_MIN,    0.7f,  0.f,  2.1f, 0,                 "gentlest rim wobble (near-elliptical)")
TWEAK(LEDGE_WOB_MAX,    2.1f,  0.f,  2.1f, 0,                 "wildest rim wobble (pinched, can split the shelf)")
TWEAK(LEDGE_RAD_BASE,   0.25f, 0.05f, 1.f, 0,                 "rim radius floor as a fraction of R, before wobble")
TWEAK(LEDGE_WOB1_FREQ,  1.3f,  0.1f,  4.f, 0,                 "rim octave 1 size = R * this (broad lobes)")
TWEAK(LEDGE_WOB1_WT,    0.6f,  0.f,   1.f, 0,                 "rim octave 1 blend weight (the two sum ~1)")
TWEAK(LEDGE_WOB2_FREQ,  0.5f,  0.1f,  4.f, 0,                 "rim octave 2 size = R * this (fine detail)")
TWEAK(LEDGE_WOB2_WT,    0.4f,  0.f,   1.f, 0,                 "rim octave 2 blend weight")

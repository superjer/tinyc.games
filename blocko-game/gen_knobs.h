// ============================================================================
// gen_knobs.h — blocko's rows of the big tweak table
// ============================================================================
//
// Runtime-tunable knobs for the parts of world generation that live in blocko
// itself: the soil/vegetation/cave/tree passes in chunker.c, the rock
// formations in formations.c, and the taylor-noise algorithm knobs. Same row
// format as common/tinyc.games/terrain_knobs.h:
//
//     TWEAK(name, default, lo, hi, flags, "what it does")
//
// defs.c expands TWEAK rows into float variable definitions; tweak.c expands
// both this file and terrain_knobs.h into the in-game tweaker panel and the
// `tweak` socket command.
//
// TWEAK_VAR rows register a variable that is *defined elsewhere* (the noise_*
// knobs live in taylor_noise.c, shared with other games); the default given
// here must match the definition's initializer — it's what Backspace/reset
// restores.
//
// Heads-up on the y axis: world y grows downward, so "blocks above sea level"
// means smaller y. SEA_LEVEL is TILESH - 80.

#ifndef TW_INT
#define TW_INT 1
#define TW_LOG 2
#endif

// --- soil bands (gen_columns) ------------------------------------------------
// Solid ground is constant runs between four wobbling levels: stone (exposed
// on high mountains), two dirt bands, a sand shelf around the waterline, and
// deep granite. Offsets are y relative to sea level: negative = higher.
TWEAK_SECTION("soil bands")
TWEAK(SOIL_WOB_SZ,    350,   25,  5000, TW_INT|TW_LOG, "feature size of the band-boundary wobble")
TWEAK(SOIL_WOB_DEEP,  100,    0,   300, TW_INT,        "wobble amplitude of the deep boundaries (blocks)")
TWEAK(SOIL_WOB_SHAL,  50,     0,   300, TW_INT,        "wobble amplitude of the sand/granite boundaries")
TWEAK(SOIL_LEV1_OFF, -60,  -150,   150, TW_INT,        "mountain stone ends: y offset from sea level")
TWEAK(SOIL_LEV2_OFF, -30,  -150,   150, TW_INT,        "upper/lower dirt boundary: y offset from sea level")
TWEAK(SOIL_LEV3_OFF,  0,   -150,   150, TW_INT,        "dirt gives way to the sand shelf: y offset")
TWEAK(SOIL_LEV4_OFF,  45,  -150,   150, TW_INT,        "sand/stone give way to deep granite: y offset")
TWEAK(MESA_CLIFF_PLAT, 0.60f, 0.f, 1.f, 0,             "min plateauness for terrace risers to show bare stone")

// --- vegetation lines (gen_columns) ------------------------------------------
// Ordinary grass gives way to mountain grass at MTN_LINE and to bare rock at
// BARREN, both in blocks above sea level, ragged-edged by a fine noise.
TWEAK_SECTION("vegetation lines")
TWEAK(VEG_RAG_SZ,      40,  10,  500, TW_INT|TW_LOG,   "feature size of the ragged edge on both lines")
TWEAK(VEG_MTN_LINE,    55,   0,  160, TW_INT,          "grass -> mountain grass, blocks above sea level")
TWEAK(VEG_MTN_RAG,     16,   0,   60, TW_INT,          "ragged-edge amplitude of the mountain grass line")
TWEAK(VEG_BARREN,      98,   0,  160, TW_INT,          "mountain grass -> bare rock, blocks above sea level")
TWEAK(VEG_BARREN_RAG,  26,   0,   60, TW_INT,          "ragged-edge amplitude of the barren line")

// --- ore (gen_columns) ---------------------------------------------------------
// A positional hash shared across each 2x2x2 cell clumps hits into veins.
TWEAK_SECTION("ore")
TWEAK(ORE_VEIN_PCT, 5, 0, 100, TW_INT,                 "chance per stone cell of a real ore vein (%)")
TWEAK(ORE_HINT_PCT, 7, 0, 100, TW_INT,                 "chance of a hint of ore in the rock (%)")

// --- caves (gen_columns) -------------------------------------------------------
// Each region grows 0..CAVE_MAX bezier curves random-walked from its center,
// carved as spheres of varying radius.
TWEAK_SECTION("caves")
TWEAK(CAVE_MAX,         12,  0,   40, TW_INT,          "most cave curves per region (rolls 0..this)")
TWEAK(CAVE_STRETCH_MIN, 10,  1,  200, TW_INT,          "cave system stretchiness, low roll")
TWEAK(CAVE_STRETCH_MAX, 60,  1,  200, TW_INT,          "cave system stretchiness, high roll")
TWEAK(CAVE_RAD_SCALE,   50.f, 1.f, 200.f, 0,           "cave radius^2 scale and clamp (bigger = fatter caves)")

// --- trees (gen_chunk_pass2) ---------------------------------------------------
// A low-frequency noise picks forest regions; inside one, trees chain-spawn
// with probability PCT until a roll fails.
TWEAK_SECTION("trees")
TWEAK(TREE_REGION_SZ,  1300, 100, 10000, TW_INT|TW_LOG, "forest region feature size")
TWEAK(TREE_DENSE_T,    0.51f, 0.f, 1.f, 0,             "region noise above this = dense forest")
TWEAK(TREE_DENSE_PCT,  96,    0,  99, TW_INT,          "chained tree chance in dense forest (%)")
TWEAK(TREE_SPARSE_T,   0.45f, 0.f, 1.f, 0,             "region noise above this = sparse forest")
TWEAK(TREE_SPARSE_PCT, 85,    0,  99, TW_INT,          "chained tree chance in sparse forest (%)")

// --- tall grass (gen_chunk_pass2) ----------------------------------------------
// A low-frequency field carves shaggy patches; inside one, surface grass
// sprouts blades with a chance that grows toward the patch center.
TWEAK_SECTION("tall grass")
TWEAK(SHAG_PATCH_SZ, 90,    10, 1000, TW_INT|TW_LOG,   "patch field feature size")
TWEAK(SHAG_PATCH_T,  0.5f,  0.f,  1.f, 0,              "patch noise above this sprouts tall grass")
TWEAK(SHAG_RAMP,     0.32f, 0.01f, 1.f, 0,             "how fast density rises past the threshold")
TWEAK(SHAG_MAX_PCT,  88,    0,  100, TW_INT,           "peak blade chance at patch center (%)")

// --- rock formations (formations.c) --------------------------------------------
// Overhanging crags: a metaball scaffold, carved by 3D noise, kept grounded
// by a flood fill. See formations.c for the full story.
TWEAK_SECTION("rock formations")
TWEAK(form_enable, 1,     0,    1,  TW_INT,            "0 turns formations off")
TWEAK(form_region, 0.56f, 0.f,  1.f, 0,                "region mask threshold (lower = more regions)")
TWEAK(form_chance, 0.50f, 0.f,  1.f, 0,                "per-cell spawn chance inside a region")
TWEAK(form_steps,  12,    6,   30,  TW_INT,            "max steps in the scaffold trunk walk")
TWEAK(form_rmin,   3.f,   1.f, 20.f, 0,                "smallest scaffold sphere radius")
TWEAK(form_rmax,   15.f,  1.f, 25.f, 0,                "largest scaffold sphere radius")
TWEAK(form_detail, 0,     0,    1,  TW_INT,            "fbm grit shell: subtle, ~2x gen cost")
TWEAK(FORM_MBALL_T,    0.32f,  0.05f,  1.f,  0,        "metaball isosurface threshold (lower = fatter)")
TWEAK(FORM_BITE_FREQ,  0.045f, 0.005f, 0.2f, 0,        "concave-bay noise frequency")
TWEAK(FORM_BITE_T,     0.53f,  0.f,    1.f,  0,        "bays gouge in past this (lower = more eroded)")
TWEAK(FORM_CRACK_FREQ, 0.075f, 0.005f, 0.2f, 0,        "crack/arch ridged-noise frequency")
TWEAK(FORM_CRACK_T,    0.60f,  0.f,    1.f,  0,        "cracks cut in past this")
TWEAK(FORM_WARP_AMP,   7.0f,   0.f,   20.f,  0,        "carve-field domain warp amplitude")
TWEAK(FORM_WARP_FREQ,  0.018f, 0.001f, 0.1f, 0,        "carve-field domain warp frequency")
TWEAK(FORM_CARVE_OCT,  2,      1,      4,   TW_INT,    "carve noise octaves")
TWEAK(FORM_DTL_FREQ,   0.15f,  0.01f,  0.5f, 0,        "grit shell noise frequency")
TWEAK(FORM_DTL_T,      0.47f,  0.f,    1.f,  0,        "grit shell threshold")
TWEAK(FORM_DTL_OCT,    3,      1,      4,   TW_INT,    "grit shell noise octaves")

// --- noise algorithm (taylor_noise.c) -------------------------------------------
// Knobs on the noise function itself, so every field above changes character.
// Defined in taylor_noise.c (shared with other games) — registered here.
TWEAK_SECTION("noise algorithm")
TWEAK_VAR(noise_kernel_sq,   0,   0,   1, TW_INT,      "squared falloff: smoother blobs, no edge crease")
TWEAK_VAR(noise_base_weight, 1.f, 0.f, 3.f, 0,         "phantom 0.5 feature weight; lower = more contrast")
TWEAK_VAR(noise_aniso,       0.f, 0.f, 1.f, 0,         "stretch features into oriented ridges")
TWEAK_VAR(noise_nvary,       0,   0,   1, TW_INT,      "vary feature count per cell (lumpier fields)")
TWEAK_VAR(noise_interp,      1,   0,   1, TW_INT,      "lattice + bilinear shortcut (0 = exact, slow)")

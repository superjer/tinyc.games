// ============================================================================
// gen_knobs.h — blocko's world-gen tunables
// ============================================================================
//
// Knobs for the parts of world generation that live in blocko itself: the
// soil/vegetation/tree passes in chunker.c. Same row format as
// common/tinyc.games/terrain_knobs.h:
//
//     TWEAK(name, default, lo, hi, flags, "what it does")
//
// defs.c expands TWEAK rows into float variable definitions.
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
TWEAK(MESA_CLIFF_TERRACE, 0.60f, 0.f, 1.f, 0,          "min terraceness for terrace risers to show bare stone")

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

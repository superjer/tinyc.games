// ============================================================================
// warp_config.c — tuning knobs for the scattered domain-warp features
// ============================================================================
//
// Before sampling the base terrain height, terrain_raw_height() (in terrain.c)
// warps the lookup coordinate with scattered "key points":
//   * spirals twist the sampling grid into whorls, and
//   * bubbles bulge it outward (or pinch it inward) into blisters.
// This is what makes swirled, non-tectonic landforms. It used to be three
// hand-placed features at fixed spots; now they are seeded across the world on
// a grid, exactly like the ledges (see ledge_config.c).
//
// The world is diced into WARP_CELL-sized cells. A low-frequency density field
// decides, per cell, how many of up to WARP_MAX key points appear, so warps
// cluster in some regions and leave others untouched. Each key point draws a
// random position, radius, kind (spiral or bubble), strength and spin.
//
// Every active key point near a sample adds its displacement, then the base
// height is read at the summed, warped coordinate; everything downstream keeps
// using the true (x,y).
//
// INVARIANT — do not break: a warp reaches WARP_R_MAX blocks from its key
// point, and a sample only searches the 3x3 cells around itself, so WARP_R_MAX
// MUST stay below WARP_CELL or warps get clipped at cell borders.
//     900 < 1024   ✓

#ifndef TINYCGAMES_WARP_CONFIG_C_INCLUDED
#define TINYCGAMES_WARP_CONFIG_C_INCLUDED

// --- grid & how many warps per cell -----------------------------------------
#define WARP_CELL        1024.f // cell size in blocks; also the warp spacing
#define WARP_MAX         3      // most key points one cell can hold (0..this)

// --- density field (which regions get warped) -------------------------------
// Low-frequency noise per cell, remapped to a 0..1 fill probability; each slot
// appears with that probability. Lower _LO or widen _LO.._HI for more warps;
// raise _SZ for larger, smoother warped/calm regions.
#define WARP_DENSITY_SZ  8000   // noise feature size (bigger = broader regions)
#define WARP_DENSITY_LO  0.48f  // noise <= this  -> cell gets no warps
#define WARP_DENSITY_HI  0.70f  // noise >= this  -> cell fills all WARP_MAX slots

// --- per-warp size ----------------------------------------------------------
#define WARP_R_MIN       250.f  // smallest warp radius (blocks)
#define WARP_R_MAX       900.f  // largest warp radius (must stay < WARP_CELL)

// --- kind mix ---------------------------------------------------------------
#define WARP_BUBBLE_FRAC 0.35f  // share of key points that are bubbles (rest spirals)

// --- spiral: twists the sampling grid into a whorl --------------------------
// The rotation angle is  strength * (1-dist)^3 * noise, and the spin direction
// (sign) is randomized per key point. STR is the base angle scale in radians.
#define WARP_SPIRAL_STR_MIN  0.4f
#define WARP_SPIRAL_STR_MAX  0.9f
#define WARP_SPIRAL_ANG      10.f  // angle-noise remaps to 0..this
#define WARP_SPIRAL_NOISE_SZ 1080  // feature size of the angle-variation noise

// --- bubble: bulges (or, with a negative sign, pinches) the grid in a ring --
// The outward push peaks at mid-radius and fades to nothing at center and rim.
// STR scales the push; the sign is randomized so some bulge and some pinch.
#define WARP_BUBBLE_STR_MIN  3.f
#define WARP_BUBBLE_STR_MAX  7.f

// --- internals (rarely need touching) ---------------------------------------
#define WARP_CACHE        1024      // per-thread cell cache slots (power of two)
#define WARP_SALT_CELL    0x3a17c0
#define WARP_SALT_DENSITY 0x9b0bb1
#define WARP_SALT_NOISE   0x5c17e1

#endif // TINYCGAMES_WARP_CONFIG_C_INCLUDED

// ============================================================================
// warp_config.c — compile-time constants for the scattered domain-warp pass
// ============================================================================
//
// The tunable warp knobs (density, radii, strengths) moved to terrain_knobs.h
// — the big table behind blocko's in-game tweaker. What stays here is the
// grid geometry and hash salts, which size arrays and caches and so must be
// compile-time.
//
// INVARIANT — do not break: a warp reaches WARP_R_MAX blocks from its key
// point, and a sample only searches the 3x3 cells around itself, so
// WARP_R_MAX MUST stay below WARP_CELL or warps get clipped at cell borders.

#ifndef TINYCGAMES_WARP_CONFIG_C_INCLUDED
#define TINYCGAMES_WARP_CONFIG_C_INCLUDED

#define WARP_CELL        1024.f // cell size in blocks; also the warp spacing
#define WARP_MAX         3      // most key points one cell can hold (0..this)
#define WARP_CACHE       1024   // per-thread cell cache slots (power of two)

#define WARP_SALT_CELL    0x3a17c0
#define WARP_SALT_DENSITY 0x9b0bb1
#define WARP_SALT_NOISE   0x5c17e1

#endif // TINYCGAMES_WARP_CONFIG_C_INCLUDED

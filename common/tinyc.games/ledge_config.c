// ============================================================================
// ledge_config.c — compile-time constants for the mountain-range ledge pass
// ============================================================================
//
// The tunable ledge knobs (density, radii, wobble, aspect) moved to
// terrain_knobs.h — the big table behind blocko's in-game tweaker. What stays
// here is the grid geometry, array dimensions and hash salts.
//
// INVARIANT — do not break this: the farthest a ledge can reach from its key
// point is  LEDGE_R_MAX * (LEDGE_RAD_BASE + LEDGE_WOB_MAX). A height query
// only searches the 3x3 cells around itself, so that reach MUST stay below
// LEDGE_CELL or ledges get clipped at cell borders. With the defaults:
//     80 * (0.25 + 2.1) = 188  <  192   ✓
// The table's hi bounds keep this true; mind it if you loosen them.

#ifndef TINYCGAMES_LEDGE_CONFIG_C_INCLUDED
#define TINYCGAMES_LEDGE_CONFIG_C_INCLUDED

#define LEDGE_CELL       192.f  // cell size in blocks; also the ledge spacing
#define LEDGE_MAX        8      // most key points one cell can hold (0..this)
#define LEDGE_CACHE      2048   // per-thread cell cache slots (power of two)

#define LEDGE_RAD_FLOOR  0.5f   // absolute min rim radius in blocks (avoids /0)
#define LEDGE_WOB1_FLOOR 8      // rim-noise min feature size, octave 1
#define LEDGE_WOB2_FLOOR 5      // rim-noise min feature size, octave 2

#define LEDGE_SLOT_SPREAD 16         // > LEDGE_MAX, keeps per-slot rim seeds distinct
#define LEDGE_TWO_PI      6.2831853f // random orientation is 0..2pi
// distinct hash salts so the independent per-cell draws don't correlate
#define LEDGE_SALT_CELL    0x1ed9e5
#define LEDGE_SALT_KP      0x513a9e
#define LEDGE_SALT_DENSITY 0x0de5a1
#define LEDGE_SALT_WOB2    0x2b7

#endif // TINYCGAMES_LEDGE_CONFIG_C_INCLUDED

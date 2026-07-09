// ============================================================================
// ledge_config.c — tuning knobs for the mountain-range ledge/plateau pass
// ============================================================================
//
// The ledge pass (get_filtered_height in terrain.c) scatters invisible "key
// points" across mountain-range terrain and pulls the surrounding ground
// toward each key point's natural height, carving flat shelves you can stand
// on. Every knob below is safe to tweak; each notes what it does.
//
// How it fits together: the world is diced into square cells LEDGE_CELL blocks
// on a side. A low-frequency density field decides, per cell, how many of up
// to LEDGE_MAX key points actually appear — so whole regions come out crowded
// with shelves, lightly dotted, or bare. Each key point then draws a random
// size, flat-top fraction, orientation, stretch and wobbly outline from the
// ranges below, so no two ledges look alike.
//
// INVARIANT — do not break this: the farthest a ledge can reach from its key
// point is  LEDGE_R_MAX * (LEDGE_RAD_BASE + LEDGE_WOB_MAX). A height query only
// searches the 3x3 cells around itself, so that reach MUST stay below
// LEDGE_CELL or ledges get clipped at cell borders. With the defaults:
//     80 * (0.25 + 2.1) = 188  <  192   ✓
// If you raise LEDGE_R_MAX / LEDGE_WOB_MAX or lower LEDGE_CELL, keep it true.

#ifndef TINYCGAMES_LEDGE_CONFIG_C_INCLUDED
#define TINYCGAMES_LEDGE_CONFIG_C_INCLUDED

// --- grid & how many ledges per cell ----------------------------------------
#define LEDGE_CELL       192.f  // cell size in blocks; also the ledge spacing
#define LEDGE_MAX        8      // most key points one cell can hold (0..this)

// --- density field (how crowded each region is) -----------------------------
// A low-frequency noise sampled per cell, remapped to a 0..1 fill probability;
// each of the LEDGE_MAX slots appears with that probability. Lower _LO or widen
// the _LO.._HI window for more ledges overall; raise _SZ for larger, smoother
// dense/bare regions.
#define LEDGE_DENSITY_SZ 5000   // noise feature size (bigger = broader regions)
#define LEDGE_DENSITY_LO 0.45f  // noise <= this  -> cell gets no ledges
#define LEDGE_DENSITY_HI 0.68f  // noise >= this  -> cell fills all LEDGE_MAX slots

// --- per-ledge size & flat top ----------------------------------------------
// Radius in blocks (diameter = 2*R); and the fraction of the radius that is
// perfectly flat (the standable shelf) before the rim eases off.
#define LEDGE_R_MIN      5.f    // smallest ledge radius (10 blocks across)
#define LEDGE_R_MAX      80.f   // largest ledge radius (160 blocks across)
#define LEDGE_CORE_MIN   0.30f  // smallest flat-top fraction of the radius
#define LEDGE_CORE_MAX   0.70f  // largest flat-top fraction

// --- shape: anisotropy (ovals & ridges) -------------------------------------
// Each ledge is stretched along a random axis: 1 = round, higher = a longer,
// narrower ridge.
#define LEDGE_ASPECT_MIN 1.0f   // round
#define LEDGE_ASPECT_MAX 3.5f   // long ridge

// --- shape: rim wobble (irregular, pinched, degenerate outlines) ------------
// Two octaves of coherent noise perturb the rim radius. Bigger amplitude bites
// deeper into the outline, down to pinched-off crescents and split patches.
#define LEDGE_WOB_MIN    0.7f   // gentle, near-elliptical rim
#define LEDGE_WOB_MAX    2.1f   // wild, pinched, can split the shelf
#define LEDGE_RAD_BASE   0.25f  // rim radius floor as a fraction of R, before wobble
#define LEDGE_RAD_FLOOR  0.5f   // absolute min rim radius in blocks (avoids /0)
// per octave: rim-noise feature size is R * _FREQ (with a _FLOOR so tiny ledges
// still wobble), and _WT is that octave's blend weight (the two should sum ~1).
#define LEDGE_WOB1_FREQ  1.3f   // octave 1: broad lobes
#define LEDGE_WOB1_FLOOR 8
#define LEDGE_WOB1_WT    0.6f
#define LEDGE_WOB2_FREQ  0.5f   // octave 2: fine detail
#define LEDGE_WOB2_FLOOR 5
#define LEDGE_WOB2_WT    0.4f

// --- mountain-range gate ----------------------------------------------------
// Ledges appear only where this mask is positive. It MUST match the mountain-
// range mask in terrain_raw_height() so shelves land on the ranges, not the
// plains — if you retune the ranges there, mirror the change here.
#define LEDGE_RANGE_SZ   4000
#define LEDGE_RANGE_SEED 11223344
#define LEDGE_RANGE_LO   0.60f
#define LEDGE_RANGE_HI   0.72f

// --- internals (rarely need touching) ---------------------------------------
#define LEDGE_CACHE       2048       // per-thread cell cache slots (power of two)
#define LEDGE_SLOT_SPREAD 16         // > LEDGE_MAX, keeps per-slot rim seeds distinct
#define LEDGE_TWO_PI      6.2831853f // random orientation is 0..2pi
// distinct hash salts so the independent per-cell draws don't correlate
#define LEDGE_SALT_CELL    0x1ed9e5
#define LEDGE_SALT_KP      0x513a9e
#define LEDGE_SALT_DENSITY 0x0de5a1
#define LEDGE_SALT_WOB2    0x2b7

#endif // TINYCGAMES_LEDGE_CONFIG_C_INCLUDED

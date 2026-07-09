// ============================================================================
// terrain_config.c — tuning knobs for the base terrain in terrain.c
// ============================================================================
//
// This is the single home for terrain tuning constants. The two scattered
// key-point systems keep their own files (warp_config.c, ledge_config.c)
// because they are large and self-contained; everything else in terrain.c
// lands here, one documented section per feature, in the order the passes run.
//
// The pipeline: get_height() builds a base height field, then presses plateaus
// and oceans into it. terrain_raw_height() domain-warps the lookup (warp_config)
// and then adds, in order: peaks, mounds, lumps, pits, a smoothing pass, the
// broad mountain ranges, and finally a ceiling bounce that folds over-tall
// crests into calderas. get_filtered_height() lays ledges on top (ledge_config).
//
// The game maps height 0.5 to sea level and ~1.56 to just below Y=0, so height
// knobs below read against that scale. Each knob is safe to tweak; the comment
// says what it does.

#ifndef TINYCGAMES_TERRAIN_CONFIG_C_INCLUDED
#define TINYCGAMES_TERRAIN_CONFIG_C_INCLUDED

// --- base height field (get_height) -----------------------------------------
// Five noise octaves summed with a constant bias make the raw continent shape.
// Per octave: _SZ = feature size in blocks (smaller = finer detail), _WT = how
// much it contributes, _OCT = internal fBm octaves in that noise call. Weights
// roughly halve each step, so O1 sets the broad shape and O5 adds fine grain.
#define BASE_BIAS   0.05f   // constant floor added to every sample
#define BASE_O1_SZ  1300
#define BASE_O1_WT  0.60f
#define BASE_O1_OCT 2
#define BASE_O2_SZ  800
#define BASE_O2_WT  0.30f
#define BASE_O2_OCT 1
#define BASE_O3_SZ  400
#define BASE_O3_WT  0.15f
#define BASE_O3_OCT 1
#define BASE_O4_SZ  200
#define BASE_O4_WT  0.08f
#define BASE_O4_OCT 1
#define BASE_O5_SZ  100
#define BASE_O5_WT  0.04f
#define BASE_O5_OCT 1

// --- plateau / shelf pass (get_height) --------------------------------------
// Inside plateau regions the smooth height is quantized onto evenly spaced flat
// shelves joined by short cliffs (mesa/terrace country). The step ladder uses a
// *centered* smoothstep riser, so terracing adds shelves without raising or
// lowering the terrain on average, and shelves span the whole height range — not
// just near sea level. A low-frequency phase offset drifts shelf elevations from
// region to region so mesas don't all line up at the same heights.
//
// MASK picks which regions terrace, blended in by plateauness. noise() is
// symmetric about 0.5, so LO/HI around there set coverage: at LO=0.45,HI=0.52
// roughly two thirds of the world terraces and about 45% of it fully.
#define PLATEAU_MASK_SZ    2500      // size of a plateau region in blocks
#define PLATEAU_MASK_SEED  34899346
#define PLATEAU_MASK_LO    0.45f     // mask <= this -> no terracing
#define PLATEAU_MASK_HI    0.52f     // mask >= this -> full terracing

// Shelf geometry. STEP is the vertical spacing between shelves in height units
// (x TERRAIN_VSCALE 160 = blocks; 0.075 ~ 12 blocks per step). RISER is the
// fraction of each step taken up by the cliff, the rest being flat tread;
// smaller = crisper, more vertical cliffs.
#define PLATEAU_STEP       0.075f
#define PLATEAU_RISER      0.35f

// Regional shelf-height drift. PHASE_SZ must be large so a shelf stays flat
// across a whole mesa; the offset spans a full step so mesa heights decorrelate
// region to region (kills global contour-banding). The .32..68 window matches
// the noise 5th/95th percentiles so the phase covers ~0..1.
#define PLATEAU_PHASE_SZ   1500
#define PLATEAU_PHASE_SEED 0x5a1e00
#define PLATEAU_PHASE_LO   0.32f
#define PLATEAU_PHASE_HI   0.68f

// --- big oceans (get_height) ------------------------------------------------
// A very low-frequency mask presses lowlands below sea level (0.5). The mountain
// passes run afterward and can still raise islands out of the water. The floor
// keeps a little relief (base + FLOOR_RELIEF * height) so ocean beds aren't dead
// flat.
#define OCEAN_MASK_SZ     9000
#define OCEAN_MASK_SEED   41741741
#define OCEAN_MASK_LO     0.53f   // mask <= this -> dry land
#define OCEAN_MASK_HI     0.61f   // mask >= this -> full ocean
#define OCEAN_FLOOR_BASE  0.38f   // deepest the sea bed is pressed to
#define OCEAN_FLOOR_RELIEF 0.10f  // how much original relief survives on the bed

// --- peaks: isolated blobby mountains + calderas + stacks -------------------
// A mid-scale mask raises scattered massifs; above THRESH the excess is scaled
// by GAIN into height. calds carves twin caldera dimples (both a value and its
// inverse gate off the same noise, so craters appear on high and low sides);
// stacks add spiky over-height. Distinct from the broad "ranges" pass below.
#define PEAK_MASK_SZ    1200
#define PEAK_MASK_SEED  46447731
#define PEAK_THRESH     0.7f    // mask must exceed this to raise a peak
#define PEAK_GAIN       3.0f    // multiplies the excess above THRESH
#define PEAK_CALD_SZ    140
#define PEAK_CALD_SEED  96264448
#define PEAK_CALD_THRESH 0.59f  // caldera bites in past this (both polarities)
#define PEAK_STACK_SZ   205
#define PEAK_STACK_SEED 77000325
#define PEAK_STACK_THRESH 0.61f // spiky over-height appears past this

// --- mounds / lumps / pits: small threshold-noise bumps ---------------------
// Three near-identical passes. Each reads a noise field; where it exceeds
// THRESH, the excess is added to (mounds, lumps) or subtracted from (pits) the
// height, dotting the surface with gentle rises and hollows at three scales.
#define MOUND_SZ     290
#define MOUND_SEED   98453517
#define MOUND_THRESH 0.65f
#define LUMP_SZ      175
#define LUMP_SEED    36447731
#define LUMP_THRESH  0.65f
#define PIT_SZ       430
#define PIT_SEED     77488339
#define PIT_THRESH   0.65f

// --- smoothouts: local box blur ---------------------------------------------
// Where this mask exceeds THRESH, height is replaced by the average of four
// samples offset by SPREAD, softening terrain into rolling basins. The offset
// grows from 0 at THRESH to MAX_SPREAD blocks at mask = 1 (stronger = smoother).
#define SMOOTH_SZ        990
#define SMOOTH_SEED      13546936
#define SMOOTH_THRESH    0.65f
#define SMOOTH_MAX_SPREAD 1000.f

// --- mountain ranges: broad linear cordilleras (terrain_raw_height) ---------
// The region mask lives in RANGE_MASK_* (below, shared with the ledge pass).
// Inside a range, two octaves of folded ("ridged") noise carve crests and
// valleys; amplitude rides the range mask so ranges can cross ocean surface as
// island chains. Added height = mask * (BASE + R1_WT*ridge1 + R2_WT*ridge2^2).
#define RANGE_R1_SZ    700
#define RANGE_R1_SEED  22334455
#define RANGE_R2_SZ    250
#define RANGE_R2_SEED  33445566
#define RANGE_AMP_BASE 0.06f   // minimum lift anywhere in a range
#define RANGE_R1_WT    0.85f   // weight of the broad ridgelines
#define RANGE_R2_WT    0.30f   // weight of the finer ridge detail

// --- mountain-range region mask ---------------------------------------------
// A broad, low-frequency noise picks which regions are mountain ranges (the big
// linear cordilleras above). Below LO there is no range; the value ramps to full
// strength by HI.
//
// SHARED: the ledge pass (ledge_cell_get in terrain.c) gates its shelves on this
// same mask so ledges only land on the ranges. Both sites read these macros, so
// the two can no longer drift — retune here and both follow.
#define RANGE_MASK_SZ    4000       // feature size of the range-region mask
#define RANGE_MASK_SEED  11223344   // XORed into world_seed for this mask
#define RANGE_MASK_LO    0.60f      // mask <= this  -> no range here
#define RANGE_MASK_HI    0.72f      // mask >= this  -> full range strength

// --- ceiling bounce: fold over-tall crests into calderas --------------------
// Mountains can't punch through the sky. A gently wandering ceiling (~Y10) sits
// at CEIL_BASE; crests rising past it fold back down (a raised rim ringing a
// sunken crater) instead of pancaking flat. The ceiling wanders on low-frequency
// noise (WANDER_AMP about CEIL_BASE) so rim heights and crater mouths vary. Fold
// depth is clamped to MAX_CRATER so floors stay above the mid-mountain.
#define CEIL_BASE       1.53f
#define CEIL_WANDER_AMP 0.11f
#define CEIL_WANDER_SZ  300
#define CEIL_WANDER_SEED 0x0ca1de7
#define CEIL_MAX_CRATER 0.38f

#endif // TINYCGAMES_TERRAIN_CONFIG_C_INCLUDED

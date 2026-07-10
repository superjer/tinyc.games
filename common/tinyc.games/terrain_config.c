// ============================================================================
// terrain_config.c — compile-time constants for the base terrain in terrain.c
// ============================================================================
//
// All the tunable knobs moved to terrain_knobs.h — one big table that also
// generates blocko's in-game tweaker (K key) and `tweak` socket command.
// What stays here is the stuff that can't change at runtime: the seeds that
// decorrelate the noise fields. Change one to re-roll where that feature
// lands in the world.

#ifndef TINYCGAMES_TERRAIN_CONFIG_C_INCLUDED
#define TINYCGAMES_TERRAIN_CONFIG_C_INCLUDED

#define PLATEAU_MASK_SEED  34899346
#define PLATEAU_PHASE_SEED 0x5a1e00
#define PLATEAU_JITTER_SEED 0x71771e
#define OCEAN_MASK_SEED   41741741
#define PEAK_MASK_SEED  46447731
#define PEAK_CALD_SEED  96264448
#define PEAK_STACK_SEED 77000325
#define MOUND_SEED   98453517
#define LUMP_SEED    36447731
#define PIT_SEED     77488339
#define SMOOTH_SEED      13546936
#define RANGE_R1_SEED  22334455
#define RANGE_R2_SEED  33445566
#define RANGE_MASK_SEED  11223344   // shared: gates both ranges and ledges
#define CEIL_WANDER_SEED 0x0ca1de7

#endif // TINYCGAMES_TERRAIN_CONFIG_C_INCLUDED

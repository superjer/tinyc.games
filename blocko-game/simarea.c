#include "blocko.c"
#ifndef BLOCKO_SIMAREA_C_INCLUDED
#define BLOCKO_SIMAREA_C_INCLUDED

// Simulation reads the world through these, never raw T_: the render ring
// only holds terrain near the local player, but the sim (mobs chasing remote
// players) must see terrain around every anchor. Resolution: main ring if
// the absolute chunk is stamp-resident, then the per-player sim areas, then
// solid barrier - a mob outside all coverage stands on nothing rather than
// falling through the world. Window coords like T_; callers bounds-check y.

int sim_tile(int wx, int y, int wz)
{
        if (AGEN_(B2C(wx), B2C(wz)))
                return T_(wx, y, wz);
        return BARR;
}

int sim_gndh(int wx, int wz)
{
        if (AGEN_(B2C(wx), B2C(wz)))
                return GNDH_(wx, wz);
        return 0; // ground at the world top: no headroom, nothing spawns
}

#endif // BLOCKO_SIMAREA_C_INCLUDED

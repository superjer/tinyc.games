#ifndef TINYCGAMES_UTILS_C_INCLUDED
#define TINYCGAMES_UTILS_C_INCLUDED

#include <stdio.h>
#include <stdlib.h>

#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define SWAP(a,b) {int *x = &(a); int *y = &(b); int t = *x; *x = *y; *y = t;}
#define CLAMP(x,lo,hi) ((x) < (lo) ? (lo) : (x) > (hi) ? (hi) : (x))
#define MOD(a,b) (((a) % (b) + (b)) % (b))

// lerp between values a and b, t = 0..1
float lerp(float t, float a, float b)
{
        return a + t * (b - a);
}

// remap a value in the range fromlo..fromhi to tolo..tohi with clamping
float remap(float val, float fromlo, float fromhi, float tolo, float tohi)
{
        val = CLAMP(val, fromlo, fromhi);
        val = (val - fromlo) / (fromhi - fromlo);
        val = val * (tohi - tolo) + tolo;
        return val;
}

// remap exclusively values in the range fromlo..fromhi to tolo..tohi, leaving other values untouched
float excl_remap(float val, float fromlo, float fromhi, float tolo, float tohi)
{
        if (val < fromlo || val > fromhi)
                return val;
        val = (val - fromlo) / (fromhi - fromlo);
        val = val * (tohi - tolo) + tolo;
        return val;
}

#endif // TINYCGAMES_UTILS_C_INCLUDED

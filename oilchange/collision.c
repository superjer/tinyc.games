#include "oilchange.h"

//collide a rect with a rect
int collide(struct box l, struct box r)
{
        int xcollide = l.x + l.w >= r.x && l.x < r.x + r.w;
        int ycollide = l.y + l.h >= r.y && l.y < r.y + r.h;
        int zcollide = l.z + l.d >= r.z && l.z < r.z + r.d;
        return xcollide && ycollide && zcollide;
}


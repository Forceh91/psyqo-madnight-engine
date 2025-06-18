#ifndef _COLLISION_TYPES_H
#define _COLLISION_TYPES_H

#include "psyqo/vector.hh"

struct OBB
{
    psyqo::Vec3 center;
    psyqo::Vec3 axes[3];
    psyqo::Vec3 halfExtents;
};

struct AABBCollision
{
    psyqo::Vec3 min;
    psyqo::Vec3 max;
};

#endif

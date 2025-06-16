#ifndef _COLLISION_H
#define _COLLISION_H

#include "object/gameobject.hh"

struct AABBCollision
{
    psyqo::Vec3 min;
    psyqo::Vec3 max;
};

class Collision
{
public:
    static void GenerateAABBForMesh(const GameObject *object, AABBCollision *collisionBoxOut);
    static bool IsAABBCollision(const AABBCollision &collisionA, const AABBCollision &collisionB);
};

#endif

#ifndef _COLLISION_H
#define _COLLISION_H

#include "object/gameobject.hh"
#include "collision_types.hh"

class Collision
{
public:
    static void GenerateAABBForMesh(const GameObject *object, AABBCollision *collisionBoxOut);
    static bool IsAABBCollision(const AABBCollision &collisionA, const AABBCollision &collisionB);
    static bool IsSATCollision(const OBB &collisionA, const OBB &collisionB);
};

#endif

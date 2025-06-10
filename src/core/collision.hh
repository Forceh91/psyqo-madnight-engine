#ifndef _COLLISION_H
#define _COLLISION_H

#include "../mesh/mesh_manager.hh"

typedef struct _AABB_COLLISION
{
    psyqo::Vec3 min;
    psyqo::Vec3 max;
} AABBCollision;

class Collision
{
public:
    static void GenerateAABBForMesh(const MESH *mesh, AABBCollision *collisionBoxOut);
    static bool IsAABBCollision(const AABBCollision &collisionA, const AABBCollision &collisionB);
};

#endif

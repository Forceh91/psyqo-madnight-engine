#ifndef _RAYCAST_H
#define _RAYCAST_H

#include "../mesh/mesh_manager.hh"
#include "object/gameobject.hh"
#include "world_defs.hh"

#include "psyqo/vector.hh"

static constexpr psyqo::FixedPoint<> maxRayDistance = ONE_METRE * 10; // 10m

typedef struct _RAY
{
    psyqo::Vec3 origin;
    psyqo::Vec3 direction;           // normalized
    psyqo::FixedPoint<> maxDistance; // in metres. try to keep this value small. 128px = 1m
} Ray;

typedef struct _RAY_HIT
{
    bool hit;
    psyqo::FixedPoint<> distance;
    GameObject *object;
} RayHit;

class Raycast
{
    static bool DoesRaycastInterceptAABB(const Ray &ray, const GameObject *object);

public:
    static bool RaycastScene(const Ray &ray, GameObjectTag targetTag, RayHit *hitOut);
};

#endif

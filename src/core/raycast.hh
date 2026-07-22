#ifndef _RAYCAST_H
#define _RAYCAST_H

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
    psyqo::Vec3 hitPos; // world-space hit
    GameObject *object;
} RayHit;

class Raycast
{
    static bool DoesRaycastInterceptAABB(const Ray &ray, const GameObject *object, psyqo::FixedPoint<> *outDistance);

public:
    static bool RaycastScene(const Ray &ray, GameObjectTag targetTag, RayHit *hitOut);
};

#endif

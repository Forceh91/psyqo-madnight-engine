#ifndef _RAYCAST_H
#define _RAYCAST_H

#include "../mesh/mesh_manager.hh"
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
    LOADED_MESH *mesh;
} RayHit;

class Raycast
{
    static bool DoesRaycastInterceptAABB(const Ray &ray, const MESH *mesh);

public:
    static bool RaycastScene(const Ray &ray, const MeshType &targetType, RayHit *hitOut);
};

#endif

#include "raycast.hh"
#include "collision.hh"
#include "psyqo/xprintf.h"

bool Raycast::RaycastScene(const Ray &ray, const MeshType &targetType, RayHit *hitOut)
{
    // make sure its not too short/long
    if (ray.maxDistance <= 0 || ray.maxDistance > maxRayDistance)
        return false;

    // find all meshes of type, if none then presume no hit
    LOADED_MESH *meshes[MAX_LOADED_MESHES];
    uint8_t count = 0;
    if ((count = MeshManager::GetMeshesOfType(targetType, meshes)) != 0)
    {
        // for each mesh of type...
        for (uint8_t i = 0; i < count; i++)
        {
            MESH *mesh = &meshes[i]->mesh;

            // do an AABB check on this, did it hit?
            if ((hitOut->hit = DoesRaycastInterceptAABB(ray, mesh)))
            {
                // hitOut->distance = distance; // get distance? do we need it
                hitOut->mesh = meshes[i];
                return true;
            }
        }
    }

    hitOut->hit = false;
    return false;
}

bool Raycast::DoesRaycastInterceptAABB(const Ray &ray, const MESH *mesh)
{
    // make sure the mesh is valid
    if (mesh == nullptr)
        return false;

    // get AABB box for the mesh
    AABBCollision aabbBox = {0};
    Collision::GenerateAABBForMesh(mesh, &aabbBox);
    if (aabbBox.min.x == UINT16_MAX)
        return false;

    psyqo::FixedPoint<> tMin = -10.0_fp;
    psyqo::FixedPoint<> tMax = 10.0_fp;

    for (uint8_t axis = 0; axis < 3; axis++)
    {
        // we won't move into AABB along X
        if (ray.direction.get(axis) == 0)
        {
            // but are we already inside it?
            if (ray.origin.get(axis) < aabbBox.min[axis] || ray.origin.get(axis) > aabbBox.max[axis])
                return false; // no we're not

            continue; // yes we are
        }

        // we know the ray isn't parallel so where do we enter/exit?
        psyqo::FixedPoint<> invDirX = 1.0_fp / ray.direction.get(axis);
        psyqo::FixedPoint<> t1 = (aabbBox.min[axis] - ray.origin.get(axis)) * invDirX;
        psyqo::FixedPoint<> t2 = (aabbBox.max[axis] - ray.origin.get(axis)) * invDirX;

        // if entry > exit, swap them round
        if (t1 > t2)
            eastl::swap(t1, t2);

        printf("t1=%d, t2=%d\n", t1, t2);

        if (t1 > tMin)
            tMin = t1;

        if (t2 < tMax)
            tMax = t2;

        printf("tmin=%d, tmax=%d\n", tMin, tMax);

        if (tMax < tMin)
            return false; // no overlaps
    }

    // is the box behind or too far?
    if (tMin < 0 || tMin > ray.maxDistance)
        return false;

    return true;
}

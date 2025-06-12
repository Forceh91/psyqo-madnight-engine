#include "raycast.hh"
#include "collision.hh"
#include "../render/camera.hh"
#include "psyqo/soft-math.hh"

bool Raycast::RaycastScene(const Ray &ray, RayHit *hitOut)
{
    // make sure its not too short/long
    if (ray.maxDistance <= 0)
        return false;

    // ray.maxDistance = eastl::min(ray.maxDistance, maxRayDistance);

    // find all meshes of type, if none then presume no hit
    LOADED_MESH *meshes[MAX_LOADED_MESHES];
    uint8_t count = 0;
    // if ((count = MeshManager::GetMeshesOfType(targetType, meshes)) != 0)
    // {
    //     // for each mesh of type...
    //     for (uint8_t i = 0; i < count; i++)
    //     {
    //         // make sure its loaded...
    //         if (!meshes[i]->is_loaded)
    //             continue;

    //         MESH *mesh = &meshes[i]->mesh;

    //         // do an AABB check on this, did it hit?
    //         if ((hitOut->hit = DoesRaycastInterceptAABB(ray, mesh)))
    //         {
    //             // hitOut->distance = distance; // get distance? do we need it
    //             hitOut->mesh = meshes[i];
    //             return true;
    //         }
    //     }
    // }

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

    psyqo::FixedPoint<> tMin = -1000.0_fp;
    psyqo::FixedPoint<> tMax = 1000.0_fp;
    psyqo::Vec3 origin = ray.origin;
    psyqo::Vec3 normalizedRayDirection = ray.direction;
    psyqo::SoftMath::normalizeVec3(&normalizedRayDirection);
    for (uint8_t axis = 0; axis < 3; axis++)
    {
        // we won't move into AABB along axis
        if (normalizedRayDirection[axis] == 0)
        {
            // but are we already inside it?
            if (origin[axis] < aabbBox.min[axis] || origin[axis] > aabbBox.max[axis])
                return false; // no we're not

            continue; // yes we are
        }

        // we know the ray isn't parallel so where do we enter/exit?
        psyqo::FixedPoint<> invDir = 1.0_fp / normalizedRayDirection[axis];
        psyqo::FixedPoint<> t1 = (aabbBox.min[axis] - origin[axis]) * invDir;
        psyqo::FixedPoint<> t2 = (aabbBox.max[axis] - origin[axis]) * invDir;

        // if entry > exit, swap them round
        if (t1 > t2)
            eastl::swap(t1, t2);

        if (t1 > tMin)
            tMin = t1;

        if (t2 < tMax)
            tMax = t2;

        if (tMin > tMax)
            return false; // missed the box
    }

    // is the box behind or too far?
    if (tMin < 0 || tMin > ray.maxDistance)
        return false;

    return true;
}

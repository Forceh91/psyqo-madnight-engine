#include "collision.hh"
#include "../mesh/mesh_manager.hh"
#include "psyqo/soft-math.hh"
#include "psyqo/fixed-point.hh"
#include "psyqo/xprintf.h"

// TODO: cache these AABBs somewhere?
// TODO: this obviously gets weird with rotated objects so might need to update later
[[deprecated("This should be done offline. DO NOT USE")]]
void Collision::GenerateAABBForMesh(const GameObject *object, AABBCollision *collisionBoxOut)
{
    const MESH *mesh = object->mesh();

    // make sure it has faces
    if (mesh->faces_num == 0)
        return;

    // vec for min/max points
    collisionBoxOut->min.x = UINT16_MAX;
    collisionBoxOut->min.y = UINT16_MAX;
    collisionBoxOut->min.z = UINT16_MAX;
    collisionBoxOut->max.x = -UINT16_MAX;
    collisionBoxOut->max.y = -UINT16_MAX;
    collisionBoxOut->max.z = -UINT16_MAX;

    psyqo::Vec3 transformedPos = {0};
    psyqo::SoftMath::matrixVecMul3(object->rotationMatrix(), object->pos(), &transformedPos);

    // for each vertex on the mesh
    for (uint16_t vert = 0; vert < mesh->vertex_count; vert++)
    {
        // apply object rotation to the vert positions
        auto vertPos = mesh->vertices[vert];

        // mins
        if (vertPos.x < collisionBoxOut->min.x)
            collisionBoxOut->min.x = vertPos.x;

        if (vertPos.y < collisionBoxOut->min.y)
            collisionBoxOut->min.y = vertPos.y;

        if (vertPos.z < collisionBoxOut->min.z)
            collisionBoxOut->min.z = vertPos.z;

        // maxes
        if (vertPos.x > collisionBoxOut->max.x)
            collisionBoxOut->max.x = vertPos.x;

        if (vertPos.y > collisionBoxOut->max.y)
            collisionBoxOut->max.y = vertPos.y;

        if (vertPos.z > collisionBoxOut->max.z)
            collisionBoxOut->max.z = vertPos.z;
    }
}

// this is super simple and doesn't care about rotations
bool Collision::IsAABBCollision(const AABBCollision &collisionA, const AABBCollision &collisionB)
{
    // AABB box: check the min/max X
    if (collisionA.max.x < collisionB.min.x || collisionA.min.x > collisionB.max.x)
        return false;

    // AABB box: check the min/max Y
    if (collisionA.max.y < collisionB.min.y || collisionA.min.y > collisionB.max.y)
        return false;

    // AABB box: check the min/max Z
    if (collisionA.max.z < collisionB.min.z || collisionA.min.z > collisionB.max.z)
        return false;

    // ok so we know we're colliding
    return true;
}

// this is more complicated and does take into account object rotation properly
bool Collision::IsSATCollision(const OBB &collisionA, const OBB &collisionB)
{
    // get all of the axes that we want to check
    // this is the 3 from object a, 3 object b, and 9 from the cross product of all a+b combos
    eastl::array<psyqo::Vec3, 15> axesToCheck = {
        collisionA.axes[0],
        collisionA.axes[1],
        collisionA.axes[2],
        collisionB.axes[0],
        collisionB.axes[1],
        collisionB.axes[2],
        psyqo::SoftMath::crossProductVec3(collisionA.axes[0], collisionB.axes[0]),
        psyqo::SoftMath::crossProductVec3(collisionA.axes[0], collisionB.axes[1]),
        psyqo::SoftMath::crossProductVec3(collisionA.axes[0], collisionB.axes[2]),
        psyqo::SoftMath::crossProductVec3(collisionA.axes[1], collisionB.axes[0]),
        psyqo::SoftMath::crossProductVec3(collisionA.axes[1], collisionB.axes[1]),
        psyqo::SoftMath::crossProductVec3(collisionA.axes[1], collisionB.axes[2]),
        psyqo::SoftMath::crossProductVec3(collisionA.axes[2], collisionB.axes[0]),
        psyqo::SoftMath::crossProductVec3(collisionA.axes[2], collisionB.axes[1]),
        psyqo::SoftMath::crossProductVec3(collisionA.axes[2], collisionB.axes[2])};

    // we can skip this axes if we get a vec3 zero for any of these
    for (auto &axis : axesToCheck)
    {
        // skip this one if its zero
        if (axis.x == 0 && axis.y == 0 && axis.z == 0)
            continue;

        // normalize it
        auto normalizedAxis = axis;
        psyqo::SoftMath::normalizeVec3(&normalizedAxis);

        // project centres
        // TODO: make this a function?
        auto aCentre = (collisionA.center.x * normalizedAxis.x) + (collisionA.center.y * normalizedAxis.y) + (collisionA.center.z * normalizedAxis.z);
        auto bCentre = (collisionB.center.x * normalizedAxis.x) + (collisionB.center.y * normalizedAxis.y) + (collisionB.center.z * normalizedAxis.z);

        // now project the half extents
        auto aRadius = ((collisionA.axes[0].x * normalizedAxis.x) + (collisionA.axes[0].y * normalizedAxis.y) + (collisionA.axes[0].z * normalizedAxis.z)).abs() * collisionA.halfExtents.x +
                       ((collisionA.axes[1].x * normalizedAxis.x) + (collisionA.axes[1].y * normalizedAxis.y) + (collisionA.axes[1].z * normalizedAxis.z)).abs() * collisionA.halfExtents.y +
                       ((collisionA.axes[2].x * normalizedAxis.x) + (collisionA.axes[2].y * normalizedAxis.y) + (collisionA.axes[2].z * normalizedAxis.z)).abs() * collisionA.halfExtents.z;

        auto bRadius = ((collisionB.axes[0].x * normalizedAxis.x) + (collisionB.axes[0].y * normalizedAxis.y) + (collisionB.axes[0].z * normalizedAxis.z)).abs() * collisionB.halfExtents.x +
                       ((collisionB.axes[1].x * normalizedAxis.x) + (collisionB.axes[1].y * normalizedAxis.y) + (collisionB.axes[1].z * normalizedAxis.z)).abs() * collisionB.halfExtents.y +
                       ((collisionB.axes[2].x * normalizedAxis.x) + (collisionB.axes[2].y * normalizedAxis.y) + (collisionB.axes[2].z * normalizedAxis.z)).abs() * collisionB.halfExtents.z;

        // check for overlap
        auto distance = (aCentre - bCentre).abs();
        auto overlap = aRadius + bRadius;

        // no overlap if distance is greater than radius
        if (distance > overlap)
            return false;
    }

    // no separated axis found, so we must be colliding
    return true;
}

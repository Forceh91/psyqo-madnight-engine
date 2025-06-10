#include "collision.hh"
#include "psyqo/xprintf.h"

// TODO: cache these AABBs somewhere?
void Collision::GenerateAABBForMesh(const MESH *mesh, AABBCollision *collisionBoxOut)
{
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

    // for each vertex on the mesh
    for (uint16_t vert = 0; vert < mesh->vertex_count; vert++)
    {
        // TODO: apply camera pos to it...?
        // TODO: apply rotation to it?
        // create a vector
        psyqo::Vec3 vertPos = {mesh->vertices[vert].x, mesh->vertices[vert].y, mesh->vertices[vert].z};

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

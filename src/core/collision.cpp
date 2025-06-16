#include "collision.hh"
#include "../mesh/mesh_manager.hh"
#include "psyqo/soft-math.hh"

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

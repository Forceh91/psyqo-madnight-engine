#include "gameobject.hh"

#include "psyqo/soft-math.hh"
#include "psyqo/fixed-point.hh"
#include "psyqo/xprintf.h"
#include "../../madnight.hh"
#include "../collision.hh"

using namespace psyqo::fixed_point_literals;

void GameObject::Destroy(void)
{
    m_name.clear();
    m_pos = {0, 0, 0};
    m_rotation = {0, 0, 0};
    m_tag = GameObjectTag::NONE;
    m_mesh = nullptr;
    m_texture = nullptr;
    m_rotationMatrix = {0};
    m_obb = {0};
    m_collisionType = CollisionType::SOLID;
}

void GameObject::SetMesh(const char *meshName)
{
    MeshManager::GetMeshFromName(meshName, &m_mesh);
    GenerateOBB();
}

void GameObject::SetTexture(const char *textureName)
{
    TextureManager::GetTextureFromName(textureName, &m_texture);
}

void GameObject::SetPosition(psyqo::FixedPoint<12> x, psyqo::FixedPoint<12> y, psyqo::FixedPoint<12> z)
{
    m_pos.x = x;
    m_pos.y = y;
    m_pos.z = z;

    // update the OBB
    GenerateOBB();
}

void GameObject::SetRotation(psyqo::Angle x, psyqo::Angle y, psyqo::Angle z)
{
    m_rotation.x = x;
    m_rotation.y = y;
    m_rotation.z = z;

    GenerateRotationMatrix();
}

void GameObject::SetAsTrigger(const psyqo::Vec3 &triggerSize)
{
    // mark as trigger
    m_collisionType = CollisionType::TRIGGER;

    // generate half extents from trigger size, this should never need to change
    m_obb.halfExtents = triggerSize / 2;

    // generate the obb axes
    UpdateOBB();
}

void GameObject::GenerateRotationMatrix(void)
{
    auto roll = psyqo::SoftMath::generateRotationMatrix33(m_rotation.x, psyqo::SoftMath::Axis::X, g_madnightEngine.m_trig);
    auto pitch = psyqo::SoftMath::generateRotationMatrix33(m_rotation.y, psyqo::SoftMath::Axis::Y, g_madnightEngine.m_trig);
    auto yaw = psyqo::SoftMath::generateRotationMatrix33(m_rotation.z, psyqo::SoftMath::Axis::Z, g_madnightEngine.m_trig);

    // create complete x/y/z rotation. this is done ROLL then YAW then PITCH
    psyqo::Matrix33 tempMatrix = {0};
    psyqo::SoftMath::multiplyMatrix33(yaw, pitch, &tempMatrix);
    psyqo::SoftMath::multiplyMatrix33(tempMatrix, roll, &m_rotationMatrix);

    // update the OBB
    UpdateOBB();
}

void GameObject::GenerateOBB(void)
{
    if (m_mesh == nullptr)
    {
        printf("GAME OBJECT: [WARNING] You tried to generate an OBB before you had a mesh assigned.\n");
        return;
    }

    psyqo::Vec3 rotatedCentre = {0}, localCentre = (m_mesh->collisionBox.min + m_mesh->collisionBox.max) / 2;
    psyqo::SoftMath::matrixVecMul3(m_rotationMatrix, localCentre, &rotatedCentre);

    m_obb.center = m_pos + rotatedCentre;
    m_obb.halfExtents = (m_mesh->collisionBox.max - m_mesh->collisionBox.min) / 2;

    m_obb.axes[0] = m_rotationMatrix.vs[0];
    m_obb.axes[1] = m_rotationMatrix.vs[1];
    m_obb.axes[2] = m_rotationMatrix.vs[2];
}

void GameObject::UpdateOBB(void)
{
    // double check we're not being dumb
    if (m_collisionType == CollisionType::SOLID && m_mesh == nullptr)
        return;

    psyqo::Vec3 rotatedCentre = {0, 0, 0}, localCentre = m_collisionType == CollisionType::SOLID ? (m_mesh->collisionBox.min + m_mesh->collisionBox.max) / 2 : psyqo::Vec3{0, 0, 0};
    psyqo::SoftMath::matrixVecMul3(m_rotationMatrix, localCentre, &rotatedCentre);

    // update the centre
    m_obb.center = m_pos + rotatedCentre;

    // and the new rotation matrix
    m_obb.axes[0] = m_rotationMatrix.vs[0];
    m_obb.axes[1] = m_rotationMatrix.vs[1];
    m_obb.axes[2] = m_rotationMatrix.vs[2];
}

#include "gameobject.hh"

#include "psyqo/soft-math.hh"
#include "../../madnight.hh"

void GameObject::Destroy(void)
{
    m_name.clear();
    m_pos = {0, 0, 0};
    m_rotation = {0, 0, 0};
    m_tag = GameObjectTag::NONE;
    m_mesh = nullptr;
    m_texture = nullptr;
    m_rotationMatrix = {0};
}

void GameObject::SetMesh(const char *meshName)
{
    MeshManager::GetMeshFromName(meshName, &m_mesh);
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
}

void GameObject::SetRotation(psyqo::Angle x, psyqo::Angle y, psyqo::Angle z)
{
    m_rotation.x = x;
    m_rotation.y = y;
    m_rotation.z = z;

    GenerateRotationMatrix();
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
}

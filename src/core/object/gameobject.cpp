#include "gameobject.hh"

void GameObject::Destroy(void)
{
    m_name.clear();
    m_pos = {0, 0, 0};
    m_rotation = {0, 0, 0};
    m_tag = GameObjectTag::NONE;
    m_mesh = nullptr;
    m_texture = nullptr;
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
}

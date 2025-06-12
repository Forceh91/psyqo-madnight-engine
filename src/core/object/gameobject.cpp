#include "gameobject.hh"

void GameObject::Destroy(void)
{
    m_name.clear();
    m_pos = {0, 0, 0};
    m_rotation = {0, 0, 0};
    m_tag = GameObjectTag::NONE;
}

psyqo::Coroutine<> GameObject::SetMesh(const char *meshName)
{
    co_await MeshManager::LoadMeshFromCDROM(meshName, m_mesh);
}

psyqo::Coroutine<> GameObject::SetTexture(const char *textureName, uint16_t x, uint16_t y, uint16_t clutX, uint16_t clutY)
{
    co_await TextureManager::LoadTIMFromCDRom(textureName, x, y, clutX, clutY, m_texture);
}

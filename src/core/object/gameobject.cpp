#include "gameobject.hh"

void GameObject::Destroy(void)
{
    m_name.clear();
    m_pos = {0, 0, 0};
    m_rotation = {0, 0, 0};
    m_tag = GameObjectTag::NONE;
}

void GameObject::SetMesh(const char *meshName)
{
    MeshManager::load_mesh_from_cdrom(meshName, [this](MESH *mesh)
                                      { m_mesh = mesh; });
}

void GameObject::SetTexture(const char *textureName, uint16_t x, uint16_t y, uint16_t clutX, uint16_t clutY)
{
    TextureManager::LoadTIMFromCDRom(textureName, x, y, clutX, clutY, [this](TimFile *tim)
                                     { m_mesh->tim = tim; });
}

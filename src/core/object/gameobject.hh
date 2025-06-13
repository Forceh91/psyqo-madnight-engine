#ifndef _GAMEOBJECT_H
#define _GAMEOBJECT_H

#include "psyqo/coroutine.hh"
#include "psyqo/fragments.hh"
#include "psyqo/trigonometry.hh"
#include "psyqo/vector.hh"
#include "EASTL/fixed_string.h"

#include "gameobject_defs.hh"
#include "../../helpers/file_defs.hh"
#include "../../textures/texture_manager.hh"
#include "../../mesh/mesh_manager.hh"

typedef struct _GAMEOBJECT_ROTATION
{
    psyqo::Angle x, y, z;
} GameObjectRotation;

class GameObject final
{
    eastl::fixed_string<char, MAX_CDROM_FILE_NAME_LEN> m_name = "";
    GameObjectTag m_tag = GameObjectTag::NONE;
    psyqo::Vec3 m_pos = {0, 0, 0};
    GameObjectRotation m_rotation = {0, 0, 0};
    MESH *m_mesh = nullptr;
    TimFile *m_texture = nullptr;

public:
    GameObject() = default;
    GameObject(const char *name, psyqo::Vec3 pos, GameObjectRotation rotation, GameObjectTag tag)
    {
        m_name = name;
        m_pos = pos;
        m_rotation = rotation;
        m_tag = tag;
    };
    void Destroy(void);

    const eastl::fixed_string<char, MAX_CDROM_FILE_NAME_LEN> &name() { return m_name; }
    const psyqo::Vec3 &pos() { return m_pos; }
    const GameObjectRotation &rotation() { return m_rotation; }
    const MESH *mesh() { return m_mesh; }
    const TimFile *texture() { return m_texture; }
    const GameObjectTag &tag() { return m_tag; }

    psyqo::Coroutine<> SetMesh(const char *meshName);
    psyqo::Coroutine<> SetTexture(const char *textureName, uint16_t vramX, uint16_t vramY, uint16_t clutX, uint16_t clutY);
};

#endif

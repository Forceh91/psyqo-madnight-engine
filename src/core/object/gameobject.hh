#ifndef _GAMEOBJECT_H
#define _GAMEOBJECT_H

#include "psyqo/fragments.hh"
#include "psyqo/trigonometry.hh"
#include "psyqo/vector.hh"
#include "EASTL/fixed_string.h"

#include "gameobject_defs.hh"
#include "../../helpers/file_defs.hh"
#include "../../textures/texture_manager.hh"
#include "../../mesh/mesh_manager.hh"
#include "../collision_types.hh"

static constexpr uint8_t INVALID_GAMEOBJECT_ID = 255;

enum GameObjectQuadType
{
    Quad,
    TexturedQuad,
    GouraudQuad,
    GouraudTextureQuad,
};

typedef struct _GAMEOBJECT_ROTATION
{
    psyqo::Angle x, y, z;
} GameObjectRotation;

class GameObject final
{
    eastl::fixed_string<char, MAX_CDROM_FILE_NAME_LEN> m_name = "";
    uint8_t m_id = INVALID_GAMEOBJECT_ID;
    GameObjectQuadType m_quadType = GameObjectQuadType::Quad;
    GameObjectTag m_tag = GameObjectTag::NONE;
    psyqo::Vec3 m_pos = {0, 0, 0};
    GameObjectRotation m_rotation = {0, 0, 0};
    psyqo::Matrix33 m_rotationMatrix = {0};
    MESH *m_mesh = nullptr;
    TimFile *m_texture = nullptr;
    OBB m_obb = {0};
    CollisionType m_collisionType = CollisionType::SOLID;

    void GenerateRotationMatrix(void);
    void GenerateOBB(void);
    void UpdateOBB(void);

public:
    GameObject() = default;
    GameObject(const char *name, psyqo::Vec3 pos, GameObjectRotation rotation, GameObjectTag tag, uint8_t id)
    {
        m_name = name;
        m_pos = pos;
        m_rotation = rotation;
        m_tag = tag;
        m_id = id;

        GenerateRotationMatrix();
    };
    void Destroy(void);

    const eastl::fixed_string<char, MAX_CDROM_FILE_NAME_LEN> &name() { return m_name; }
    const uint8_t &id() { return m_id; };
    const psyqo::Vec3 &pos() const { return m_pos; }
    const GameObjectRotation &rotation() const { return m_rotation; }
    const psyqo::Matrix33 &rotationMatrix() const { return m_rotationMatrix; }
    const MESH *mesh() const { return m_mesh; }
    const TimFile *texture() { return m_texture; }
    const GameObjectTag &tag() { return m_tag; }
    const GameObjectQuadType &quadType() { return m_quadType; }
    const OBB &obb() { return m_obb; }

    void SetPosition(psyqo::FixedPoint<12> x, psyqo::FixedPoint<12> y, psyqo::FixedPoint<12> z);
    void SetRotation(psyqo::Angle x, psyqo::Angle y, psyqo::Angle z);
    void SetMesh(const char *meshName);
    void SetTexture(const char *textureName);
    // note: doesn't actually do anything yet. need to figure it out later when its important
    void SetQuadType(const GameObjectQuadType quadType) { m_quadType = quadType; }
    void SetAsTrigger(const psyqo::Vec3 &size);
};

#endif

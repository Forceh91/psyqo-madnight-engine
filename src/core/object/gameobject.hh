#pragma once

#include "../../helpers/archive.hh"
#include "../../mesh/mesh_manager.hh"
#include "../../textures/texture_manager.hh"
#include "../collision_types.hh"
#include "gameobject_defs.hh"

#include <psyqo/fixed-point.hh>
#include <psyqo/trigonometry.hh>
#include <psyqo/vector.hh>

static constexpr uint8_t MAX_GAMEOBJECT_NAME_LENGTH = 32;

enum GameObjectQuadType {
  Quad,
  TexturedQuad,
  GouraudQuad,
  GouraudTextureQuad,
};

typedef struct _GAMEOBJECT_ROTATION {
  psyqo::Angle x, y, z;
} GameObjectRotation;

enum RenderFlags { RF_NONE = 0, RF_DISTANCE_CHECK = 1 };

class GameObject final {
  uint64_t m_nameHash = 0;
  eastl::fixed_string<char, MAX_GAMEOBJECT_NAME_LENGTH> m_name;
  int16_t m_id = INVALID_POOL_ID;
  GameObjectQuadType m_quadType = GameObjectQuadType::Quad;
  GameObjectTag m_tag = GameObjectTag::NONE;
  psyqo::Vec3 m_pos = {0, 0, 0};
  GameObjectRotation m_rotation = {0, 0, 0};
  psyqo::Matrix33 m_rotationMatrix = {0};
  MeshBin* m_mesh = nullptr;
  TimFile* m_texture = nullptr;
  OBB m_obb = {0};
  CollisionType m_collisionType = CollisionType::SOLID;
  uint16_t m_renderFlags = 0;
  uint32_t m_flags = 0;

  void GenerateRotationMatrix(void);
  void GenerateOBB(void);
  void UpdateOBB(void);

public:
  GameObject() = default;
  void Init(const char* name, psyqo::Vec3 pos, GameObjectRotation rotation, GameObjectTag tag, int16_t id) {
    m_nameHash = HashName(name);
    m_name = name;
    m_pos = pos;
    m_rotation = rotation;
    m_tag = tag;
    m_id = id;

    GenerateRotationMatrix();
  };
  void Destroy(void);

  uint64_t nameHash() const { return m_nameHash; }
  const eastl::fixed_string<char, MAX_GAMEOBJECT_NAME_LENGTH> &name() const { return m_name; }
  const int16_t &id() const { return m_id; };
  const psyqo::Vec3 &pos() const { return m_pos; }

  const psyqo::Vec3 *posPtr() const { return &m_pos; }
  psyqo::Vec3 *posPtr() { return &m_pos; }

  const GameObjectRotation &rotation() const { return m_rotation; }
  const psyqo::Matrix33 &rotationMatrix() const { return m_rotationMatrix; }
  const MeshBin *mesh() const { return m_mesh; }
  MeshBin *mesh() { return m_mesh; }
  const TimFile *texture() const { return m_texture; }
  const GameObjectTag &tag() { return m_tag; }
  const GameObjectQuadType &quadType() { return m_quadType; }
  const OBB &obb() { return m_obb; }
  const OBB &obb() const { return m_obb; }

  void SetPosition(const psyqo::Vec3& pos);
  void SetPosition(psyqo::FixedPoint<12> x, psyqo::FixedPoint<12> y, psyqo::FixedPoint<12> z);
  void SetRotation(const GameObjectRotation &rotation);
  void SetRotation(psyqo::Angle x, psyqo::Angle y, psyqo::Angle z);
  void SetMesh(const char *meshName);
  void SetTexture(const char *textureName);
  // note: doesn't actually do anything yet. need to figure it out later when its important
  void SetQuadType(const GameObjectQuadType quadType) { m_quadType = quadType; }
  void SetAsTrigger(const psyqo::Vec3 &size);
  bool HasRenderFlag(const RenderFlags &rf) { return m_renderFlags & (1 << rf); }
  void SetRenderFlag(const RenderFlags &rf) { m_renderFlags |= (1 << rf); }
  void ClearRenderFlag(const RenderFlags &rf) { m_renderFlags &= ~(1 << rf); }
  void ClearRenderFlags(void) { m_renderFlags = RF_NONE; }

  bool HasFlag(const uint32_t &rf) { return m_flags & (1 << rf); }
  void SetFlag(const uint32_t &rf) { m_flags |= (1 << rf); }
  void ClearFlag(const uint32_t &rf) { m_flags &= ~(1 << rf); }
  void ClearFlags(void) { m_flags = 0; }
};

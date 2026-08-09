---
title: Physics & Collision
sidebar_position: 5
---

# Physics & Collision

`src/core/collision.hh`, `src/core/raycast.hh`, and `src/mesh/colbin_manager.hh` — AABB/SAT collision tests, raycasting, and loading baked `.COLBIN` collision meshes (see the [COLBIN format spec](../guides/colbin)).

## Collision

`src/core/collision.hh`

```cpp
struct CollisionTest {
  psyqo::Vec3 mtv;              // minimum translation vector to resolve the overlap
  psyqo::Vec3 normal;
  psyqo::FixedPoint<> penetration;
};

class Collision {
public:
  static void GenerateAABBForMesh(const GameObject *object, AABBCollision *collisionBoxOut);
  static bool IsAABBCollision(const AABBCollision &collisionA, const AABBCollision &collisionB);
  static bool IsSATCollision(const OBB &collisionA, const OBB &collisionB, CollisionTest *resultOut);
};
```

- **`IsAABBCollision`** is a cheap axis-aligned overlap test — used for broad-phase checks.
- **`IsSATCollision`** runs the Separating Axis Theorem against two oriented boxes and, on overlap, fills `CollisionTest` with the minimum translation vector needed to push them apart — this is what backs push-out collision response against `.COLBIN` wall `OBB`s (see [COLBIN](../guides/colbin#types)).

`OBB` and `AABBCollision` themselves are defined in `src/core/collision_types.hh` — see [Core → Collision types](./core#collision-types).

## Raycast

`src/core/raycast.hh`

```cpp
static constexpr psyqo::FixedPoint<> maxRayDistance = ONE_METRE * 10; // 10m

struct Ray {
  psyqo::Vec3 origin;
  psyqo::Vec3 direction;            // normalized
  psyqo::FixedPoint<> maxDistance;  // metres — keep small, 128px = 1m
};

struct RayHit {
  bool hit;
  psyqo::FixedPoint<> distance;
  psyqo::Vec3 hitPos;   // world-space hit position
  GameObject *object;
};

class Raycast {
public:
  static bool RaycastScene(const Ray &ray, GameObjectTag targetTag, RayHit *hitOut);
};
```

`RaycastScene` only tests against game objects carrying the given `GameObjectTag` (see [Core → GameObjectTag](./core#gameobjecttag)) — pass a specific tag to avoid testing against everything in the world, e.g. `INTERACTABLE` for a "what am I looking at" prompt. Internally it tests the ray against each candidate object's AABB.

## ColbinManager

`src/mesh/colbin_manager.hh`

Loads a single `.COLBIN` collision mesh — floor triangles (for raycast-based ground detection) plus wall `OBB`s (for SAT-based push-out collision), pre-bucketed into a spatial grid for broad-phase culling. See the [COLBIN format spec](../guides/colbin) for the full binary layout and the runtime grid-lookup algorithm.

```cpp
struct FloorTri {
  psyqo::Vec3 v0, v1, v2; // scaled by 128
  int16_t n[3];           // face normal, FP12 (scaled by 4096)
};

struct GridHeader {
  int32_t originX, originZ;
  uint32_t cellSize;
  uint16_t gridWidth, gridHeight;
};

struct GridCell {
  uint16_t count;
  uint16_t *indices; // into ColBin::walls
};

struct ColBin {
  Header header;
  GridHeader gridHeader;
  GridCell *gridCells;
  FloorTri *floors;
  OBB *walls;
};

class ColbinManager {
public:
  static psyqo::Coroutine<> LoadColbin(const eastl::fixed_string<char, MAX_ARCHIVE_FILE_NAME_LEN> &name, ColBin **colbinOut);
  static ColBin *Colbin(void);
  static void Dump(void);
  static eastl::span<OBB> walls(void);
};
```

Unlike `MeshManager`/`TextureManager`, `ColbinManager` holds a single collision mesh at a time (`m_colbin` is a lone static instance, not a pool) — one `.COLBIN` per loaded scene/level.

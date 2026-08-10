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

### Usage

Push-out collision response between two game objects, the standard pattern (also shown, commented out, in the engine's own `GameplayScene`):

```cpp
CollisionTest result;
if (Collision::IsSATCollision(player->obb(), wall->obb(), &result))
    player->SetPosition(player->pos() + result.mtv);
```

### Internals

- `IsSATCollision` checks 15 axes total (each box's 3 face normals, plus the 9 pairwise cross products between them) and tracks whichever axis has the *smallest* overlap — that's what ends up as the resolution `mtv`.
- `GenerateAABBForMesh` is marked `[[deprecated]]` in-source ("should be done offline, DO NOT USE") — collision boxes are meant to be baked into the mesh at export time, not computed at runtime.

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

### Usage

```cpp
Ray ray = {.origin = camera->pos(), .direction = camera->forwardVector(), .maxDistance = 2.0_ws};
RayHit hit = {0};
if (Raycast::RaycastScene(ray, GameObjectTag::INTERACTABLE, &hit))
    ShowInteractPrompt(hit.object);
```

### Internals

- Returns the **first** object hit while iterating the tagged list, not the closest one — if two interactables overlap along the ray, which one you get depends on iteration order, not distance.
- The header itself notes this is inaccurate for rotated objects — it's a straight AABB test, no OBB/SAT version yet.

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

### Usage

```cpp
ColBin *level;
co_await ColbinManager::LoadColbin("level01.colbin", &level);

for (auto &wall : ColbinManager::walls()) {
    CollisionTest result;
    if (Collision::IsSATCollision(player->obb(), wall, &result))
        player->SetPosition(player->pos() + result.mtv);
}
```

### Internals

- Loading a new `.COLBIN` fully overwrites `m_colbin` — there's no unload/reload-alongside step, so swap it out only when you're actually changing levels/rooms.
- The spatial grid (`gridCells`) buckets wall indices per cell for broad-phase lookups; see the [COLBIN format spec](../guides/colbin) for exactly how a world position maps to a cell.

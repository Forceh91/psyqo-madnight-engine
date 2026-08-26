---
title: Core
sidebar_position: 2
---

# Core

`src/core/` — game objects, billboards, particles, and in-engine debug tooling.

## GameObject

`src/core/object/gameobject.hh`

The engine's fundamental "thing in the world" — a positioned, rotated entity with an optional mesh, texture, and collision volume. Game objects are always owned and created by [`GameObjectManager`](#gameobjectmanager), never constructed directly in game code.

```cpp
class GameObject final {
public:
  GameObject() = default;
  GameObject(const char *name, psyqo::Vec3 pos, GameObjectRotation rotation, GameObjectTag tag, uint8_t id);

  void Destroy(void);

  const eastl::fixed_string<char, MAX_ARCHIVE_FILE_NAME_LEN> &name();
  const uint8_t &id() const;
  const psyqo::Vec3 &pos() const;
  const psyqo::Vec3 *posPtr() const;
  psyqo::Vec3 *posPtr();
  const GameObjectRotation &rotation() const;
  const psyqo::Matrix33 &rotationMatrix() const;
  const MeshBin *mesh() const;
  MeshBin *mesh();
  const TimFile *texture() const;
  const GameObjectTag &tag();
  const GameObjectQuadType &quadType();
  const OBB &obb();

  void SetPosition(const psyqo::Vec3 &pos);
  void SetPosition(psyqo::FixedPoint<12> x, psyqo::FixedPoint<12> y, psyqo::FixedPoint<12> z);
  void SetRotation(const GameObjectRotation &rotation);
  void SetRotation(psyqo::Angle x, psyqo::Angle y, psyqo::Angle z);
  void SetMesh(const char *meshName);
  void SetTexture(const char *textureName);
  void SetQuadType(const GameObjectQuadType quadType); // not implemented yet
  void SetAsTrigger(const psyqo::Vec3 &size);

  bool HasRenderFlag(const RenderFlags &rf);
  void SetRenderFlag(const RenderFlags &rf);
  void ClearRenderFlag(const RenderFlags &rf);
  void ClearRenderFlags(void);

  // generic, game-defined flags -- separate bitfield from RenderFlags, no engine meaning of its own
  bool HasFlag(const uint32_t &flag);
  void SetFlag(const uint32_t &flag);
  void ClearFlag(const uint32_t &flag);
  void ClearFlags(void);
};
```

- **`SetMesh`/`SetTexture`** look the asset up by name via `MeshManager`/`TextureManager` — the asset must already be loaded.
- **`SetAsTrigger`** sets the object's `CollisionType` to `TRIGGER` and its half-extents to the given size, and that's all it does today. `Collision::IsAABBCollision`/`IsSATCollision` take raw OBB/AABB data and never branch on collision type, so there's no built-in overlap-only response distinguishing a trigger from a `SOLID` object. The only reader of `m_collisionType` is `GameObject::UpdateOBB`, which decides whether to derive the OBB centre from the mesh's collision box. Any actual trigger behaviour (skip physical response, fire an event on overlap) is on the game to implement.
- **`RenderFlags::RF_DISTANCE_CHECK`** is currently unused. `HasRenderFlag`/`SetRenderFlag`/`ClearRenderFlag` don't appear anywhere outside `gameobject.hh`, and `Renderer::RenderGameObjects` never consults them: it gates visibility solely on `IsGameObjectVisible`.
- **`HasFlag`/`SetFlag`/`ClearFlag`** work on a separate, generic `uint32_t` bitfield with no engine-defined meaning — it's yours to use for game-specific per-object state (e.g. "already collected", "triggered this run") without needing a new field on every object.
- The object's OBB (`obb()`) and rotation matrix are (re)computed internally when position/rotation change — you don't need to update them yourself.

### Usage

```cpp
#include "core/object/gameobject_manager.hh"

// spawn a static prop
GameObject *crate = GameObjectManager::CreateGameObject(
    "crate_01",
    psyqo::Vec3{2.0_ws, 0.0_ws, 5.0_ws},
    GameObjectRotation{0, 0, 0},
    GameObjectTag::ENVIRONMENT);

crate->SetMesh("crate");      // must already be loaded via MeshManager::LoadMesh
crate->SetTexture("crate");   // must already be loaded via TextureManager::LoadTIM

// later, e.g. on level unload
GameObjectManager::DestroyGameObject(crate);
```

To mark the same object as a trigger instead of solid geometry (see above: it only changes what `UpdateOBB` derives the centre from; overlap handling is on you):

```cpp
GameObject *doorTrigger = GameObjectManager::CreateGameObject(
    "door_trigger", doorPos, {0, 0, 0}, GameObjectTag::INTERACTABLE);
doorTrigger->SetAsTrigger(psyqo::Vec3{1.0_ws, 2.0_ws, 1.0_ws}); // no mesh needed
```

### Internals

- `SetPosition`/`SetRotation` both recompute the OBB on every call — no dirty-flag batching, so setting both in one frame means two recomputations.
- A `SOLID` object's half-extents come from the mesh's baked collision box, set at asset-export time — not derived at runtime.
- `UpdateOBB` no longer bails out for a `SOLID` object with no mesh yet — it now falls through with a `{0,0,0}` local centre in that case, same as a `TRIGGER`, rather than skipping the update entirely.
- Worth double-checking if you rely on OBB centring: the current local-centre calculation for a solid mesh is `collisionBox.min + collisionBox.max / 2`, without parentheses around `min + max` — that's not the same value as the midpoint formula `(min + max) / 2` unless `min` is zero.

### GameObjectTag

```cpp
enum GameObjectTag { NONE, ENVIRONMENT, INTERACTABLE, PORTAL, PORTAL_ENTRANCE, PORTAL_EXIT };
```

Used to filter/query objects (see `GameObjectManager::GetGameObjectsWithTag` and [`Raycast::RaycastScene`](./physics-and-collision#raycast), which raycasts only against objects with a specific tag).

## GameObjectManager

`src/core/object/gameobject_manager.hh`

Owns a fixed pool of up to `MAX_GAME_OBJECTS` (250) `GameObject`s and hands out slots from it.

```cpp
class GameObjectManager final {
public:
  static GameObject *CreateGameObject(const char *name, psyqo::Vec3 pos, GameObjectRotation rotation, GameObjectTag tag = GameObjectTag::NONE);
  static void DestroyGameObject(GameObject *gameObject);
  static const eastl::fixed_vector<GameObject *, MAX_GAME_OBJECTS> &GetActiveGameObjects(void);
  static void ClearRenderableGameObjects(void);
  static void SetRenderableGameObjects(const eastl::span<GameObject*> renderList);
  static const eastl::fixed_vector<GameObject *, MAX_GAME_OBJECTS> &GetGameObjectsWithTag(GameObjectTag tag);
  static const eastl::array<GameObject, MAX_GAME_OBJECTS> &GetGameObjects(void);
  static GameObject *GetGameObjectByName(const char *name);
  static void Dump(void);
};
```

- **Active vs. renderable:** "active" objects are all objects currently alive in the world; "renderable" is meant to be a separate, explicitly-set subset (`SetRenderableGameObjects`), useful for e.g. only rendering objects in the current room/cell. The mechanism exists but nothing in the engine calls `SetRenderableGameObjects` today, so `GetActiveGameObjects` always falls through to a full scan of all 250 slots every frame.
- **`Dump`** frees every game object at once — intended for scene teardown (see `MadnightEngine::HardLoadingScreen`), not for per-object cleanup.

### Usage

```cpp
// query everything tagged as environment geometry, e.g. to feed collision checks
auto walls = GameObjectManager::GetGameObjectsWithTag(GameObjectTag::ENVIRONMENT);
for (auto *wall : walls) {
    CollisionTest result;
    if (Collision::IsSATCollision(player->obb(), wall->obb(), &result))
        player->SetPosition(player->pos() + result.mtv);
}

// restrict rendering to just the objects in the current room
eastl::fixed_vector<GameObject*, 32> roomObjects = /* ...gathered elsewhere... */;
GameObjectManager::SetRenderableGameObjects(roomObjects);
// later, e.g. leaving the room:
GameObjectManager::ClearRenderableGameObjects(); // falls back to all active objects
```

### Internals

- Finding a free slot is a linear scan over all 250, which is fine normally but worth knowing if you're creating/destroying many objects in one frame. If the pool is full, `CreateGameObject` returns `null`.
- `GetActiveGameObjects()` silently returns the renderable list instead if one's been set via `SetRenderableGameObjects` — call `ClearRenderableGameObjects()` to go back to "all active objects".
- `GetGameObjectsWithTag` and `GetActiveGameObjects` share the same internal scratch buffer — don't hold a reference from one across a call to the other.

## Billboard

`src/core/billboard/billboard.hh`

A camera-facing textured quad — position, size, colour, texture, and UVs, with no mesh required. Base class for [`Particle`](#particle).

```cpp
class Billboard {
public:
  Billboard() = default;
  Billboard(eastl::fixed_string<char, MAX_BILLBOARD_NAME_LENGTH> name, psyqo::Vec3 pos, psyqo::Vec2 size, uint8_t id);

  void Destroy(void);

  const eastl::fixed_string<char, MAX_BILLBOARD_NAME_LENGTH> &name() const;
  const uint8_t &id() const;
  const psyqo::Vec3 &pos() const;
  void SetPosition(const psyqo::Vec3 pos);
  const psyqo::Vec2 &size() const;
  void setSize(const psyqo::Vec2 size);
  const psyqo::Color &colour() const;
  void SetColour(const psyqo::Color colour);
  const TimFile *pTexture() const;
  void SetTexture(const eastl::fixed_string<char, MAX_ARCHIVE_FILE_NAME_LEN> &textureName, const eastl::array<psyqo::PrimPieces::UVCoords, 4> &uv);
  void SetTexture(TimFile *texture, const eastl::array<psyqo::PrimPieces::UVCoords, 4> &uv);
  const eastl::array<psyqo::Vec3, 4> &corners() const;
  const eastl::array<psyqo::PrimPieces::UVCoords, 4> &uv() const;
  void SetUVCoords(const eastl::array<psyqo::PrimPieces::UVCoords, 4> &uv);
};
```

### Usage

```cpp
Billboard *glow = BillboardManager::CreateBillboard("torch_glow", torchPos, {1.0_ws, 1.0_ws});
glow->SetColour({255, 200, 120});
// no SetTexture call -> renders as a flat Gouraud-shaded quad instead of a textured one
```

### Internals

- Corners are stored flat, centered on the origin — camera-facing happens entirely on the render side (`Renderer::RenderBillboards`), not in `Billboard` itself.
- Skipping `SetTexture` is a valid, supported state — it just renders as a flat coloured quad instead of a textured one.

## BillboardManager

`src/core/billboard/billboard_manager.hh`

Fixed pool of up to `MAX_BILLBOARDS` (200) billboards, same create/destroy/lookup pattern as `GameObjectManager`.

```cpp
class BillboardManager final {
public:
  static Billboard* CreateBillboard(const eastl::fixed_string<char, MAX_BILLBOARD_NAME_LENGTH> name, psyqo::Vec3 pos, psyqo::Vec2 size);
  static void DestroyBillboard(Billboard* billboard);
  static const eastl::fixed_vector<Billboard*, MAX_BILLBOARDS> &GetActiveBillboards(void);
  static const eastl::array<Billboard, MAX_BILLBOARDS> &GetBillboards(void);
  static Billboard* GetBillboardByName(const eastl::fixed_string<char, MAX_BILLBOARD_NAME_LENGTH> name);
};
```

Same create/destroy/slot-reuse pattern as [`GameObjectManager`](#gameobjectmanager), just over a 200-entry pool.

## Particle

`src/core/particles/particle.hh`

A `Billboard` subclass that interpolates size, colour, and velocity from a start to an end value over its lifetime, then reports itself dead. You don't normally create these directly — [`ParticleEmitter`](#particleemitter) spawns and owns them.

```cpp
class Particle final : public Billboard {
public:
  Particle() = default;
  Particle(const psyqo::Vec3 pos, const psyqo::Vec2 size, const psyqo::Color colour, const psyqo::Vec3 velocity, const uint8_t lifetime = 1);
  Particle(const psyqo::Vec3 pos, const psyqo::Vec2 startSize, const psyqo::Vec2 endSize,
           const psyqo::Color startColour, const psyqo::Color endColour,
           const psyqo::Vec3 startVelocity, const psyqo::Vec3 endVelocity, const uint8_t lifetime = 1);

  void Process(const uint32_t &deltaTime);
  const bool IsDead(void) const;
};
```

`lifetime` is in whole seconds; `Process` advances the particle's age and interpolates its visual/velocity state accordingly. `deltaTime`, here and everywhere else it shows up in the engine, is the GPU vsync frame count elapsed since the last call (whatever `Renderer::Process` returned), not milliseconds or microseconds.

### Internals

- `Process` lerps colour, size, and velocity together based on age/lifetime, then applies velocity as a straight per-frame displacement (not an accumulated integration).

## ParticleEmitter

`src/core/particles/particle_emitter.hh`

Spawns `Particle`s at a configurable rate from a spherical volume, with shared start/end size, colour, velocity, and optional 2D-only motion. Not constructed directly — see [`ParticleEmitterManager`](#particleemittermanager).

```cpp
class ParticleEmitter final {
public:
  const eastl::fixed_string<char, MAX_PARTICLE_EMITTER_NAME_LENGTH> &name() const;
  const uint8_t &id() const;

  void Start(void);
  void Stop(void);
  void Destroy(void);
  void Process(const uint32_t &deltaTime);
  const eastl::vector<Particle> &particles() const;

  void SetRotation(const EmitterRotation &rotation);
  void SetParticles2D(const bool &is2D);
  void SetParticleVelocity(const psyqo::Vec3 &particleVelocity);
  void SetParticleVelocity(const psyqo::Vec3 &particleVelocity, const psyqo::Vec3 &particleEndVelocity);
  void SetParticleSize(const psyqo::Vec2 &particleSize);
  void SetParticleSize(const psyqo::Vec2 &particleSize, const psyqo::Vec2 &particleEndSize);
  void SetParticleColour(const psyqo::Color &particleColour);
  void SetParticleColour(const psyqo::Color &particleColour, const psyqo::Color &particleEndColour);
  void SetParticleTexture(const eastl::fixed_string<char, MAX_ARCHIVE_FILE_NAME_LEN> &textureName, const eastl::array<psyqo::PrimPieces::UVCoords, 4> &uv);
  void SetParticleUVCoords(const eastl::array<psyqo::PrimPieces::UVCoords, 4> &uv);

  const TimFile *pParticleTexture() const;
  const bool &AreParticles2D() const;
};
```

- Constructed with a name, id, position, spawn radius, particles-per-second, and per-particle lifetime in seconds — `maxParticles` and `spawnRate` are derived from those automatically.
- The single-value `Set*` overloads set both the start and end value to the same thing (no interpolation over lifetime); the two-value overloads set distinct start/end values for the particle to lerp between.
- `SetRotation` applies an emitter-space rotation matrix so particles are emitted in a consistent cone/spread direction, then rotated into world space.

### Usage

```cpp
ParticleEmitter *sparks = ParticleEmitterManager::CreateParticleEmitter(
    "torch_sparks", torchPos, /*radius*/ 0.1_ws, /*particlesPerSecond*/ 8, /*lifetimeSecs*/ 2);

sparks->SetParticleSize({0.1_ws, 0.1_ws}, {0.02_ws, 0.02_ws});       // shrink over life
sparks->SetParticleColour({255, 180, 60}, {80, 20, 20});             // orange -> dark red
sparks->SetParticleVelocity({0.0_ws, 0.5_ws, 0.0_ws}, {0.0_ws, 0.1_ws, 0.0_ws}); // rise, then slow
sparks->Start();

// per-frame, wherever your emitters get updated:
sparks->Process(deltaTime);
```

### Internals

- `Process` advances elapsed time using the `deltaTime` you pass in: spawn timing and particle ageing both track whatever value you feed it each frame.
- Even while stopped (`Stop()`), `Process` still advances and prunes existing particles — only *new* spawns are gated on `Start()`/`Stop()`.
- Spawn points land on the circumference of a ring around the emitter, not scattered through a sphere's volume — despite the "spherical volume" framing in the header.

## ParticleEmitterManager

`src/core/particles/particle_manager.hh`

Fixed pool of up to `MAX_PARTICLE_EMITTERS` (3) emitters.

```cpp
class ParticleEmitterManager final {
public:
  static ParticleEmitter* CreateParticleEmitter(const eastl::fixed_string<char, MAX_PARTICLE_EMITTER_NAME_LENGTH> &name, const psyqo::Vec3 &pos, const psyqo::FixedPoint<> &radius, const uint8_t &particlesPerSecond, const uint8_t &particleLifeTimeSecs);
  static void DestroyParticleEmitter(ParticleEmitter* emitter);
  static const eastl::fixed_vector<ParticleEmitter*, MAX_PARTICLE_EMITTERS> &GetActiveEmitters(void);
  static const eastl::array<ParticleEmitter, MAX_PARTICLE_EMITTERS> &GetEmitters(void);
  static ParticleEmitter* GetEmitterByName(const eastl::fixed_string<char, MAX_PARTICLE_EMITTER_NAME_LENGTH> name);
};
```

:::note Only 3 emitters at once
`MAX_PARTICLE_EMITTERS` is 3 — noticeably smaller than the 200/250-entry pools for billboards and game objects. Budget emitters carefully (e.g. one for the player, one or two for the current room's environmental effects) rather than one per particle-emitting object in a scene.
:::

## PerfMonitor

`src/core/debug/perf_monitor.hh`

A small on-screen HUD reporting FPS, heap usage, and rendered-vs-total game object counts, built on the engine's own [`GameplayHUD`](./ui#gameplayhud). Intended to be called last in your render loop.

```cpp
class PerfMonitor final {
public:
  // this should be called last in your render loop
  static void Render(uint32_t deltaTime);
  static void SetRenderedGameObjects(uint8_t renderedObjects, uint8_t totalObjects);
};
```

### Usage

```cpp
void GameplayScene::frame() {
  auto &renderInstance = Renderer::Instance();
  uint32_t deltaTime = renderInstance.Process();
  if (deltaTime == 0) return;

  renderInstance.Render();
  PerfMonitor::Render(deltaTime); // last, after everything else has drawn
}
```

### Internals

- No `Init()` call needed — it lazily sets itself up the first time `Render` runs.
- The FPS shown is a 30-frame rolling average, not an instantaneous per-frame value, so it updates a couple of times a second rather than every frame.

## DebugMenu

`src/core/debug/debug_menu.hh`

An in-engine debug overlay. It's toggled by holding L1 + L2 + R1 + R2 together (all four within a 30-frame window); Triangle closes it while it's open. Exposes a raycast-distance setting used by the gameplay scene, and a toggle for whether `PerfMonitor` renders.

```cpp
class DebugMenu final {
public:
  static void Init(void);
  static void Process(void);
  static void Draw(psyqo::GPU &gpu);
  static bool IsEnabled();
  static uint8_t RaycastDistance();
  static bool DisplayDebugHUD();
};
```

`DisplayDebugHUD()` gates whether [`PerfMonitor`](#perfmonitor) renders at all: `src/scenes/gameplay.cpp` only calls `PerfMonitor::Render` when it's true.

Both Up and Down cycle through the menu's options, wrapping at each end.

## Collision types

`src/core/collision_types.hh` — shared by [Physics & Collision](./physics-and-collision), `GameObject`, and `MeshBin`.

```cpp
enum CollisionType { SOLID, TRIGGER };

struct OBB {
  psyqo::Vec3 center;
  psyqo::Vec3 axes[3];
  psyqo::Vec3 halfExtents;
  uint32_t flags = 0; // reserved
};

struct AABBCollision {
  psyqo::Vec3 min;
  psyqo::Vec3 max;
};
```

## World units

`src/core/world_defs.hh`

```cpp
static constexpr psyqo::FixedPoint<> ONE_METRE = 0.03125_fp; // 128 units = 1m
```

The engine's world scale: 128 engine units per metre, matching the scale baked into exported `.MESHBIN`/`.COLBIN` assets (see the [Asset Pipeline Guides](../guides/overview)). See also the `_ws`/`_ws10` literals in [Helpers](./helpers#world-space-literals) for converting Blender units directly.

---
title: Core
sidebar_position: 2
---

# Core

`src/core/` has the things you place in the world (game objects, billboards, particle emitters), the pool that backs them, and the in-engine debug tools.

All the managers on this page are members of `g_madnightEngine`, not static classes. See [Updating the Engine](../getting-started/updating-the-engine#v001-instance-based-managers) if you are coming from an older version.

## GameObject

`src/core/object/gameobject.hh`

A `GameObject` is a thing in the world: a position, a rotation, and optionally a mesh, a texture and a collision box. You never construct one yourself. Ask [`GameObjectManager`](#gameobjectmanager) for one and it hands you a pointer into its pool.

### Creating and setting up an object

Create the object, then give it a mesh and texture. `SetMesh` and `SetTexture` look the asset up by the name you loaded it under, so the asset has to be loaded already (see [Loading Assets](../getting-started/loading-assets)). The names are the archive paths.

```cpp
auto *street = g_madnightEngine.m_gameObjectManager.CreateGameObject(
    "STREET", {0, 0, 0}, {0, 0, 0}, GameObjectTag::ENVIRONMENT);
if (street != nullptr) {
  street->SetMesh("MODELS/STREET.MB");
  street->SetTexture("TEXTURES/STREET.TIM");
}
```

This is the pattern `src/game.cpp` uses. `CreateGameObject` returns `nullptr` when all 250 slots are taken, so check the result. An object with no mesh is skipped by the renderer, so it is invisible but still exists for lookups and collision.

Move and turn it with `SetPosition` and `SetRotation`. Both keep the object's collision box in step for you.

```cpp
street->SetPosition({0.05_fp, 0, 0.5_fp});
street->SetRotation(0, 1.0_pi, 0.25_pi);
```

`pos()`, `rotation()`, `mesh()`, `texture()`, `tag()`, `name()`, `nameHash()` and `id()` read the current state back. `posPtr()` gives you a pointer to the position, which is what [`Camera::SetFollow`](./render#camera) wants.

### Triggers

`SetAsTrigger(size)` marks an object as a trigger volume of the given size. A trigger needs no mesh.

```cpp
auto *door = g_madnightEngine.m_gameObjectManager.CreateGameObject(
    "door_trigger", doorPos, {0, 0, 0}, GameObjectTag::INTERACTABLE);
door->SetAsTrigger({1.0_ws, 2.0_ws, 1.0_ws});
```

That is all it does. The engine has no overlap-only response, so testing whether the player is inside the volume is up to you, with `door->obb()` and [`Collision::IsSATCollision`](./physics-and-collision#collision). [`Raycast`](./physics-and-collision#raycast) also works on triggers.

### Flags

There are two separate sets of per-object flags, and both take a bit number, not a mask. `SetFlag(3)` sets bit 3, it does not set the value 3.

`HasFlag`, `SetFlag`, `ClearFlag` and `ClearFlags` are yours. The engine never reads them, so use them for state like "already collected".

```cpp
constexpr uint32_t FLAG_COLLECTED = 0;
if (!coin->HasFlag(FLAG_COLLECTED)) {
  coin->SetFlag(FLAG_COLLECTED);
}
```

`HasRenderFlag`, `SetRenderFlag`, `ClearRenderFlag` and `ClearRenderFlags` take a `RenderFlags` value, and the renderer does read one of them:

| Flag | Effect |
| --- | --- |
| `RF_QUICK_ADVANCED_FOG` | When [advanced fog](./render#advanced-fog) is on, fog this object with one quad instead of drawing every face twice. See [Quick advanced fog](./render#quick-advanced-fog). |
| `RF_DISTANCE_CHECK` | Not used by anything yet. |

The renderer checks `RF_QUICK_ADVANCED_FOG` for each object when it draws it, and only while advanced fog is enabled. `RF_DISTANCE_CHECK` is never looked at, whether set or not. Whether an object is drawn at all depends only on whether its bounding sphere is on screen.

```cpp
hero->SetRenderFlag(RF_QUICK_ADVANCED_FOG);
```

:::caution[Flags are not cleared when an object is destroyed]
`DestroyGameObject` resets the position, mesh, texture, tag and collision state, but not the flags or render flags. When a slot is reused, the new object can inherit the old one's flags, so call `ClearFlags()` and `ClearRenderFlags()` right after creating an object if you rely on them. Tracked in [#93](https://github.com/Forceh91/psyqo-madnight-engine/issues/93).
:::

### GameObjectTag

```cpp
enum GameObjectTag { NONE, ENVIRONMENT, INTERACTABLE, PORTAL, PORTAL_ENTRANCE, PORTAL_EXIT };
```

The tag is what you filter on with `GetGameObjectsWithTag`, and what [`Raycast::RaycastScene`](./physics-and-collision#raycast) tests against. Pass it as the last argument of `CreateGameObject`; it defaults to `NONE`.

### Collision box

`obb()` returns the object's oriented bounding box, which is what you hand to `IsSATCollision`. For a normal object the box size comes from the collision box baked into the mesh at export time, and the box follows the object when you call `SetPosition` or `SetRotation`. You do not update it yourself. If you set the mesh after the object is created, the box is built from that mesh at that point.

## GameObjectManager

`src/core/object/gameobject_manager.hh`

Owns a fixed pool of 250 `GameObject`s (`MAX_GAME_OBJECTS`) and is the only way to create or destroy one.

### Finding objects

```cpp
auto &objects = g_madnightEngine.m_gameObjectManager;

GameObject *player = objects.GetGameObjectByName("player");   // nullptr if there is none
const auto &walls = objects.GetGameObjectsWithTag(GameObjectTag::ENVIRONMENT);
for (auto *wall : walls) {
  CollisionTest result;
  if (g_madnightEngine.m_collisionHelper.IsSATCollision(player->obb(), wall->obb(), &result)) {
    player->SetPosition(player->pos() + result.mtv);
  }
}
```

Names are hashed, so lookup compares a 64-bit hash and returns the first match. Keep names unique. Both `GetGameObjectByName` and `GetGameObjectsWithTag` walk the whole pool, so look an object up once and keep the pointer rather than searching every frame.

:::caution[One shared result list]
`GetGameObjectsWithTag` and `GetActiveGameObjects` fill the same internal list. Do not keep the reference from one across a call to the other, and note that `RaycastScene` calls `GetGameObjectsWithTag` internally.
:::

### Removing objects

```cpp
g_madnightEngine.m_gameObjectManager.DestroyGameObject(crate);
```

This frees the slot and wipes the object. Do not use the pointer afterwards. `Dump()` destroys every object at once. You rarely call it yourself, because the file loader dumps game objects, meshes, textures, collision data and sounds for you when it starts a loading screen. It does not touch billboards or particle emitters, so destroy those yourself when you change scene.

### Rendering only some objects

By default the renderer draws every live object. If you only want the ones in the current room, hand the manager a list:

```cpp
eastl::fixed_vector<GameObject *, 32> inRoom;
// ...fill it with the room's objects...
g_madnightEngine.m_gameObjectManager.SetRenderableGameObjects(
    eastl::span<GameObject *>(inRoom.data(), inRoom.size()));

// leaving the room: go back to rendering everything
g_madnightEngine.m_gameObjectManager.ClearRenderableGameObjects();
```

While a list is set, `GetActiveGameObjects()` returns that list instead of all live objects. Nothing in the engine sets one for you.

## Billboard

`src/core/billboard/billboard.hh`

A camera-facing quad with a position, a size, a colour and an optional texture. It needs no mesh, which makes it good for glows, light flares and simple sprites in 3D space. Create one with [`BillboardManager`](#billboardmanager).

```cpp
auto *glow = g_madnightEngine.m_billboardManager.CreateBillboard("torch_glow", torchPos, {1.0_ws, 1.0_ws});
if (glow != nullptr) {
  glow->SetColour({255, 200, 120});
}
```

With no texture it draws as a flat coloured quad. To texture it, pass the texture name and four UV corners:

```cpp
glow->SetTexture("TEXTURES/GLOW.TIM", uv); // uv is an eastl::array<psyqo::PrimPieces::UVCoords, 4>
```

`SetPosition`, `setSize` (note the lower-case `s`), `SetColour` and `SetUVCoords` change the rest. Set the colour yourself after creating a billboard rather than relying on the default.

Only the hash of the name is stored, so `Billboard` has no `name()`. Hold on to the name yourself if you need to display it.

## BillboardManager

`src/core/billboard/billboard_manager.hh`

A pool of 200 billboards (`MAX_BILLBOARDS`), created, destroyed and looked up the same way as game objects.

```cpp
auto &billboards = g_madnightEngine.m_billboardManager;
Billboard *glow = billboards.GetBillboardByName("torch_glow");
billboards.DestroyBillboard(glow);
```

`CreateBillboard` returns `nullptr` when the pool is full. `GetActiveBillboards()` lists the live ones.

## ParticleEmitter

`src/core/particles/particle_emitter.hh`

An emitter spawns short-lived particles around a point and moves them for you. A particle is a [`Billboard`](#billboard) that changes size, colour and velocity from a start value to an end value over its life. You never build particles yourself.

### Setting up an emitter

```cpp
auto *sparks = g_madnightEngine.m_particleEmitterManager.CreateParticleEmitter(
    "torch_sparks", torchPos, /*radius*/ 0.1_ws, /*per second*/ 8, /*lifetime secs*/ 2);
if (sparks != nullptr) {
  sparks->SetRotation({0, 0, 0});
  sparks->SetParticleTexture("TEXTURES/SPARK.TIM", uv);
  sparks->SetParticleSize({0.1_ws, 0.1_ws}, {0.02_ws, 0.02_ws});         // shrink over life
  sparks->SetParticleColour({255, 180, 60}, {80, 20, 20});               // orange to dark red
  sparks->SetParticleVelocity({0.0_ws, 0.5_ws, 0.0_ws}, {0.0_ws, 0.1_ws, 0.0_ws}); // rise, then slow
  sparks->Start();
}
```

The `Set*` calls with one value use it for both the start and the end. The two-value versions give the start and end values the particle blends between. The maximum number of live particles is `particlesPerSecond * lifetimeSecs`, so the example above never has more than 16.

Particles are 2D sprites by default, drawn at a size that scales with distance. A 2D particle uses only the first of the four UV corners you pass. `SetParticles2D(false)` switches to full 3D quads, which cost more.

### Updating

Nothing in the engine calls `Process` on an emitter, so call it once per frame from your scene. The renderer only draws the particles an emitter currently has.

```cpp
void GameplayScene::frame() {
  uint32_t deltaTime = Renderer::Instance().Process();
  if (deltaTime == 0) return;

  sparks->Process(deltaTime);
  Renderer::Instance().Render();
}
```

`Stop()` stops new particles but the ones already alive finish their lifetime. `Start()` resumes spawning.

:::note[The deltaTime argument is not used]
`ParticleEmitter::Process` measures elapsed time from the GPU clock itself and ignores the `deltaTime` you pass. You still have to call it every frame, but passing a different value changes nothing.
:::

:::caution[Call SetRotation before you expect particles to move]
`SetRotation` is what builds the emitter's rotation matrix. A pooled emitter does not get one from `CreateParticleEmitter`, so until you call `SetRotation` (even with zero angles, as above) the particle velocities are rotated by an empty matrix and every particle stays where it spawned. Tracked in [#94](https://github.com/Forceh91/psyqo-madnight-engine/issues/94).
:::

Particles are spawned at a random point on a flat circle of the emitter's radius (in the X/Z plane), not inside a sphere. `SetRotation` tilts the emitter so the circle and the particle velocities rotate with it.

## ParticleEmitterManager

`src/core/particles/particle_manager.hh`

```cpp
auto &emitters = g_madnightEngine.m_particleEmitterManager;
ParticleEmitter *sparks = emitters.GetEmitterByName("torch_sparks");
emitters.DestroyParticleEmitter(sparks);
```

:::note[Only 3 emitters at once]
`MAX_PARTICLE_EMITTERS` is 3, much smaller than the 200 and 250 slot pools for billboards and game objects. `CreateParticleEmitter` returns `nullptr` when all three are in use. Budget them, for example one for the player and one or two for the current room, rather than one per object that emits something.
:::

## Pool

`src/pool/pool.hh`

The managers share one small fixed-size pool type to hand out slots. You do not use it directly, but it explains a few behaviours:

- Creating something when every slot is taken returns `nullptr` (the pool reports `INVALID_POOL_ID`).
- An object's `id()` is its slot number. A free slot has `id() == INVALID_POOL_ID`.
- Freeing a slot makes its ID available again straight away, so an ID from a destroyed object can later belong to a new one.
- The pool does not clean up the object in a freed slot. Each manager's `Destroy*` function does that.

The game object, billboard, particle emitter, mesh, texture and sound managers all use it.

## PerfMonitor

`src/core/debug/perf_monitor.hh`

A small on-screen readout of the heap size, the frame rate and how many game objects were drawn out of the total (for example `FPS: 60 (12/40 GO)`). Call `Render(deltaTime)` last in your frame so it draws on top.

```cpp
void GameplayScene::frame() {
  auto &renderInstance = Renderer::Instance();
  uint32_t deltaTime = renderInstance.Process();
  if (deltaTime == 0) return;

  renderInstance.Render();
  g_madnightEngine.m_perfMonitor.Render(deltaTime);
}
```

It sets itself up the first time `Render` runs. The frame rate is averaged over 30 frames, so it updates a couple of times a second. The renderer fills in the game object counts for you.

## DebugMenu

`src/core/debug/debug_menu.hh`

An in-engine overlay for changing debug settings while the game runs. Press L1, L2, R1 and R2 all within 30 frames to open it (it watches for the buttons being released), and press Triangle to close it. Up and Down move between options, Left and Right change the selected one.

It has two options: a raycast distance from 1 to 10 (default 3), and a toggle for whether the performance readout shows. The engine initialises and draws the menu itself. Your game only reads the settings:

```cpp
auto &menu = g_madnightEngine.m_debugMenu;
Ray ray = {.origin = camera->pos(), .direction = camera->forwardVector(),
           .maxDistance = menu.RaycastDistance() * ONE_METRE};

if (menu.DisplayDebugHUD() && !menu.IsEnabled()) {
  g_madnightEngine.m_perfMonitor.Render(deltaTime);
}
```

`GameplayScene::frame` does exactly this: it skips the performance readout while the menu is open.

## Collision types

`src/core/collision_types.hh`

```cpp
enum CollisionType { SOLID, TRIGGER };
```

The three shapes the collision code works with are `OBB` (a centre, three axes and half-extents), `AABBCollision` (a min and a max corner) and the `CollisionType` above. [Physics & Collision](./physics-and-collision) shows how to use them.

## World units

`src/core/world_defs.hh`

```cpp
static constexpr psyqo::FixedPoint<> ONE_METRE = 0.5_ws;
```

One Blender unit is 2 m, so `1.0_ws` is 2 m and `ONE_METRE` is `0.5_ws`. Use `ONE_METRE` when you need a distance in metres, for example a ray length; `5.0_ws` is 10 m. To place things using the numbers Blender shows you, use the `_ws` literal from [Helpers](./helpers#world-space-literals), which also explains the scale convention. See the [Asset Pipeline Guides](../guides/overview) for how exported assets are scaled.

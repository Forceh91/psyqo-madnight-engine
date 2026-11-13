---
title: Updating the Engine
sidebar_position: 6
---

# Updating the Engine

Since the engine is a submodule, pull in upstream changes with:

```bash
cd madnight_engine
git pull origin main
cd ..
git add madnight_engine
git commit -m "Update madnight_engine submodule"
```

The engine pins its own copy of nugget (psyqo), so update that too after pulling:

```bash
cd madnight_engine
git submodule update --init --recursive
```

## v0.1.0 to v1.0.0

v1.0.0 needs the nugget revision it pins. The renderer uses `psyqo::ScratchpadStackful`, which the v0.1.0 nugget does not have, so run the `git submodule update` above or the engine will not build.

No asset format changed between these versions and the exporters in `tools/` are the same, so `.MESHBIN`, `.ANIMBIN`, `.COLBIN` and `.scenebin` files do not need re-exporting.

### Camera position is no longer applied twice

The renderer used to subtract the camera position from every object and then add the camera translation again, so a `FIXED` or `FREE_LOOK` camera at `c` drew the scene as if it were at `2 * c`. `FOLLOW` mode cancelled this out and drew correctly. As of v1.0.0 every mode draws from the position you give the camera ([#121](https://github.com/Forceh91/psyqo-madnight-engine/pull/121)).

What to do, for `FIXED` and `FREE_LOOK` cameras:

- If you set a camera position by eye, double it to get the same view back.
- If you placed positions that get compared with the camera or player position (triggers, spawn points, raycast targets) so they lined up with what was on screen, double those too.
- Placing meshes relative to each other is unaffected.

The example from #121, an exit trigger placed to match a level where the camera moves:

```cpp
// before
m_exitLevelTrigger = g_madnightEngine.m_gameObjectManager.CreateGameObject(
    "EXIT_LEVEL", {0, 0, -26.0_ws}, {0, 0, 0}, PORTAL_EXIT);

// after
m_exitLevelTrigger = g_madnightEngine.m_gameObjectManager.CreateGameObject(
    "EXIT_LEVEL", {0, 0, -52.0_ws}, {0, 0, 0}, PORTAL_EXIT);
```

`Camera::deltaOffset()`, which only existed to undo the doubling in `FOLLOW` mode, is gone.

### LookAt is no longer mirrored

`Camera::LookAt` built its right vector the wrong way round, so a view from `LookAt` was mirrored left to right. That includes `FOLLOW` mode, which calls `LookAt` every frame ([#123](https://github.com/Forceh91/psyqo-madnight-engine/pull/123)). If you flipped an axis or the orbit input to make it look right, remove that.

### ONE_METRE is half its old size

`ONE_METRE` was `0.03125_fp` (128 units, which is one Blender unit). It is now `0.5_ws` (64 units), matching the convention that one Blender unit is 2 m ([#92](https://github.com/Forceh91/psyqo-madnight-engine/pull/92)). The `_ws` literal did not change.

Anything you wrote as `N * ONE_METRE` is now half as long. To keep the old length, double `N` or switch to the literal:

```cpp
// before: 10 * 128 units
.maxDistance = 10 * ONE_METRE

// after: same length
.maxDistance = 20 * ONE_METRE // or 10.0_ws
```

### Raycasts

- `maxRayDistance` is now `10.0_ws` (20 m) ([#129](https://github.com/Forceh91/psyqo-madnight-engine/pull/129)). That is the same number of units as v0.1.0's `ONE_METRE * 10`, so the limit has not moved, but in the new `ONE_METRE` it allows up to `20 * ONE_METRE`.
- A game object with a mesh is now tested against its mesh box moved to the object's position. v0.1.0 tested the box as if the object were at the origin ([#99](https://github.com/Forceh91/psyqo-madnight-engine/pull/99)). Remove any offset you added to the ray to make up for it.

### Fog constants removed

`NEAR_FOG_DISTANCE` and `FULL_FOG_DISTANCE` are no longer in `renderer.hh` ([#143](https://github.com/Forceh91/psyqo-madnight-engine/pull/143)). They only set the fog on 2D particles. There is nothing to replace them with in game code: the near and far distances now come from the fog mode `StartScene()` sets up, the same ones meshes use. Delete any reference to the two constants.

Other fog behaviour that changed in the same release:

- While simple or advanced fog is on, a game object whose bounding sphere is entirely past the far fog distance is not drawn at all. Before, it was drawn fully fogged. If your clear colour differs from the fog colour, far objects now disappear rather than showing as fog-coloured shapes.
- 2D particles are no longer fogged with fog off. v0.1.0 fogged them anyway. See [Fog on billboards and particles](../api/render#fog-on-billboards-and-particles).

### atan2_fixed is deprecated

`atan2_fixed` is marked `[[deprecated]]`, so calling it now gives a compiler warning ([#79](https://github.com/Forceh91/psyqo-madnight-engine/pull/79)). Use `psyqo::atan2` from `<psyqo/atan2.hh>`. It takes the same `(y, x)` order and returns a `psyqo::Angle` in the same `(-1.0_pi, 1.0_pi]` range, but takes `int32_t` and only the ratio of the two matters, so you no longer need to scale inputs into -128 to 127.

```cpp
// before
psyqo::Angle yaw = atan2_fixed(dz, dx);

// after
#include <psyqo/atan2.hh>
psyqo::Angle yaw = psyqo::atan2(dz, dx);
```

### Particles

- Emitters spawn at the rate you ask for. Below 16 particles per second the spawn interval overflowed and they spawned faster than asked ([#127](https://github.com/Forceh91/psyqo-madnight-engine/pull/127)), and an emitter now spawns more than one particle in a frame when its rate needs it ([#139](https://github.com/Forceh91/psyqo-madnight-engine/pull/139)). Retune particle counts you picked by eye.
- Slow particles move at short frame times. Their position now comes from their age instead of adding `velocity * deltaTime` each frame, which rounded to zero ([#140](https://github.com/Forceh91/psyqo-madnight-engine/pull/140)).
- An emitter builds its rotation matrix when it is created, so particles move without a `SetRotation` call ([#98](https://github.com/Forceh91/psyqo-madnight-engine/pull/98)). Calling `SetRotation({0, 0, 0})` as a workaround is harmless but no longer needed.

### Smaller behaviour changes

- `DestroyGameObject` now also clears flags, render flags and the quad type, so a reused slot starts clean ([#97](https://github.com/Forceh91/psyqo-madnight-engine/pull/97)). You can drop `ClearFlags()`/`ClearRenderFlags()` calls you added after `CreateGameObject` for this.
- Disabled menu items no longer react to the confirm button or custom buttons ([#82](https://github.com/Forceh91/psyqo-madnight-engine/pull/82)).
- MOD music: the volume starts at `DEFAULT_MUSIC_VOLUME` instead of 0, calling `PlayMusic` while music is playing replaces the update timer instead of adding a second one, and `LoadMODSound` stops the current music before loading ([#117](https://github.com/Forceh91/psyqo-madnight-engine/pull/117)).
- VAG samples loaded before the first hard load now go to the right place in SPU RAM and play ([#116](https://github.com/Forceh91/psyqo-madnight-engine/pull/116)).
- Skeletal animations blend between keys instead of snapping to the earlier key, slerp takes the short way round, and `LerpAngle` takes the short way across +-180 degrees ([#86](https://github.com/Forceh91/psyqo-madnight-engine/pull/86)).
- Bright ambient colours saturate at 255 instead of wrapping round to dark ([#90](https://github.com/Forceh91/psyqo-madnight-engine/pull/90)).
- Subdivision stops splitting a face once its longer side on screen is under 32 pixels (`SUBDIVISION_SCREEN_CUTOFF` in `renderer.hh`, 0 turns it off) ([#133](https://github.com/Forceh91/psyqo-madnight-engine/pull/133)).
- Builds at `-Os` and `-O0` run the face loop on a stack in main RAM, because subdivision overflows the scratchpad stack there ([#146](https://github.com/Forceh91/psyqo-madnight-engine/pull/146)). Your game's `BUILD` setting needs no change. The tech demo in this repository builds with `BUILD ?= ReleaseFast` ([#144](https://github.com/Forceh91/psyqo-madnight-engine/pull/144)).

## v0.0.1: instance-based managers

The engine's managers used to be classes with `static` methods over static state. As of v0.0.1 they're **non-`static` members owned by `MadnightEngine`**, so every call site needs an instance to call through — the global `g_madnightEngine`:

```cpp
// before
MeshBin *crateMesh;
co_await MeshManager::LoadMesh("crate", &crateMesh);

// after
MeshBin *crateMesh;
co_await g_madnightEngine.m_meshManager.LoadMesh("crate", &crateMesh);
```

The member names, all public on `MadnightEngine` unless noted:

| Manager | Member |
| --- | --- |
| [`TextureManager`](../api/textures) | `m_textureManager` |
| [`MeshManager`](../api/mesh-and-animation#meshmanager) | `m_meshManager` |
| [`GameObjectManager`](../api/core#gameobjectmanager) | `m_gameObjectManager` |
| [`ColbinManager`](../api/physics-and-collision#colbinmanager) | `m_colbinManager` |
| [`SoundManager`](../api/sound#soundmanager) | `m_soundManager` |
| [`ModSoundManager`](../api/sound#modsoundmanager) | `m_modSoundManager` |
| [`AnimationManager`](../api/mesh-and-animation#animationmanager) | `m_animationManager` |
| [`BillboardManager`](../api/core#billboardmanager) | `m_billboardManager` |
| [`ParticleEmitterManager`](../api/core#particleemittermanager) | `m_particleEmitterManager` |
| [`FileLoader`](../api/helpers#fileloader) | `m_fileLoader` |
| [`ArchiveHelper`](../api/helpers#archivehelper) | `m_archiveHelper` |
| [`ControllerHelper`](../api/controller) | `m_controllerHelper` |
| [`Collision`](../api/physics-and-collision#collision) | `m_collisionHelper` |
| [`Raycast`](../api/physics-and-collision#raycast) | `m_raycast` |
| [`SkeletonController`](../api/mesh-and-animation#skeleton--skeletoncontroller) | `m_skeletonController` |
| [`DebugMenu`](../api/core#debugmenu) | `m_debugMenu` |
| [`PerfMonitor`](../api/core#perfmonitor) | `m_perfMonitor` |

[`Renderer`](../api/render#renderer) (`Renderer::Instance()`) and [`Lighting`](../api/render#lighting) (`Lighting::instance()`) are unaffected — they're still classic singletons.

`CDRomHelper` is now `[[deprecated]]` in favour of `ArchiveHelper`, and its instance (`m_cdromHelper`) is a *private* member of `MadnightEngine` — game code shouldn't reach for it directly.

A few other things changed alongside the static-to-instance move, worth knowing about when pulling in v0.0.1:

- Most name parameters across the API (mesh/texture/VAG/animation names, etc.) switched from `const char *` to `eastl::string_view`.
- Several types — `TimFile`, `VagEntry`, `Billboard` — now store a hashed `uint64_t` name (`nameHash()`) instead of the full string. A `HashName()` helper (`src/helpers/archive.hh`) hashes a runtime string or folds a string literal to a compile-time constant, matching how these managers hash names internally — precompute a hash once with it if you're going to look the same name up repeatedly, rather than re-hashing a string_view/fixed_string every call.
- `GameObject`, `Billboard`, `Particle`, and `ParticleEmitter` default constructors are now `private` (`protected` for `Billboard`, since `Particle` derives from it), with their owning manager (`GameObjectManager`, `BillboardManager`, `ParticleEmitterManager`) as a `friend` — and `Particle`'s is `ParticleEmitter`, not a manager, since particles are spawned by their emitter. They were already only meant to be created via `Create*`/their emitter, this just makes it compiler-enforced.
- `ControllerHelper::IsPadAnalog` is gone — `GetNormalizedAnalogStickInput` now checks pad connection/analog support internally (via psyqo's own `hasAnalog`) and safely returns `0` if the pad isn't analog, so there's no need to check first.
- `Menu::Activate`/`Deactivate` were renamed `Enable`/`Disable` (and `SetOnActivate`/`SetOnDeactivate` to `SetOnEnable`/`SetOnDisable`) to match `GameplayHUD`'s naming.
- `Quaternion`'s components switched from `psyqo::GTE::Short` to full `psyqo::FixedPoint<>` ("full fat int32_t") for more precision headroom; `DotProduct` returns `psyqo::FixedPoint<>` to match. `FindRotationQuat` is commented out in-source and not currently available.
- `GTEMath` lost `MultiplyMatrix33`/`MultiplyMatrixVec3` — hot-path matrix/vector multiplication now goes through `psyqo::GteMath` directly instead of this wrapper; only `ProjectVectorOntoAxes` remains on `GTEMath`.

See the [API Reference](../api/overview) for the updated per-class signatures.

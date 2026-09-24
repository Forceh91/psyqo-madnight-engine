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
| `TextureManager` | `m_textureManager` |
| `MeshManager` | `m_meshManager` |
| `GameObjectManager` | `m_gameObjectManager` |
| `ColbinManager` | `m_colbinManager` |
| `SoundManager` | `m_soundManager` |
| `ModSoundManager` | `m_modSoundManager` |
| `AnimationManager` | `m_animationManager` |
| `BillboardManager` | `m_billboardManager` |
| `ParticleEmitterManager` | `m_particleEmitterManager` |
| `FileLoader` | `m_fileLoader` |
| `ArchiveHelper` | `m_archiveHelper` |
| `ControllerHelper` | `m_controllerHelper` |
| `Collision` | `m_collisionHelper` |
| `Raycast` | `m_raycast` |
| `SkeletonController` | `m_skeletonController` |
| `DebugMenu` | `m_debugMenu` |
| `PerfMonitor` | `m_perfMonitor` |

`Renderer` (`Renderer::Instance()`) and `Lighting` (`Lighting::instance()`) are unaffected — they're still classic singletons.

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

---
title: API Reference
sidebar_position: 1
---

# API Reference

This section documents the engine's public classes and functions, organized by subsystem. It's written from the engine source directly — where a class's intent isn't obvious from its name, the description below explains what it's for and how it's meant to be used.

As of v0.0.1, the engine's managers (`GameObjectManager`, `MeshManager`, `TextureManager`, `ColbinManager`, `SoundManager`, `ModSoundManager`, `AnimationManager`, `BillboardManager`, `ParticleEmitterManager`, `FileLoader`, `ArchiveHelper`, `ControllerHelper`, `Collision`, `Raycast`, `SkeletonController`, `DebugMenu`, and `PerfMonitor`) are **non-static instance members owned by `MadnightEngine`**, not static classes — call them off the global engine instance, e.g. `g_madnightEngine.m_gameObjectManager.CreateGameObject(...)` rather than `GameObjectManager::CreateGameObject(...)`. See [Updating the Engine → v0.0.1](../getting-started/updating-the-engine#v001-instance-based-managers) for the full list of member names.

A couple of things remain classic singletons reached via `Instance()`/`instance()` — `Renderer` and `Lighting` — and a handful of types (`GameObject`, `Camera`, `Billboard`, `ParticleEmitter`, `Menu`, HUD elements) are regular value/instance types you create and own yourself, usually via a manager's `Create*` call. Several of those (`GameObject`, `Billboard`, `Particle`, `ParticleEmitter`) now have **private constructors with their owning manager as a `friend`**, so they can only be constructed through that manager's `Create*` call, not directly in game code.

- [Core](./core): [`GameObject`](./core#gameobject), [`GameObjectManager`](./core#gameobjectmanager), [billboards](./core#billboard), [particles](./core#particleemitter), [debug tooling](./core#debugmenu)
- [Render](./render): [`Camera`](./render#camera), [`Renderer`](./render#renderer), [lighting](./render#lighting), [fog](./render#fog), [colour constants](./render#colour-constants), [clipping](./render#clipping)
- [Mesh & Animation](./mesh-and-animation): [`MeshManager`](./mesh-and-animation#meshmanager), [skeletons](./mesh-and-animation#skeleton--skeletoncontroller), [`AnimationManager`](./mesh-and-animation#animationmanager), [`Quaternion`](./mesh-and-animation#quaternion)
- [Physics & Collision](./physics-and-collision): [`Collision`](./physics-and-collision#collision), [`Raycast`](./physics-and-collision#raycast), [`ColbinManager`](./physics-and-collision#colbinmanager)
- [Controller](./controller): analog sticks and deadzones
- [Math](./math): [vectors](./math#vector), [lerp](./math#lerp), [trig](./math#trig), [matrices](./math#matrix), [random numbers](./math#rand)
- [Sound](./sound): [`SoundManager`](./sound#soundmanager) for VAG sound effects, [`ModSoundManager`](./sound#modsoundmanager) for MOD music
- [Textures](./textures): [loading](./textures#loading-a-texture), [VRAM placement](./textures#placing-textures-in-vram), [transparency](./textures#transparency-and-the-stp-bit)
- [UI](./ui): [`GameplayHUD`](./ui#gameplayhud), HUD elements, [`Menu`](./ui#menu)
- [Helpers](./helpers): [world-space literals](./helpers#world-space-literals), [`LoadQueue`](./helpers#loadqueue), [loading screens](./helpers#loading-screens), [`ArchiveHelper`](./helpers#archivehelper)

:::note[Source of truth]
These pages are written from the engine source. If something here disagrees with the code, the code is right, and a PR or an issue fixing the page is welcome.
:::

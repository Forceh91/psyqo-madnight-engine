---
title: Madnight Engine
slug: /
sidebar_position: 1
---

# Madnight Engine

:::info[AI Disclaimer]

AI usage in this repository has been limited to tooling and documentation generation. AI was not used to author any engine code, however existing logic in some areas may have been refactored with AI assistance.

:::

Madnight Engine is a reusable engine layer for PlayStation 1 homebrew, built on top of the [psyqo](https://github.com/grumpycoders/pcsx-redux/tree/main/src/mips/psyqo) / [nugget](https://github.com/pcsx-redux/nugget) framework. It's the engine behind Madnight Games' PS1 projects, open-sourced so anyone can build a game on top of it.

:::caution[Early days]

This engine is in an **ALPHA state**, however it is at the point where you should be able to at least get some form of 3D game working with it. My unreleased tech demo of [Supermarket Sweep](https://discord.com/channels/642647820683444236/1485343661439848508) runs off of it. Pull requests with fixes and improvements are very welcome.

:::

## What it gives you

- **Object & scene management** — [`GameObject`](./api/core#gameobject) / [`GameObjectManager`](./api/core#gameobjectmanager), plus a [`Camera`](./api/render#camera) with fixed, follow, and free-look modes
- **Rendering** — an ordering-table-based [`Renderer`](./api/render#renderer) with GTE-accelerated fog, ambient lighting, textured/Gouraud quads, and automatic subdivision of large polys
- **Collision & physics primitives** — AABB and SAT (OBB) collision tests, raycasting against tagged game objects, and CD-baked collision meshes (`.COLBIN`)
- **Skeletal animation** — quaternion-based skeletons and bone tracks (`.ANIMBIN`) driven by [`SkeletonController`](./api/mesh-and-animation#skeleton--skeletoncontroller)
- **Particles & billboards** — a lightweight [`ParticleEmitter`](./api/core#particleemitter)/[`Billboard`](./api/core#billboard) system for sprite-based effects
- **UI** — a [`GameplayHUD`](./api/ui#gameplayhud) for on-screen text/sprites, and a [`Menu`](./api/ui#menu) scene base class for interactive menus with configurable pad bindings
- **Audio** — SPU-backed VAG sample playback via [`SoundManager`](./api/sound#soundmanager), and MOD tracker music via [`ModSoundManager`](./api/sound#modsoundmanager)
- **Asset loading** — CD-ROM archive loading via [`ArchiveHelper`](./api/helpers#archivehelper) (`CDRomHelper` is deprecated), texture and mesh managers, all built around `psyqo::Coroutine`
- **Debug tooling** — an in-game debug menu and a performance monitor HUD (FPS, heap usage, rendered object counts)

## Where to go next

- New to the engine? Start with [Getting Started](./getting-started/overview).
- Working with Blender-exported assets? See the [Asset Pipeline Guides](./guides/overview).
- Looking up a class or function? Jump to the [API Reference](./api/overview).

## Credits

Huge thanks to the [PSX.Dev Discord](https://discord.gg/QByKPpH) and [Nicolas Noble](https://github.com/nicolasnoble) for their help getting this engine off the ground and for all things psyqo / PS1 hardware.

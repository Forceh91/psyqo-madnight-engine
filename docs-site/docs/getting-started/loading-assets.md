---
title: Loading Assets
sidebar_position: 5
---

# Loading Assets

If you wish to dynamically load assets rather than having them hardcoded as header files, take a look at pcsx-redux's [authoring tool](https://github.com/grumpycoders/pcsx-redux/tree/main/tools/authoring).

In the engine's `cdrom` directory you'll find an example [`toc.json`](https://github.com/Forceh91/psyqo-madnight-engine/blob/main/cdrom/toc.json) that you can pass into the authoring tool to generate your ISO.

At runtime, assets are loaded through the engine's managers:

- [`ArchiveHelper`](../api/helpers#archivehelper) for reading files off the CD, and a [`LoadQueue`](../api/helpers#loadqueue) for loading a batch of them behind a [loading screen](../api/helpers#loading-screens)
- [`TextureManager`](../api/textures#loading-a-texture) for `.TIM` textures
- [`MeshManager`](../api/mesh-and-animation#meshmanager) for `.MESHBIN` meshes
- [`ColbinManager`](../api/physics-and-collision#colbinmanager) for `.COLBIN` collision meshes
- [`AnimationManager`](../api/mesh-and-animation#animationmanager) for `.ANIMBIN` animations
- [`SoundManager`](../api/sound#soundmanager) for `.VAG` sound effects and [`ModSoundManager`](../api/sound#modsoundmanager) for MOD music

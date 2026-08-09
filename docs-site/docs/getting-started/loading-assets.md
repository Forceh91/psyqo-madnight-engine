---
title: Loading Assets
sidebar_position: 5
---

# Loading Assets

If you wish to dynamically load assets rather than having them hardcoded as header files, take a look at pcsx-redux's [authoring tool](https://github.com/grumpycoders/pcsx-redux/tree/main/tools/authoring).

In the engine's `cdrom` directory you'll find an example [`toc.json`](https://github.com/Forceh91/psyqo-madnight-engine/blob/main/cdrom/toc.json) that you can pass into the authoring tool to generate your ISO.

At runtime, assets are loaded through the engine's managers — see the [API Reference](../api/overview) for `ArchiveHelper`/`CDRomHelper` (CD-ROM file loading), `TextureManager`, `MeshManager`, `ColbinManager`, `AnimationManager`, `SoundManager`, and `ModSoundManager`.

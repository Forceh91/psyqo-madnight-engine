---
title: Helpers
sidebar_position: 11
---

# Helpers

`src/helpers/` — CD-ROM/archive file loading, the asset load queue used by `MadnightEngine::HardLoadingScreen`, and world-space unit literals.

## ArchiveHelper

`src/helpers/archive.hh`

The engine's default file loader — reads files out of a `psyqo::paths::ArchiveManager`-backed archive (the format produced by pcsx-redux's authoring tool, see [Loading Assets](../getting-started/loading-assets)). Every asset manager (`TextureManager`, `MeshManager`, `ColbinManager`, `AnimationManager`, `SoundManager`, `ModSoundManager`) loads its raw file data through this.

```cpp
constexpr uint8_t MAX_ARCHIVE_FILE_NAME_LEN = 255;

class ArchiveHelper final {
public:
  static void init(eastl::function<void()> cb);
  static psyqo::Coroutine<psyqo::Buffer<uint8_t>> LoadFile(const char* fileName);
};
```

`MAX_ARCHIVE_FILE_NAME_LEN` is the shared name-length constant used throughout the engine for fixed-capacity asset name strings (mesh names, texture names, VAG names, and so on).

### Internals

- `init()` is asynchronous (callback-based, sets up the archive's decompressor and initializes against the CD-ROM device) — calling `LoadFile` before that callback has fired just logs a warning and returns an empty buffer, it doesn't queue or wait.

## CDRomHelper

`src/helpers/cdrom.hh`

An alternative, lower-level loader that reads directly off an ISO9660 filesystem instead of a packed archive — used when `PCDRV` isn't defined (i.e. not running under a host-connected debug build).

```cpp
class CDRomHelper {
public:
  static void init(eastl::function<void()> cb);
  static psyqo::Coroutine<psyqo::Buffer<uint8_t>> LoadFile(const char *fileName);
#ifndef PCDRV
  static psyqo::CDRomDevice& CDRomDevice();
#endif
};
```

## LoadQueue

`src/helpers/load_queue.hh`

The runtime entry type produced by parsing a [SCENEBIN manifest](../guides/scenebin) — a list of these is what you hand to `MadnightEngine::HardLoadingScreen` to bulk-load everything a scene needs before switching to it.

```cpp
enum LoadFileType { OBJECT, TEXTURE, MOD_FILE, ANIMATION, COLBIN, VAG, SCENE = 255 };

struct LoadQueue {
  eastl::fixed_string<char, MAX_ARCHIVE_FILE_NAME_LEN> name;
  LoadFileType type;
  union {
    struct { uint16_t x, y, clutX, clutY; }; // used when type == TEXTURE
  };
};
```

See [SCENEBIN → Types](../guides/scenebin#types) for the full type table and the on-disk manifest format this is parsed from.

### Usage

```cpp
eastl::vector<LoadQueue> files;
co_await SceneLoader::LoadScene("level01.scenebin", files); // parses the manifest into a queue

// hands the queue to the engine's built-in loading scene, which loads everything
// then switches to postLoadScene once done
co_await g_madnightEngine.HardLoadingScreen(eastl::move(files), &gameplayScene);
```

## World-space literals

`src/helpers/world_space.hh`

User-defined literals for converting Blender-exported units directly into engine world-space fixed-point values, matching the 128-units-per-metre scale described in [Core → World units](../api/core#world-units).

```cpp
using namespace psyqo::fixed_point_literals;

// 1m, 0.5 Blender units = 0.5_ws
// 2m, 1 Blender unit = 1.0_ws
// if Blender says X is 50m, you want 25.0_ws; if 1m, you want 0.5_ws
consteval psyqo::FixedPoint<> operator"" _ws(long double blenderUnits);
consteval psyqo::FixedPoint<10> operator"" _ws10(long double blenderUnits);
```

`_ws10` is the same conversion targeting a `FixedPoint<10>` (10 fractional bits) rather than the engine's default fixed-point precision — used wherever a call site specifically needs that precision (e.g. angle-adjacent lerps).

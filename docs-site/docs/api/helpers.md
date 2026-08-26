---
title: Helpers
sidebar_position: 11
---

# Helpers

`src/helpers/` — CD-ROM/archive file loading, the asset load queue and loader that consumes it, and world-space unit literals.

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
- `LoadFile` loads a file from the archive by name.

## CDRomHelper

`src/helpers/cdrom.hh`

A lower-level loader that reads directly off an ISO9660 filesystem instead of a packed archive.

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

`CDRomHelper::LoadFile` has no callers; `ArchiveHelper::LoadFile` runs all asset loads instead, in both `PCDRV` and non-`PCDRV` builds, borrowing `CDRomHelper::CDRomDevice()` as its underlying device in the latter case.

## LoadQueue

`src/helpers/load_queue.hh`

The runtime entry type produced by parsing a [SCENEBIN manifest](../guides/scenebin) — a list of these is what you hand to [`FileLoader::LoadFiles`](#fileloader) (directly, or via `MadnightEngine::HardLoadingScreen`/`SoftLoadingScreen`) to bulk-load everything a scene needs.

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

## FileLoader

`src/file_loader.hh`

Consumes a `LoadQueue` list and loads every entry through the matching manager (`MeshManager`, `TextureManager`, `ModSoundManager`, `AnimationManager`, `ColbinManager`, `SoundManager`) in order, one file at a time. This is the actual loading engine — `MadnightEngine::HardLoadingScreen`/`SoftLoadingScreen` (below) are the higher-level entry points most game code should call instead of using `FileLoader` directly, but it's useful to understand since it owns the queue, progress counters, and load order.

```cpp
enum LOAD_STATE : uint8_t { UNKNOWN, LOADING, COMPLETE };

class FileLoader final {
public:
  // clearPools dumps all existing game objects, textures, meshes, colbins, and sfx first —
  // it does not check whether anything is still in use before doing so.
  static psyqo::Coroutine<> LoadFiles(eastl::vector<LoadQueue> &&files, bool clearPools = true);
  static uint16_t TotalFiles(void);  // increases as SCENE entries are encountered
  static uint16_t LoadedFiles(void);
  static LOAD_STATE LoadState(void);
};
```

### Usage

Most game code goes through `MadnightEngine` rather than calling `FileLoader` directly — it wraps `FileLoader::LoadFiles` with scene push/pop around the built-in (or your own) loading screen:

```cpp
eastl::vector<LoadQueue> files;
co_await SceneLoader::LoadScene("level01.scenebin", files); // parses the manifest into a queue

// pops the current scene, shows the loading screen, loads everything, then switches to postLoadScene
co_await g_madnightEngine.HardLoadingScreen(eastl::move(files), &gameplayScene);
```

Two lighter variants exist alongside `HardLoadingScreen`:

```cpp
// keeps the current scene on the stack instead of unloading it, and doesn't dump existing
// pools -- useful for streaming in extra assets without a full scene transition. Uses the
// engine's own built-in loading scene.
co_await g_madnightEngine.SoftLoadingScreen(eastl::move(files));

// either HardLoadingScreen or SoftLoadingScreen can take your own loading scene instead
// of the engine's default one
co_await g_madnightEngine.HardLoadingScreen(eastl::move(files), myLoadingScene, &gameplayScene);
```

The one-argument `SoftLoadingScreen(files)` forwards to the two-argument overload with the engine's built-in default loading scene; pass your own `psyqo::Scene` as a second argument if you want a custom one instead.

Calling `FileLoader::LoadFiles` directly only makes sense if you're building your own loading-screen flow rather than using `MadnightEngine`'s.

### Internals

- `LoadFiles` empties and releases the queue's capacity (`m_queue.set_capacity(0)`) once loading finishes — it doesn't just sit there holding memory between loads.
- A `SCENE`-type entry can append more entries onto the queue mid-load (via `SceneLoader::LoadScene`), which is why `TotalFiles()` can grow while loading is in progress — don't treat it as a fixed count up front.
- `clearPools` (default `true`) dumps game objects, meshes, textures, colbins, and sound all at once before loading starts — pass `false` (as `SoftLoadingScreen` does) if you want to add to what's already loaded instead of replacing it.

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

---
title: Helpers
sidebar_position: 11
---

# Helpers

`src/helpers/` has the world-space unit literals, the archive reader every asset manager loads through, and the load queue that tells the engine what to load. `src/file_loader.hh` is the loader that consumes the queue.

## World-space literals

`src/helpers/world_space.hh`

`_ws` turns a number from Blender into an engine world-space value. Use it for any position, size, speed or distance you write in code.

```cpp
auto *door = g_madnightEngine.m_gameObjectManager.CreateGameObject(
    "door_trigger", {10.0_ws, 0, 3.0_ws}, {0, 0, 0}, GameObjectTag::INTERACTABLE);
door->SetAsTrigger({1.0_ws, 2.0_ws, 1.0_ws});
```

### The world scale: 1 Blender unit = 2 m

The engine's convention is that one Blender unit is 2 metres.

| In code | Blender units | Metres |
| --- | --- | --- |
| `1.0_ws` | 1 | 2 m |
| `0.5_ws` | 0.5 | 1 m (`ONE_METRE`) |
| `5.0_ws` | 5 | 10 m (`maxRayDistance`) |

If Blender's grid says a wall is 25 units long, write `25.0_ws`. Blender itself will call that 25 m, though the engine treats it as 50 m. To make Blender's displayed metres match the engine, set the scene's Unit Scale to 2.0, or model everything at half size.

`ONE_METRE` (`src/core/world_defs.hh`) is defined as `0.5_ws`. Multiply it when you want real metres, for example `3 * ONE_METRE` for a 3 m ray.

### _ws10

`_ws10` is the same conversion, but gives you a `psyqo::FixedPoint<10>` instead of the engine's default `FixedPoint<>`. You will only need it where a call asks for that type.

Both literals are `consteval`, so they only accept numbers written in the source. For a value that changes at runtime, multiply `ONE_METRE` by an integer instead.

## LoadQueue

`src/helpers/load_queue.hh`

A `LoadQueue` entry is one file for the loader to read: a name, a type, and for textures a VRAM position. You build a list of them and hand it to a loading screen.

```cpp
eastl::vector<LoadQueue> queue = {
    {.name = "TEXTURES/STREET.TIM", .type = LoadFileType::TEXTURE, .x = 320, .y = 0, .clutX = 0, .clutY = 240},
    {.name = "MODELS/STREET.MB", .type = LoadFileType::OBJECT},
};
```

This is how `src/game.cpp` does it. The type is one of `OBJECT`, `TEXTURE`, `MOD_FILE`, `ANIMATION`, `COLBIN`, `VAG`, `TEXTURE_EXTRA` or `SCENE`. `x`, `y`, `clutX` and `clutY` only matter for textures, and `flags` only for `TEXTURE_EXTRA` (it is passed on as the texture's user flags).

An entry with type `SCENE` names a [SCENEBIN manifest](../guides/scenebin), which expands into more entries when it is reached. That is the usual way to load a whole level with one line:

```cpp
eastl::vector<LoadQueue> files = {{.name = "level01.scenebin", .type = LoadFileType::SCENE}};
```

The [SCENEBIN guide](../guides/scenebin#types) has the type table and the on-disk format.

## Loading screens

`src/madnight.hh`

You rarely call the file loader yourself. `MadnightEngine` wraps it with a loading scene, and these are the calls to use.

`HardLoadingScreen` is for changing level. It pops the current scene, shows the loading scene, clears the game object, mesh, texture, colbin and sound pools, loads the queue, then switches to the scene you pass.

```cpp
co_await g_madnightEngine.HardLoadingScreen(eastl::move(files), &gameplayScene);
```

Pass your own loading scene as the second argument to replace the built-in one:

```cpp
co_await g_madnightEngine.HardLoadingScreen(eastl::move(files), &myLoadingScene, &gameplayScene);
```

`SoftLoadingScreen` is for streaming extra assets in. It pushes the loading scene on top of the current one, loads without clearing anything, then pops back to where you were. It also has a version that takes your own loading scene.

```cpp
co_await g_madnightEngine.SoftLoadingScreen(eastl::move(files));
```

:::caution[A hard load clears everything]
`HardLoadingScreen` dumps every game object, mesh, texture, colbin and sound before it loads. It does not check whether anything is still in use, so any pointer you hold from before the call is gone afterwards. Because the current scene is replaced, it also restarts the new scene fresh (`Start`, not `Resume`), so do not repeat set-up work that already ran.
:::

Both are coroutines. Call them with `co_await` from a coroutine such as `InitialLoad`.

## FileLoader

`src/file_loader.hh`, reached as `g_madnightEngine.m_fileLoader`.

This is what the loading screens call. It loads each entry in order through the matching manager, one file at a time. Call `LoadFiles` yourself only if you are writing your own loading flow.

```cpp
co_await g_madnightEngine.m_fileLoader.LoadFiles(eastl::move(files), /*clearPools*/ false);
```

While it runs, a loading scene can show progress:

```cpp
uint32_t percent = loader.LoadedFiles() * 100 / loader.TotalFiles();
```

`LoadState()` returns `UNKNOWN` before anything was queued, `LOADING` while files remain, and `COMPLETE` when they are all loaded.

`TotalFiles()` is not fixed up front. Each `SCENE` entry adds its contents to the queue as it is reached, so the total grows during loading and a progress bar can step backwards. An empty queue returns straight away without loading anything (after clearing pools, if you asked it to).

## ArchiveHelper

`src/helpers/archive.hh`, reached as `g_madnightEngine.m_archiveHelper`.

Reads one file out of the archive by name. The asset managers all load through it, and the engine initialises it at start-up, so you will normally not touch it.

```cpp
auto buffer = co_await g_madnightEngine.m_archiveHelper.LoadFile("DATA/LEVEL.BIN");
if (buffer.empty()) {
  // not found, or the archive was not ready
}
```

Calling it before the archive has finished initialising does not wait. It prints a warning and returns an empty buffer. A missing file also gives an empty buffer.

### HashName

`HashName` turns a name into the `uint64_t` the managers store and compare instead of a full string. Pass a string literal and the hash is worked out at compile time. If you look the same name up often, hash it once and keep the number.

```cpp
constexpr uint64_t streetHash = HashName("MODELS/STREET.MB");
```

`MAX_ARCHIVE_FILE_NAME_LEN` (255) is the longest name a `LoadQueue` entry can hold.

## CDRomHelper

`src/helpers/cdrom.hh`

:::caution[Deprecated]
`CDRomHelper` is marked `[[deprecated]]` in favour of `ArchiveHelper`, and its instance is a private member of `MadnightEngine`. Game code cannot reach it and should not need to. It only exists because the archive reader borrows its CD-ROM device on real hardware builds.
:::

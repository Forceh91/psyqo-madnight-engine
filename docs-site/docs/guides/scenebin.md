---
title: SCENEBIN Format
sidebar_position: 5
---

# SCENEBIN

A manifest of files to load. Produced by `tools/scenebin_converter.py` from a plain text
source file, consumed by `SceneLoader::LoadScene`.

A SCENEBIN does not contain assets. It expands into entries on `FileLoader`'s work queue,
each of which is then loaded by the manager for its type. Loading one is how you avoid
hand-writing a long `eastl::vector<LoadQueue>` in game code.

Every multi-byte field is little-endian. There is no version byte.

## Binary layout

| Field | Type | Bytes | Notes |
|---|---|---|---|
| magic | char[8] | 8 | `SCENEBIN`, no terminator. |
| fileCount | uint8_t | 1 | 0-255. The converter refuses more than 255 entries. |

Then `fileCount` entries, each:

| Field | Type | Bytes | Notes |
|---|---|---|---|
| type | uint8_t | 1 | A `LoadFileType` value, see below. |
| nameLen | uint8_t | 1 | Length of `name` in bytes. |
| name | char[nameLen] | nameLen | Archive-relative path. **Not** null-terminated on the wire. |
| vramX | uint16_t | 2 | TEXTURE entries only. `0xFFFF` means take it from the TIM. |
| vramY | uint16_t | 2 | TEXTURE entries only. `0xFFFF` means take it from the TIM. |
| clutX | uint16_t | 2 | TEXTURE entries only. `0xFFFF` means take it from the TIM. |
| clutY | uint16_t | 2 | TEXTURE entries only. `0xFFFF` means take it from the TIM. |

### Texture placement

The four placement values are present only when `type` is TEXTURE. Every other type ends its
entry after the name.

`vramX`/`vramY` are where the image goes in VRAM, `clutX`/`clutY` where its palette goes.
They are passed straight through to `TextureManager::LoadTIM`, which does no automatic
packing, so placement has to be planned per texture. Putting it in the manifest is what
keeps a scene's VRAM layout in one place instead of scattered through game code.

All four are used exactly as given, and 0 is a real coordinate. Write `auto` in place of a
number to mean "use the position stored in the TIM file itself" for that coordinate; the
converter encodes it as `0xFFFF`, which VRAM can never hold. See the
[textures page](../api/textures).

## Types

`LoadFileType` is defined in `src/helpers/load_queue.hh`.

| Value | Name | Manifest keyword | Extension | Loaded by |
|---|---|---|---|---|
| 0 | OBJECT | `object` | `.meshbin` | `MeshManager::LoadMesh` |
| 1 | TEXTURE | `texture` | `.TIM` | `TextureManager::LoadTIM` |
| 2 | MOD_FILE | `mod_file` | `.MOD` | `ModSoundManager::LoadMODSound` |
| 3 | ANIMATION | `animation` | `.animbin` | `AnimationManager::LoadAnimation` |
| 4 | COLBIN | `colbin` | `.colbin` | `ColbinManager::LoadColbin` |
| 5 | VAG | `vag` | `.VAG` | `SoundManager::LoadVAGFile` |
| 255 | SCENE | not emitted, see below | `.scenebin` | `SceneLoader::LoadScene`, recursively |

## Source manifest

Plain text, one entry per line, whitespace separated. Blank lines are ignored and lines
beginning with `#` are comments. Texture lines take four extra placements: `vramX vramY clutX
clutY`. Each is an integer, or `auto` to take that coordinate from the TIM file.

```
# scene manifest, SBSKT court scene
vag SFX/FCNTNA.VAG
texture TEXTURES/LOGO.TIM 320 0 0 240
texture UI/CS_UI.TIM 320 129 0 241
object MODELS/SBSKT.MB
object MODELS/SCART.MB
```

```
python3 tools/scenebin_converter.py scene.txt scene.scenebin --asset-root assets/
```

That example converts to 5 entries and 108 bytes.

## Nested scenes

`SCENE` is 255, which fits a `uint8_t` fine, and both `SceneLoader::LoadScene` and
`FileLoader::LoadFiles` handle it: a SCENE entry parses with the same layout as any
non-texture entry, and `FileLoader` recursively expands it into the same queue while
iterating, growing the total file count in place. Nested scene loading works today.

The converter does not currently emit SCENE entries. That is a limitation of the tool, not
of the format or the engine.

## Loading a scene

```cpp
eastl::vector<LoadQueue> files;
co_await SceneLoader::LoadScene("LEVEL01.SCENEBIN", files);
co_await FileLoader::LoadFiles(eastl::move(files));
```

`LoadFiles` clears every asset pool first unless you pass `false` as its second argument.
`MadnightEngine::HardLoadingScreen` and `SoftLoadingScreen` wrap this pair with a loading
scene; hard clears the pools, soft does not.

## Caveats

- **Names are not null-terminated on the wire.** The reader constructs a string from exactly
  `nameLen` bytes. A tool writing a terminator will produce paths with a trailing NUL in
  them.
- **`nameLen` is bounds-checked** against `MAX_ARCHIVE_FILE_NAME_LEN` (255) by the reader.
- **There is no version byte.** Any future layout change is silently incompatible with
  existing files. COLBIN and MESHBIN both carry one.

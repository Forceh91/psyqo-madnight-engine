---
title: Textures
sidebar_position: 9
---

# Textures

`src/textures/texture_manager.hh` — loads and manages `.TIM` textures in VRAM. See the [Asset Pipeline Guides](../guides/overview#textures) for how to author a `.TIM` from a source PNG.

```cpp
static constexpr uint16_t texturePageWidth = 64;
static constexpr uint16_t texturePageHeight = 256;
static constexpr uint8_t texturePageColumns = 16;
static constexpr uint8_t MAX_TEXTURES = 32;

struct TimFile {
  eastl::fixed_string<char, MAX_ARCHIVE_FILE_NAME_LEN> name;
  uint16_t x, y, width, height;
  psyqo::Prim::TPageAttr::ColorMode colourMode; // bits per pixel: 4, 8, or 16

  bool hasClut;
  uint16_t clutX, clutY;
  uint16_t clutWidth, clutHeight; // clutHeight is always 1
};

class TextureManager final {
public:
  static psyqo::Coroutine<> LoadTIM(const char *textureName, uint16_t x, uint16_t y, uint16_t clutX, uint16_t clutY, TimFile **timOut);
  static psyqo::PrimPieces::TPageAttr GetTPageAttr(const TimFile *tim);
  static psyqo::PrimPieces::TPageAttr GetTPageAttr(const TimFile &tim);
  static psyqo::Rect GetTPageUVForTim(const TimFile &tim);
  static psyqo::Rect GetTPageUVForTim(const TimFile *tim);

  static void GetTextureFromName(const char *textureName, TimFile **timOut);

  // dump all textures in memory and start fresh. Used when switching to a
  // loading screen. Dangerous — doesn't check what's in use, and doesn't
  // clear VRAM itself.
  static void Dump(void);
};
```

- `LoadTIM` takes the target VRAM placement (`x`, `y`) and CLUT placement (`clutX`, `clutY`) explicitly, and the engine doesn't do automatic VRAM packing, so placement has to be planned per-texture (this is exactly what the `TEXTURE` entry fields in a [SCENEBIN manifest](../guides/scenebin#texture-placement) carry).
- `GetTPageAttr`/`GetTPageUVForTim` convert a loaded `TimFile`'s VRAM placement into the texture-page attribute and UV rect a draw primitive needs — used internally by `GameObject`/`Billboard` rendering and `Renderer::RenderSprite`.
- Up to `MAX_TEXTURES` (32) textures can be resident at once. Past that, `LoadTIM` yields a null `TimFile*` in `*timOut`, so check it.

### Usage

```cpp
TimFile *crateTex;
co_await TextureManager::LoadTIM("crate.tim", /*x*/ 320, /*y*/ 0, /*clutX*/ 0, /*clutY*/ 240, &crateTex);
crateGameObject->SetTexture("crate.tim"); // looks it up by the same name afterwards
```

### Internals

VRAM placement is entirely manual, and getting `x`/`y`/`clutX`/`clutY` right requires understanding the layout (spelled out in a long comment at the top of `texture_manager.cpp`):

- The frame buffers occupy VRAM `0-319, 0-479`, so the first free texture page starts at `x=320`.
- Pages are `64×256` px each, so a texture must fit within 1, 2, or 4 *contiguous* pages depending on colour depth — 4-bit textures are squeezed to 1/4 width in VRAM, 8-bit to 1/2 width, 16-bit at full width.
- You can't mix colour depths within one page.
- CLUTs have known-safe rows at `y=240-255` and `y=496-511` (X must be a multiple of 16), which is why the usage example above places the CLUT at `y=240`.
- All four placement coordinates are used exactly as given, including 0, which is a real VRAM position. Pass `TIM_POSITION_FROM_FILE` (`0xFFFF`, a value VRAM cannot hold) for any of them to take that coordinate from the TIM file instead. In a scene manifest the same thing is spelled `auto`.

Getting this wrong doesn't crash: textures can silently overlap or clip in VRAM. The [SCENEBIN format](../guides/scenebin#texture-placement) is the intended way to keep placement authoritative and consistent per-scene rather than hardcoding coordinates per texture.

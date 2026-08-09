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

- `LoadTIM` takes the target VRAM placement (`x`, `y`) and CLUT placement (`clutX`, `clutY`) explicitly — the engine doesn't do automatic VRAM packing, so placement has to be planned per-texture (this is exactly what the `TEXTURE` entry fields in a [SCENEBIN manifest](../guides/scenebin#texture-placement-texture-entries-only) carry).
- `GetTPageAttr`/`GetTPageUVForTim` convert a loaded `TimFile`'s VRAM placement into the texture-page attribute and UV rect a draw primitive needs — used internally by `GameObject`/`Billboard` rendering and `Renderer::RenderSprite`.
- Up to `MAX_TEXTURES` (32) textures can be resident at once.

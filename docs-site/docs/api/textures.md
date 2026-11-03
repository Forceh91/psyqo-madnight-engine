---
title: Textures
sidebar_position: 9
---

# Textures

`src/textures/texture_manager.hh` loads `.TIM` textures into VRAM. See the [Asset Pipeline Guides](../guides/overview#textures) for how to make a `.TIM` from a PNG. The manager is `g_madnightEngine.m_textureManager`.

## Loading a texture

Most of the time you do not call `LoadTIM` yourself. A `TEXTURE` entry in a [load queue](./helpers#loadqueue) or a [SCENEBIN manifest](../guides/scenebin#texture-placement) loads it for you with the placement you give. To do it by hand:

```cpp
TimFile *crateTex;
co_await g_madnightEngine.m_textureManager.LoadTIM(
    "TEXTURES/CRATE.TIM", /*x*/ 320, /*y*/ 0, /*clutX*/ 0, /*clutY*/ 240, &crateTex);
crate->SetTexture("TEXTURES/CRATE.TIM"); // looked up by the same name
```

Objects find a texture by name through `SetTexture`, so the name you load it under is the name you use afterwards.

Limits to know about:

- Loading a name that is already in VRAM returns the existing texture and ignores the placement you passed.
- Up to `MAX_TEXTURES` (32) can be loaded at once. Past that, or if the file is missing, `*timOut` is `nullptr`, so check it.
- `TimFile` stores a hash of the name (`nameHash`), not the string. You can look a loaded texture up with `GetTextureFromName("TEXTURES/CRATE.TIM", &tim)`, which gives `nullptr` if it is not loaded.

## Placing textures in VRAM

The engine does not pack VRAM for you. You choose `x`, `y`, `clutX` and `clutY` for every texture, and getting them wrong does not crash, the textures just overlap or clip. The rules:

- The frame buffers take VRAM `0-319, 0-479`, so the first free texture page starts at `x=320`.
- A page is `64x256` px, and a texture needs 1, 2 or 4 contiguous pages depending on colour depth: 4-bit textures take a quarter of the width in VRAM, 8-bit half, 16-bit the full width.
- Do not mix colour depths in one page.
- Safe places for CLUTs are rows `y=240-255` and `y=496-511`, with `x` a multiple of 16. That is why the example above uses `clutY` 240.

All four values are used exactly as given, and `0` is a real position. To take a coordinate from the TIM file instead, pass `TIM_POSITION_FROM_FILE` (`0xFFFF`). In a scene manifest that is written `auto`.

```cpp
co_await g_madnightEngine.m_textureManager.LoadTIM(
    "TEXTURES/CRATE.TIM", TIM_POSITION_FROM_FILE, TIM_POSITION_FROM_FILE,
    TIM_POSITION_FROM_FILE, TIM_POSITION_FROM_FILE, &crateTex);
```

The [SCENEBIN format](../guides/scenebin#texture-placement) is the way to keep placement in one place per scene rather than scattering coordinates through code.

## Transparency and the STP bit

By default `LoadTIM` sets the STP bit (bit 15) on every non-zero CLUT entry, or every non-zero pixel of a 16-bit TIM, before uploading. [Advanced fog](./render#advanced-fog) needs this to blend textured faces.

That happens whether or not fog is ever turned on, so if you draw your own semi-transparent primitives from an engine-loaded texture, they will blend where you expected them to be opaque. Pass `1` as `userFlags` to skip it. Do the same for a texture with holes (a fence, say), or one used by an `RF_QUICK_ADVANCED_FOG` object.

```cpp
TimFile *fenceTex;
co_await g_madnightEngine.m_textureManager.LoadTIM(
    "TEXTURES/FENCE.TIM", 384, 0, 0, 241, &fenceTex, /*userFlags*/ 1);
```

From a manifest, use a `texture_extra` line with `flags` set to 1, as in `texture_extra TEXTURES/SMS.TIM 320 0 0 240 1`. A plain `texture` line always loads with flags 0. See [texture_extra](../guides/scenebin#texture-extra-placement).

## Using a texture in your own drawing

`GetTPageAttr` and `GetTPageUVForTim` turn a loaded `TimFile` into the texture-page attribute and the UV offset a primitive needs. Both take a reference or a pointer. The engine uses them for objects, billboards and `Renderer::RenderSprite`, so you only need them when you build primitives yourself.

```cpp
auto tpage = g_madnightEngine.m_textureManager.GetTPageAttr(crateTex);
auto uv = g_madnightEngine.m_textureManager.GetTPageUVForTim(crateTex);
```

## Dumping textures

`Dump()` forgets every loaded texture. The file loader calls it when a [hard loading screen](./helpers#loading-screens) clears pools. It does not check whether anything still uses them, and it does not clear VRAM, so objects pointing at a dumped texture are left with nothing valid.

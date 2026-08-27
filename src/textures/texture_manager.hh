#pragma once
#include "../pool/pool.hh"

#include <EASTL/functional.h>
#include <psyqo/coroutine.hh>
#include <psyqo/primitives.hh>

static constexpr uint16_t texturePageWidth = 64;
static constexpr uint16_t texturePageHeight = 256;
static constexpr uint8_t texturePageColumns = 16;
static constexpr uint8_t MAX_TEXTURES = 32; // this will need tweaking later
// 0 is a valid VRAM coordinate, so the 'use whatever the file says' sentinel has to be
// a value VRAM can never hold. VRAM is 1024x512.
static constexpr uint16_t TIM_POSITION_FROM_FILE = 0xFFFF;

struct TimFile
{
    uint64_t nameHash;
    int16_t id = INVALID_POOL_ID;
    bool isLoaded;                                // is this slot actually holding a texture?
    uint16_t x, y, width, height;                 // pos in vram + width/height
    psyqo::Prim::TPageAttr::ColorMode colourMode; // bits per pixel (4, 8, 16)

    bool hasClut;                   // does it need/have a clut?
    uint16_t clutX, clutY;          // clut pos in vram
    uint16_t clutWidth, clutHeight; // clut width and height (always 1)
};

class TextureManager final
{
    static Pool<TimFile, MAX_TEXTURES> m_pool;
    static psyqo::Vertex GetTPageIndex(uint16_t x, uint16_t y);

    static TimFile *IsTextureLoaded(const char *name);
    static TimFile *IsTextureLoaded(uint64_t nameHash);

public:
    static psyqo::Coroutine<> LoadTIM(const char *textureName, uint16_t x, uint16_t y, uint16_t clutX, uint16_t clutY, TimFile **timOut);
    static psyqo::PrimPieces::TPageAttr GetTPageAttr(const TimFile *tim);
    static psyqo::PrimPieces::TPageAttr GetTPageAttr(const TimFile &tim);
    static psyqo::Rect GetTPageUVForTim(const TimFile &tim);
    static psyqo::Rect GetTPageUVForTim(const TimFile *tim);

    static void GetTextureFromName(const char *textureName, TimFile **timOut);

    // dump all textures in memory and start fresh
    // this is used when switching to a loading screen for instance.
    // this is a dangerous function as it wont check if anything is used
    // this wont remove anything from vram
    static void Dump(void);
};


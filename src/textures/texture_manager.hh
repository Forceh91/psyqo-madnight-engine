#ifndef _TEXTURE_MANAGER_H
#define _TEXTURE_MANAGER_H

#include <stdint.h>
#include <EASTL/functional.h>
#include <EASTL/fixed_string.h>
#include "psyqo/coroutine.hh"
#include "psyqo/primitives.hh"
#include "../helpers/file_defs.hh"

static constexpr uint16_t texturePageWidth = 64;
static constexpr uint16_t texturePageHeight = 256;
static constexpr uint8_t texturePageColumns = 16;
static constexpr uint8_t MAX_TEXTURES = 32; // this will need tweaking later

typedef struct _TIM_FILE
{
    eastl::fixed_string<char, MAX_CDROM_FILE_NAME_LEN> name;
    uint16_t x, y, width, height;                 // pos in vram + width/height
    psyqo::Prim::TPageAttr::ColorMode colourMode; // bits per pixel (4, 8, 16)

    bool hasClut;                   // does it need/have a clut?
    uint16_t clutX, clutY;          // clut pos in vram
    uint16_t clutWidth, clutHeight; // clut width and height (always 1)
} TimFile;

class TextureManager final
{
    static psyqo::Vertex GetTPageIndex(uint16_t x, uint16_t y);
    static eastl::array<TimFile, MAX_TEXTURES> m_textures;

    static int8_t GetFreeIndex(void);
    static TimFile *IsTextureLoaded(const char *name);

public:
    static psyqo::Coroutine<> LoadTIMFromCDRom(const char *textureName, uint16_t x, uint16_t y, uint16_t clutX, uint16_t clutY, TimFile **timOut);
    static psyqo::PrimPieces::TPageAttr GetTPageAttr(const TimFile &tim);
    static psyqo::Rect GetTPageUVForTim(const TimFile &tim);

    // dump all textures in memory and start fresh
    // this is used when switching to a loading screen for instance.
    // this is a dangerous function as it wont check if anything is used
    // this wont remove anything from vram
    static void Dump(void);
};

#endif

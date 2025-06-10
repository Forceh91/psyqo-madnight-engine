#ifndef _TEXTURE_MANAGER_H
#define _TEXTURE_MANAGER_H

#include <stdint.h>
#include <EASTL/functional.h>
#include "psyqo/primitives.hh"

static constexpr uint16_t texturePageWidth = 64;
static constexpr uint16_t texturePageHeight = 256;
static constexpr uint8_t texturePageColumns = 16;

typedef struct _TIM_FILE
{
    uint16_t x, y, width, height;                 // pos in vram + width/height
    psyqo::Prim::TPageAttr::ColorMode colourMode; // bits per pixel (4, 8, 16)

    bool hasClut;                   // does it need/have a clut?
    uint16_t clutX, clutY;          // clut pos in vram
    uint16_t clutWidth, clutHeight; // clut width and height (always 1)
} TimFile;

class TextureManager final
{
    static psyqo::Vertex GetTPageIndex(uint16_t x, uint16_t y);

public:
    static void LoadTIMFromCDRom(const char *textureName, uint16_t x, uint16_t y, uint16_t clutX, uint16_t clutY, eastl::function<void(TimFile timFile)> onComplete);
    static psyqo::PrimPieces::TPageAttr GetTPageAttr(const TimFile &tim);
    static psyqo::Rect GetTPageUVForTim(const TimFile &tim);
};

#endif

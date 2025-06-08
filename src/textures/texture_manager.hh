#ifndef _TEXTURE_MANAGER_H
#define _TEXTURE_MANAGER_H

#include <stdint.h>
#include <EASTL/functional.h>

typedef struct _TIM_FILE
{
    uint16_t x, y, width, height; // pos in vram + width/height
    uint8_t bpp;                  // bits per pixel (4, 8, 16)

    uint16_t clutX, clutY;          // clut pos in vram
    uint16_t clutWidth, clutHeight; // clut width and height (always 1)
    uint16_t *clutData;             // clut data (if present)

    uint16_t *imageData; // the image data
} TimFile;

class TextureManager final
{
public:
    static void LoadTIMFromCDRom(const char *textureName, uint16_t x, uint16_t y, eastl::function<void(size_t textureSize)> onComplete);
};

#endif

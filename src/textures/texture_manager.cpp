#include "texture_manager.hh"
#include "../helpers/cdrom.hh"
#include "../render/renderer.hh"

/*
 * ok so this is confusing as hell and we have to manage VRAM ourself which is wild.
 * textures are uploaded to VRAM. then theres tpages within that vram which are split
 * into a 16x2 grid (64x256 per page).
 * when you want to use a page you have to make sure that a texture is fully contained within
 * 1, 2, or 4 (depends on colour depth) continous pages.
 * when you want to render a texture etc. you need to get the tpage its in based off its coordinates
 * so a texture at 640,0 is in the tpage at 640,0
 *
 * right now this is set up so that the front and back buffer of the gpu occupy 0-319,0-479
 * so that means the first bit of free space in the vram we've got is at 320,0
 * for example available tpage1 is 320,0 - and then tpage2 would be at 640,0
 *
 * oh right and textures are squished horizontally in the vram depending on the colour depth..
 * 4-bit (16 colours) has 1/4 width, 8-bit (256 colours) has 1/2 width, and 16-bit = 1/1 width
 *
 * we also need to make sure we're not crossing tpages vertically etc.
 * we also can't mix texture colour depths in a tpage. they have to be separate
 *
 * lets say we've got 22 tpages left after the frame buffers. then probably 2 more are taken
 * by our clut data so thaht leaves us with 20 tpages with some space to spare for overflow
 *
 * 16 bit colour textures are the biggest but least likely since they're really only used for the player
 * and the HUD or menus or whatever
 * 16-bit colour texture = 2 tpages
 * 8-bit + 4-bit colour texture = remaining 18 tpages
 */

void TextureManager::LoadTIMFromCDRom(const char *textureName, uint16_t x, uint16_t y, uint16_t clutX, uint16_t clutY, eastl::function<void(TimFile timFile)> onComplete)
{
    CDRomHelper::load_file(textureName, [x, y, clutX, clutY, onComplete](psyqo::Buffer<uint8_t> &&buffer)
                           {
                            void * data = buffer.data();
                            size_t size = buffer.size();
        if (data == nullptr || size == 0) {
            printf("TEXTURE: Failed to load texture or it has no file size.\n");
            buffer.clear();
            return;
        }

        TimFile timFile = {0};
        uint32_t * ptr = (uint32_t*)data;

        uint32_t header = *ptr;

        // check the header of the tim file
        if ((*(ptr++) & 0xFF) != 0x10) {
            printf("TEXTURE: Invalid TIM file, aborting.\n");
            buffer.clear();
            return;
        }

        // read the bpp. flags is bits 0-2 bpp, 3 = has a clut
        uint32_t flags = *(ptr++);

        // set what colour mode to use on the texture
        switch (flags & 0x3) {
            default:
            case 0:
                timFile.colourMode = psyqo::Prim::TPageAttr::ColorMode::Tex4Bits;
            break;
            case 1:
                timFile.colourMode = psyqo::Prim::TPageAttr::ColorMode::Tex8Bits;
            break;
            case 2:
                timFile.colourMode = psyqo::Prim::TPageAttr::ColorMode::Tex16Bits;
            break;
        }

        // and then read the clut data, this is only present if the flags say so
        if (flags & 0x8) {
            // mark it as having a clut
            timFile.hasClut = true;

            // read the clut data
            uint32_t *clut_end = ptr;
            clut_end += *(ptr++) / 4;

            // clut x/y/w/h data
            uint16_t* rect = (uint16_t*)ptr;
            timFile.clutX = clutX > 0 ? clutX : rect[0];
            timFile.clutY = clutY >= 0 ? clutY : rect[1];
            timFile.clutWidth = rect[2];
            timFile.clutHeight = rect[3];

            // past the rect we go (2 lots of uint32_t)
            ptr += 2;

            // data of the clut. number of colours (width * height entries, which are 2 bytes each)
            uint16_t numColours = timFile.clutWidth * timFile.clutHeight;
            uint16_t clutDataSize = numColours * sizeof(uint16_t);

            // assign the clut data from the ptr
            uint16_t * clutData = (uint16_t*) psyqo_malloc(clutDataSize);
            __builtin_memcpy(clutData, ptr, clutDataSize);

            // move pointer to the end of the clut block
            ptr = clut_end;

            // upload this to the vram
            Renderer::Instance().VRamUpload(clutData, timFile.clutX, timFile.clutY, timFile.clutWidth, timFile.clutHeight);
        }

        uint32_t imageLength = *(ptr++);
        // bnum (4 bytes) + pos(4 bytes) + size(4 bytes)
        // should be more than 12 bytes as image data follows
        if (imageLength <= 12) {
            printf("TEXTURE: Image data seems to be missing from TIM, aborting.\n");
            buffer.clear();
            return;
        }

        // first up is the rect (x, y, width, height)
        // dont forget to override x/y if provided
        uint16_t* rect = (uint16_t*)ptr;
        timFile.x = x > 0 ? x : rect[0];
        timFile.y = y >= 0 ? y : rect[1];
        timFile.width = rect[2];
        timFile.height = rect[3];

        // move past the rect (2 lots of uint32_t)
        ptr += 2;

        // get the image size (width * height pixels, each pixel is 2 bytes)
        uint32_t imageDataSize = timFile.width * timFile.height * sizeof(uint16_t);
        uint16_t * imageData = (uint16_t*)psyqo_malloc(imageDataSize);
        __builtin_memcpy(imageData, ptr, imageDataSize);

        // go to end.. do we really need to do this though
        ptr += imageDataSize / sizeof(uint32_t);

        if (timFile.width == 0 || timFile.height == 0 || timFile.colourMode > psyqo::Prim::TPageAttr::ColorMode::Tex16Bits) {
            printf("TEXTURE: Texture has no width (%d)/height (%d)/bpp (%d), aborting.\n", timFile.width, timFile.height, timFile.colourMode);
            buffer.clear();
            return;
        }

        // upload it to the vram
        Renderer::Instance().VRamUpload(imageData, timFile.x, timFile.y, timFile.width, timFile.height);

        printf("TEXTURE: Successfully loaded texture of %d bytes into VRAM.\n", buffer.size());

    // free data now we dont need it
    buffer.clear();

    // callback
    onComplete(timFile); });
}

psyqo::PrimPieces::TPageAttr TextureManager::GetTPageAttr(const TimFile &tim)
{
    psyqo::PrimPieces::TPageAttr tpage;
    tpage.setPageX(tim.x / texturePageWidth).setPageY(tim.y / texturePageHeight).enableDisplayArea().setDithering(true).set(tim.colourMode);

    return tpage;
};

psyqo::Rect TextureManager::GetTPageUVForTim(const TimFile &tim)
{
    uint16_t tpageX = (tim.x / texturePageWidth) * texturePageWidth, tpageY = (tim.y / texturePageHeight) * texturePageHeight;
    psyqo::Rect rect = {.pos{(tim.x - tpageX), (tim.y - tpageY)}};
    return rect;
}

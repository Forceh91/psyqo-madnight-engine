#ifndef _UI_SPRITE_HUD_ELEMENT_H
#define _UI_SPRITE_HUD_ELEMENT_H

#include <EASTL/fixed_string.h>
#include "hud_element.hh"
#include "hud_defines.hh"
#include "../../textures/texture_manager.hh"
#include "psyqo/primitives/common.hh"

class SpriteHUDElement final : public HUDElement
{
    TimFile *m_tim;
    psyqo::PrimPieces::UVCoords m_spriteUV;

public:
    SpriteHUDElement() : HUDElement("", {0, 0}) {};
    SpriteHUDElement(const char *name, psyqo::Rect rect) : HUDElement(name, rect) {};
    SpriteHUDElement(const char *name, psyqo::Rect rect, const char *textureName, psyqo::PrimPieces::UVCoords uv);
    void Render(const psyqo::Rect &parentRect);

    void SetSize(const psyqo::Vertex& size) { m_rect.size = size; }
    void SetUV(const psyqo::PrimPieces::UVCoords uv) { m_spriteUV = uv; }
};

#endif

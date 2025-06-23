#include "sprite_hud_element.h"

#include "text_hud_element.hh"
#include "../../render/renderer.hh"
#include "../../render/colour.hh"

SpriteHUDElement::SpriteHUDElement(const char *name, psyqo::Rect rect, const char *texture, psyqo::PrimPieces::UVCoords uv) : HUDElement(name, rect)
{
    // store the uv coords
    m_spriteUV = uv;

    // fetch the texture and store the tim
    TextureManager::GetTextureFromName(texture, &m_tim);
}

void SpriteHUDElement::Render(const psyqo::Rect &parentRect)
{
    if (!m_isEnabled)
        return;

    psyqo::Rect rect = {.pos = {parentRect.pos.x + m_rect.pos.x, parentRect.pos.y + m_rect.pos.y}, .size = m_rect.size};
    Renderer::Instance().RenderSprite(m_tim, rect, m_spriteUV);
}

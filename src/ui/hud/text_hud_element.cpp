#include "text_hud_element.hh"
#include "../../render/renderer.hh"
#include "../../render/colour.hh"

void TextHUDElement::Render(const psyqo::Rect &parentRect)
{
    if (!m_isEnabled)
        return;

    auto &rendererInstance = Renderer::Instance();
    auto font = rendererInstance.KromFont();

    psyqo::Vertex posInParent = {parentRect.pos.x + m_rect.pos.x, parentRect.pos.y + m_rect.pos.y};
    font->chainprintf(rendererInstance.GPU(), m_displayText, posInParent, COLOUR_WHITE);
}

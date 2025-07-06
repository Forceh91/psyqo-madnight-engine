#include "text_hud_element.hh"
#include "../../render/renderer.hh"

void TextHUDElement::Render(const psyqo::Rect &parentRect)
{
    if (!m_isEnabled)
        return;

    auto &rendererInstance = Renderer::Instance();

    if (m_font == nullptr)
        m_font = Renderer::Instance().KromFont();

    psyqo::Vertex posInParent = {parentRect.pos.x + m_rect.pos.x, parentRect.pos.y + m_rect.pos.y};
    m_font->chainprintf(rendererInstance.GPU(), posInParent, m_colour, m_displayText.c_str());
}

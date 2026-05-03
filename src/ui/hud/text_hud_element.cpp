#include "text_hud_element.hh"
#include "../../render/renderer.hh"

void TextHUDElement::Render(const psyqo::Rect &parentRect)
{
    Render(parentRect, nullptr);
}

void TextHUDElement::Render(const psyqo::Rect &parentRect, psyqo::Font<> *fallbackFont)
{
    if (!m_isEnabled)
        return;

    auto &rendererInstance = Renderer::Instance();

    if (m_font == nullptr)
        m_font = fallbackFont == nullptr ? rendererInstance.SystemFont() : fallbackFont;

    psyqo::Vertex posInParent = {static_cast<int16_t>(parentRect.pos.x + m_rect.pos.x), static_cast<int16_t>(parentRect.pos.y + m_rect.pos.y)};
    m_font->chainprintf(rendererInstance.GPU(), posInParent, m_colour, m_displayText.c_str());
}

#pragma once

#include "../../render/colour.hh"
#include "hud_defines.hh"
#include "hud_element.hh"
#include "psyqo/font.hh"
#include <EASTL/fixed_string.h>

class TextHUDElement final : public HUDElement {
	eastl::fixed_string<char, GAMEPLAY_HUD_ELEMENT_MAX_STR_LEN> m_displayText;
	psyqo::Color m_colour = COLOUR_WHITE;
	psyqo::Font<100>* m_font = nullptr;

  public:
	TextHUDElement() : HUDElement("", {0, 0}) {};
	TextHUDElement(const eastl::string_view& name, psyqo::Rect rect) : HUDElement(name, rect) {};
	TextHUDElement(const eastl::string_view& name, psyqo::Rect rect, psyqo::Color colour) : TextHUDElement(name, rect) {
		m_colour = colour;
	}
	void SetFont(psyqo::Font<100>* font) { m_font = font; }
	void SetDisplayText(const eastl::string_view& displayText) { m_displayText = displayText.data(); }
	void SetColour(const psyqo::Color colour) { m_colour = colour; }
	void SetPositionSize(psyqo::Rect rect) { m_rect = rect; }
	void Render(const psyqo::Rect& parentRect);
	void Render(const psyqo::Rect& parentRect, psyqo::Font<100>* defaultFont);
};

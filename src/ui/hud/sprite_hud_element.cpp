/*
 * Copyright (C) 2025-2026 Matt Hadden / Madnight Games
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "sprite_hud_element.hh"
#include "../../madnight.hh"
#include "../../render/renderer.hh"

SpriteHUDElement::SpriteHUDElement(const eastl::string_view& name, psyqo::Rect rect, const eastl::string_view& texture,
								   psyqo::PrimPieces::UVCoords uv)
	: HUDElement(name, rect) {
	// store the uv coords
	m_spriteUV = uv;

	// fetch the texture and store the tim
	g_madnightEngine.m_textureManager.GetTextureFromName(texture, &m_tim);
}

void SpriteHUDElement::Render(const psyqo::Rect& parentRect) {
	if (!m_isEnabled) {
		return;
	}

	if (!m_tim) {
		return;
	}

	psyqo::Rect rect = {.pos = {static_cast<int16_t>(parentRect.pos.x + m_rect.pos.x),
								static_cast<int16_t>(parentRect.pos.y + m_rect.pos.y)},
						.size = m_rect.size};
	Renderer::Instance().RenderSprite(m_tim, rect, m_spriteUV);
}

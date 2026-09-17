/*
 * Copyright (C) 2025-2026 Matt Hadden / Madnight Games
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include "../../textures/texture_manager.hh"
#include "hud_element.hh"
#include "psyqo/primitives/common.hh"
#include <EASTL/fixed_string.h>

class SpriteHUDElement final : public HUDElement {
	TimFile* m_tim = nullptr;
	psyqo::PrimPieces::UVCoords m_spriteUV = {0, 0};

  public:
	SpriteHUDElement() : HUDElement("", {0, 0}) {};
	SpriteHUDElement(const eastl::string_view& name, psyqo::Rect rect) : HUDElement(name, rect) {};
	SpriteHUDElement(const eastl::string_view& name, psyqo::Rect rect, const eastl::string_view& textureName,
					 psyqo::PrimPieces::UVCoords uv);
	void Render(const psyqo::Rect& parentRect);

	void SetSize(const psyqo::Vertex& size) { m_rect.size = size; }
	void SetUV(const psyqo::PrimPieces::UVCoords uv) { m_spriteUV = uv; }
};

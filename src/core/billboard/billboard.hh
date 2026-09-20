/*
 * Copyright (C) 2025-2026 Matt Hadden / Madnight Games
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once
#include "../../textures/texture_manager.hh"

#include <EASTL/array.h>
#include <EASTL/string_view.h>
#include <psyqo/primitives/common.hh>
#include <psyqo/vector.hh>

class Billboard {
  public:
	Billboard() = default;

	void Destroy(void);

	constexpr uint64_t nameHash() { return m_nameHash; }
	constexpr const int16_t& id() const { return m_id; }

	constexpr const psyqo::Vec3& pos() const { return m_pos; }
	constexpr const psyqo::Vec3* pPos() const { return &m_pos; }
	void SetPosition(const psyqo::Vec3 pos);

	constexpr const psyqo::Vec2& size() const { return m_size; }
	constexpr const psyqo::Vec2* pSize() const { return &m_size; }
	void setSize(const psyqo::Vec2 size);

	constexpr const psyqo::Color& colour() const { return m_colour; }
	constexpr const psyqo::Color* pColour() const { return &m_colour; }
	void SetColour(const psyqo::Color colour);

	const constexpr TimFile* pTexture() const { return m_texture; }
	void SetTexture(const eastl::string_view& textureName, const eastl::array<psyqo::PrimPieces::UVCoords, 4>& uv);
	void SetTexture(TimFile* texture, const eastl::array<psyqo::PrimPieces::UVCoords, 4>& uv);

	const constexpr eastl::array<psyqo::Vec3, 4>& corners() const { return m_quadCorners; }

	const constexpr eastl::array<psyqo::PrimPieces::UVCoords, 4>& uv() const { return m_uvCoords; }
	void SetUVCoords(const eastl::array<psyqo::PrimPieces::UVCoords, 4>& uv);

  protected:
	friend class BillboardManager;

	void Init(uint64_t nameHash, psyqo::Vec3 pos, psyqo::Vec2 size, int16_t id) {
		m_nameHash = nameHash;
		m_pos = pos;
		m_size = size;
		m_id = id;

		SetQuadCorners();
	}

	uint64_t m_nameHash = 0;
	int16_t m_id = INVALID_POOL_ID;
	psyqo::Vec3 m_pos = {0, 0, 0};
	psyqo::Vec2 m_size = {0, 0};
	psyqo::Color m_colour = {128, 128, 128};
	TimFile* m_texture = nullptr;
	eastl::array<psyqo::Vec3, 4> m_quadCorners;
	eastl::array<psyqo::PrimPieces::UVCoords, 4> m_uvCoords;

	void SetQuadCorners(void);
};

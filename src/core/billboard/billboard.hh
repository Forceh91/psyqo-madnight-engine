#pragma once
#include "../../textures/texture_manager.hh"
#include "defs.hh"

#include <EASTL/array.h>
#include <EASTL/string_view.h>
#include <psyqo/primitives/common.hh>

class Billboard {
  public:
	Billboard() = default;

	void Destroy(void);

	uint64_t nameHash() const { return m_nameHash; }
	const uint8_t& id() const { return m_id; }

	const psyqo::Vec3& pos() const { return m_pos; }
	const psyqo::Vec3* pPos() const { return &m_pos; }
	void SetPosition(const psyqo::Vec3 pos);

	const psyqo::Vec2& size() const { return m_size; }
	const psyqo::Vec2* pSize() const { return &m_size; }
	void setSize(const psyqo::Vec2 size);

	const psyqo::Color& colour() const { return m_colour; }
	const psyqo::Color* pColour() const { return &m_colour; }
	void SetColour(const psyqo::Color colour);

	const TimFile* pTexture() const { return m_texture; }
	void SetTexture(const eastl::string_view& textureName, const eastl::array<psyqo::PrimPieces::UVCoords, 4>& uv);
	void SetTexture(TimFile* texture, const eastl::array<psyqo::PrimPieces::UVCoords, 4>& uv);

	const eastl::array<psyqo::Vec3, 4>& corners() const { return m_quadCorners; }

	const eastl::array<psyqo::PrimPieces::UVCoords, 4>& uv() const { return m_uvCoords; }
	void SetUVCoords(const eastl::array<psyqo::PrimPieces::UVCoords, 4>& uv);

  protected:
	friend class BillboardManager;

	Billboard(uint64_t nameHash, psyqo::Vec3 pos, psyqo::Vec2 size, uint8_t id) {
		m_nameHash = nameHash;
		m_pos = pos;
		m_size = size;
		m_id = id;

		SetQuadCorners();
	}

	uint64_t m_nameHash = 0;
	uint8_t m_id = INVALID_BILLBOARD_ID;
	psyqo::Vec3 m_pos = {0, 0, 0};
	psyqo::Vec2 m_size = {0, 0};
	psyqo::Color m_colour = {128, 128, 128};
	TimFile* m_texture = nullptr;
	eastl::array<psyqo::Vec3, 4> m_quadCorners;
	eastl::array<psyqo::PrimPieces::UVCoords, 4> m_uvCoords;

	void SetQuadCorners(void);
};

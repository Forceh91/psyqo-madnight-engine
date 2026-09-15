#include "billboard.hh"
#include "../../madnight.hh"
#include "defs.hh"

#include <EASTL/array.h>
#include <EASTL/fixed_string.h>
#include <EASTL/string_view.h>
#include <psyqo/primitives/common.hh>

using namespace psyqo::fixed_point_literals;

void Billboard::Destroy(void) {
	m_nameHash = 0;
	m_pos = {0, 0, 0};
	m_size = {0, 0};
	m_texture = nullptr;
	m_id = INVALID_BILLBOARD_ID;
}

void Billboard::SetTexture(const eastl::string_view& textureName,
						   const eastl::array<psyqo::PrimPieces::UVCoords, 4>& uv) {
	g_madnightEngine.m_textureManager.GetTextureFromName(textureName, &m_texture);
	m_uvCoords = uv;
}

void Billboard::SetTexture(TimFile* texture, const eastl::array<psyqo::PrimPieces::UVCoords, 4>& uv) {
	m_texture = texture;
	m_uvCoords = uv;
}

void Billboard::SetUVCoords(const eastl::array<psyqo::PrimPieces::UVCoords, 4>& uv) { m_uvCoords = uv; }

void Billboard::SetPosition(const psyqo::Vec3 pos) { m_pos = pos; }

void Billboard::setSize(const psyqo::Vec2 size) {
	m_size = size;
	SetQuadCorners();
}

void Billboard::SetColour(const psyqo::Color colour) { m_colour = colour; }

void Billboard::SetQuadCorners(void) {
	m_quadCorners[0] = {-m_size.x / 2, m_size.y / 2, 0};
	m_quadCorners[1] = {m_size.x / 2, m_size.y / 2, 0};
	m_quadCorners[2] = {-m_size.x / 2, -m_size.y / 2, 0};
	m_quadCorners[3] = {m_size.x / 2, -m_size.y / 2, 0};
}

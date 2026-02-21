#include "billboard.hh"
#include "EASTL/fixed_string.h"
#include "defs.hh"
#include "psyqo/primitives/common.hh"

using namespace psyqo::fixed_point_literals;

void Billboard::Destroy(void) {
    m_name.clear();
    m_pos = {0,0,0};
    m_size = {0,0};
    m_texture = nullptr;
    m_id = INVALID_BILLBOARD_ID;
}

void Billboard::SetTexture(const eastl::fixed_string<char, MAX_BILLBOARD_NAME_LENGTH> &textureName) {
    TextureManager::GetTextureFromName(textureName.c_str(), &m_texture);
}

void Billboard::SetPosition(const psyqo::Vec3 pos) {
    m_pos = pos;
}

void Billboard::setSize(const psyqo::Vec2 size) {
    m_size = size;
}

void Billboard::SetColour(const psyqo::Color colour) {
    m_colour = colour;
}

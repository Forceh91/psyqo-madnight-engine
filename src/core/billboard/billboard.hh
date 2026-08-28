#pragma once

#include "defs.hh"
#include "../../textures/texture_manager.hh"

#include <psyqo/primitives/common.hh>

#include <EASTL/array.h>
#include <EASTL/fixed_string.h>

class Billboard {
public:
    Billboard() = default;
    void Init(const eastl::fixed_string<char, MAX_BILLBOARD_NAME_LENGTH>& name, psyqo::Vec3 pos, psyqo::Vec2 size, int16_t id) {
        m_nameHash = HashName(name);
        m_pos = pos;
        m_size = size;
        m_id = id;

        SetQuadCorners();
    }

    void Destroy(void);

    uint64_t nameHash() const { return m_nameHash; }
    const int16_t &id() const { return m_id; }
    
    const psyqo::Vec3 &pos() const { return m_pos; }
    const psyqo::Vec3 *pPos() const { return &m_pos; }
    void SetPosition(const psyqo::Vec3 pos);
    
    const psyqo::Vec2 &size() const { return m_size; }
    const psyqo::Vec2 *pSize() const { return &m_size; }
    void setSize(const psyqo::Vec2 size);

    const psyqo::Color &colour() const { return m_colour; }
    const psyqo::Color *pColour() const { return &m_colour; }
    void SetColour(const psyqo::Color colour);

    const TimFile *pTexture() const { return m_texture; }
    void SetTexture(const eastl::fixed_string<char, MAX_ARCHIVE_FILE_NAME_LEN> &textureName, const eastl::array<psyqo::PrimPieces::UVCoords, 4> &uv);
    void SetTexture(TimFile *texture, const eastl::array<psyqo::PrimPieces::UVCoords, 4> &uv);

    const eastl::array<psyqo::Vec3, 4> &corners() const { return m_quadCorners; }

    const eastl::array<psyqo::PrimPieces::UVCoords, 4> &uv() const { return m_uvCoords; }
    void SetUVCoords(const eastl::array<psyqo::PrimPieces::UVCoords, 4> &uv);
protected:
    uint64_t m_nameHash = 0;
    int16_t m_id = INVALID_POOL_ID;
    psyqo::Vec3 m_pos = {0,0,0};
    psyqo::Vec2 m_size = {0,0};
    psyqo::Color m_colour = {128,128,128};
    TimFile * m_texture = nullptr;
    eastl::array<psyqo::Vec3, 4> m_quadCorners;
    eastl::array<psyqo::PrimPieces::UVCoords, 4> m_uvCoords;

    void SetQuadCorners(void);
};

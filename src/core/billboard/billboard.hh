#ifndef _BILLBOARD_HH
#define _BILLBOARD_HH

#include "defs.hh"
#include "EASTL/fixed_string.h"
#include "../../textures/texture_manager.hh"
#include "psyqo/primitives/common.hh"

class Billboard final {
public:
    Billboard() = default;
    Billboard(eastl::fixed_string<char, MAX_BILLBOARD_NAME_LENGTH> name, psyqo::Vec3 pos, psyqo::Vec2 size, uint8_t id) {
        m_name = name;
        m_pos = pos;
        m_size = size;
        m_id = id;
    }

    void Destroy(void);

    const eastl::fixed_string<char, MAX_BILLBOARD_NAME_LENGTH> &name() const { return m_name; }
    const uint8_t &id() const { return m_id; }
    
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
    void SetTexture(const eastl::fixed_string<char, MAX_CDROM_FILE_NAME_LEN> &textureName);

private:
    eastl::fixed_string<char, MAX_BILLBOARD_NAME_LENGTH> m_name = "";
    uint8_t m_id = INVALID_BILLBOARD_ID;
    psyqo::Vec3 m_pos = {0,0,0};
    psyqo::Vec2 m_size = {0,0};
    psyqo::Color m_colour = {128,128,128};
    TimFile * m_texture = nullptr;
};

#endif
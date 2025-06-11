#ifndef _DEBUG_MENU_H
#define _DEBUG_MENU_H

#include "../../madnight.hh"
#include "psyqo/font.hh"
#include "../world_defs.hh"

class DebugMenu final
{
    static bool m_isEnabled;
    static psyqo::Font<> m_font;
    static uint8_t m_raycastDistance;
    static uint8_t m_selectedDebugOption;

    static void ToggleEnabled(void);

public:
    static void Init(void);
    static void Process(void);
    static void Draw(psyqo::GPU &gpu);
    static uint8_t RaycastDistance() { return m_raycastDistance; }
};

#endif

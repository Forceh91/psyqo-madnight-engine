#ifndef _DEBUG_MENU_H
#define _DEBUG_MENU_H

#include "../../madnight.hh"
#include "../world_defs.hh"

class DebugMenu final
{
    static bool m_isEnabled;
    static uint8_t m_raycastDistance;
    static uint8_t m_selectedDebugOption;
    static uint32_t m_startDebugMenuOpenCapture;
    static uint8_t m_debugMenuOpenCapturedInputs;
    static bool m_displayDebugHUD;

    static void ToggleEnabled(void);
    static void ResetInputCapture(void);

public:
    static void Init(void);
    static void Process(void);
    static void Draw(psyqo::GPU &gpu);
    static bool IsEnabled() { return m_isEnabled; }
    static uint8_t RaycastDistance() { return m_raycastDistance; }
    static bool DisplayDebugHUD() { return m_displayDebugHUD; }
};

#endif

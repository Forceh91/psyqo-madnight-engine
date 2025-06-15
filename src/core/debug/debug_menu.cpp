#include "debug_menu.hh"
#include "../../render/camera.hh"
#include "../../render/renderer.hh"
#include "../../render/colour.hh"

#include "psyqo/advancedpad.hh"
#include "psyqo/font.hh"
#include "psyqo/xprintf.h"

bool DebugMenu::m_isEnabled = false;
uint8_t DebugMenu::m_raycastDistance = 3;
uint8_t DebugMenu::m_selectedDebugOption = 0;
uint32_t DebugMenu::m_startDebugMenuOpenCapture = 0;
uint8_t DebugMenu::m_debugMenuOpenCapturedInputs = 0;

static constexpr uint8_t debugInputMask = (1 << 0) | (1 << 1) | (1 << 2) | (1 << 3);

// make sure you call this after initializing the renderer
void DebugMenu::Init()
{
    g_madnightEngine.m_input.setOnEvent([&](auto event)
                                        {
        if (event.type != psyqo::AdvancedPad::Event::ButtonReleased) return;

        if (m_isEnabled) {
            switch (m_selectedDebugOption)
            {
                case 0: // raycast distance
                    if (event.button == psyqo::AdvancedPad::Button::Left)
                        m_raycastDistance = (m_raycastDistance == 1) ? 10 : m_raycastDistance - 1;
                    if (event.button == psyqo::AdvancedPad::Button::Right)
                        m_raycastDistance = (m_raycastDistance % 10) + 1;
                break;
            }

            // if its open and we press triangle, disable it
            if (event.button == psyqo::AdvancedPad::Button::Triangle) m_isEnabled = false;
        } else {
            // its not open so lets watch for it trying to be opened
            // this happens if we press L1 + L2 + R1 + R2 in the space of 30 frames
            if (event.button == psyqo::AdvancedPad::Button::L1 || event.button == psyqo::AdvancedPad::Button::L2 || event.button == psyqo::AdvancedPad::Button::R1 || event.button == psyqo::AdvancedPad::Button::R2) {
                if (!m_startDebugMenuOpenCapture)
                    m_startDebugMenuOpenCapture = Renderer::Instance().GPU().getFrameCount();
                
                if (event.button == psyqo::AdvancedPad::Button::L1) m_debugMenuOpenCapturedInputs |= (1 << 0);
                if (event.button == psyqo::AdvancedPad::Button::L2) m_debugMenuOpenCapturedInputs |= (1 << 1);
                if (event.button == psyqo::AdvancedPad::Button::R1) m_debugMenuOpenCapturedInputs |= (1 << 2);
                if (event.button == psyqo::AdvancedPad::Button::R2) m_debugMenuOpenCapturedInputs |= (1 << 3);
                
                // if all of them are pressed
                if ((m_debugMenuOpenCapturedInputs&debugInputMask) == debugInputMask) {
                    ResetInputCapture();
                    m_isEnabled = true;
                }
            }
        } });
}

void DebugMenu::ToggleEnabled(void)
{
    m_isEnabled = !m_isEnabled;
}

void DebugMenu::Process(void)
{
    // if we have a debug menu open capture frame and its been too long, stop
    if (m_startDebugMenuOpenCapture != 0 && (Renderer::Instance().GPU().getFrameCount() - m_startDebugMenuOpenCapture) > 30)
        ResetInputCapture();
}

void DebugMenu::Draw(psyqo::GPU &gpu)
{
    if (!m_isEnabled)
        return;

    auto font = Renderer::Instance().KromFont();
    font->printf(gpu, {.x = 3, .y = 3}, COLOUR_WHITE, "Debug Menu");
    font->printf(gpu, {.x = 3, .y = 18}, m_selectedDebugOption == 0 ? COLOUR_YELLOW : COLOUR_WHITE, "Raycast Distance:");
    font->printf(gpu, {.x = 3, .y = 33}, m_selectedDebugOption == 0 ? COLOUR_YELLOW : COLOUR_WHITE, "   < %d >", m_raycastDistance);
}

void DebugMenu::ResetInputCapture(void)
{
    m_startDebugMenuOpenCapture = 0;
    m_debugMenuOpenCapturedInputs = 0;
}

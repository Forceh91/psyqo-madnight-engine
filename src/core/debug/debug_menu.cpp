#include "debug_menu.hh"
#include "psyqo/advancedpad.hh"
#include "../../render/camera.hh"
#include "../../render/renderer.hh"

bool DebugMenu::m_isEnabled = false;
psyqo::Font<> DebugMenu::m_font;
uint8_t DebugMenu::m_raycastDistance = 3;
uint8_t DebugMenu::m_selectedDebugOption = 0;

// make sure you call this after initializing the renderer
void DebugMenu::Init()
{
    m_font.uploadKromFont(Renderer::Instance().GPU());

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
        } });
}

void DebugMenu::ToggleEnabled(void)
{
    m_isEnabled = !m_isEnabled;
}

void DebugMenu::Process(void)
{
    const auto &input = &g_madnightEngine.m_input;

    if (input->isButtonPressed(psyqo::AdvancedPad::Pad::Pad1a, psyqo::AdvancedPad::Button::L1) && input->isButtonPressed(psyqo::AdvancedPad::Pad::Pad1a, psyqo::AdvancedPad::Button::L2) && input->isButtonPressed(psyqo::AdvancedPad::Pad::Pad1a, psyqo::AdvancedPad::Button::R1) && input->isButtonPressed(psyqo::AdvancedPad::Pad::Pad1a, psyqo::AdvancedPad::Button::R2))
        ToggleEnabled();
}

void DebugMenu::Draw(psyqo::GPU &gpu)
{
    if (!m_isEnabled)
        return;

    const psyqo::Color white = {.r = 255, .g = 255, .b = 255};
    const psyqo::Color yellow = {.r = 255, .g = 255, .b = 0};
    m_font.printf(gpu, {.x = 3, .y = 3}, white, "Debug Menu");
    m_font.printf(gpu, {.x = 3, .y = 18}, m_selectedDebugOption == 0 ? yellow : white, "Raycast Distance:");
    m_font.printf(gpu, {.x = 3, .y = 33}, m_selectedDebugOption == 0 ? yellow : white, "   < %d >", m_raycastDistance);
}

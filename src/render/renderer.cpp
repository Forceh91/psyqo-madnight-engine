#include "renderer.hh"
#include "../core/debug/debug_menu.hh"

Renderer *Renderer::m_instance = nullptr;

void Renderer::Init(psyqo::GPU &gpuInstance)
{
    if (m_instance != nullptr)
        return;

    m_instance = new Renderer(gpuInstance);
}

void Renderer::VRamUpload(const uint16_t *data, int16_t x, int16_t y, int16_t width, int16_t height)
{
    psyqo::Rect vramRegion = {.pos = {x, y}, .size = {width, height}};
    m_gpu.uploadToVRAM(data, vramRegion);
}

void Renderer::Render(void)
{
    // do this last so it goes on top
    DebugMenu::Draw(m_gpu);
}

#ifndef _RENDERER_H
#define _RENDERER_H

#include "psyqo/gpu.hh"
#include "psyqo/fragments.hh"
#include "psyqo/primitives.hh"
#include "psyqo/font.hh"

static constexpr uint16_t ORDERING_TABLE_SIZE = 1024;
static constexpr uint16_t QUAD_FRAGMENT_SIZE = 4096;
static constexpr psyqo::Color c_backgroundColour = {.r = 63, .g = 63, .b = 63};
static constexpr psyqo::Color c_loadingBackgroundColour = {.r = 0, .g = 0, .b = 0};

class Renderer final
{
    static Renderer *m_instance;
    static psyqo::Font<> m_kromFont;

    psyqo::GPU &m_gpu;
    uint32_t m_lastFrameCounter = 0;

    // create 2 ordering tables, one for each frame buffer
    psyqo::OrderingTable<ORDERING_TABLE_SIZE> m_orderingTables[2];

    // when using ordering tables we also need to sort fill commands as well
    psyqo::Fragments::SimpleFragment<psyqo::Prim::FastFill> m_clear[2];

    // we need a more permanent place to keep our quad info too
    eastl::array<psyqo::Fragments::SimpleFragment<psyqo::Prim::GouraudTexturedQuad>, QUAD_FRAGMENT_SIZE> m_quads[2];

    Renderer(psyqo::GPU &gpuInstance) : m_gpu(gpuInstance) {};
    ~Renderer() {};

    psyqo::Vec3 SetupCamera(void);

public:
    static void Init(psyqo::GPU &gpuInstance);

    void StartScene(void);
    void VRamUpload(const uint16_t *data, int16_t x, int16_t y, int16_t width, int16_t height);
    // returns the delta time
    // must be called on each scene frame
    uint32_t Process(void);
    void Render(void);
    void RenderLoadingScreen(uint16_t loadPercentage);

    static Renderer &Instance()
    {
        return *m_instance;
    }
    psyqo::GPU &GPU() { return m_gpu; }
    psyqo::Font<> *KromFont() { return &m_kromFont; }
};

#endif

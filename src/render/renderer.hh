#ifndef _RENDERER_H
#define _RENDERER_H

#include "psyqo/gpu.hh"
#include "psyqo/fragments.hh"
#include "psyqo/primitives.hh"

static constexpr unsigned ORDERING_TABLE_SIZE = 1024;
static constexpr psyqo::Color c_backgroundColour = {.r = 63, .g = 63, .b = 63};

class Renderer final
{
    static Renderer *m_instance;
    psyqo::GPU &m_gpu;

    // create 2 ordering tables, one for each frame buffer
    psyqo::OrderingTable<ORDERING_TABLE_SIZE> m_orderingTables[2];

    // when using ordering tables we also need to sort fill commands as well
    psyqo::Fragments::SimpleFragment<psyqo::Prim::FastFill> m_clear[2];

    Renderer(psyqo::GPU &gpuInstance) : m_gpu(gpuInstance) {};
    ~Renderer() {};

    psyqo::Vec3 SetupCamera(void);

public:
    static void Init(psyqo::GPU &gpuInstance);
    static Renderer &Instance()
    {
        return *m_instance;
    }

    void VRamUpload(const uint16_t *data, int16_t x, int16_t y, int16_t width, int16_t height);
    void Render(void);
    psyqo::GPU &GPU() { return m_gpu; }
};

#endif

#ifndef _RENDERER_H
#define _RENDERER_H

#include "psyqo/gpu.hh"

class Renderer final
{
    static Renderer *m_instance;

    Renderer(psyqo::GPU &gpuInstance) : m_gpu(gpuInstance) {};
    ~Renderer() {};
    psyqo::GPU &m_gpu;

public:
    static void Init(psyqo::GPU &gpuInstance);
    static Renderer &Instance()
    {
        return *m_instance;
    }

    void VRamUpload(const uint16_t *data, int16_t x, int16_t y, int16_t width, int16_t height);
};

#endif

#ifndef _LOADING_SCENE_H
#define _LOADING_SCENE_H

#include "../helpers/load_queue.hh"
#include "psyqo/coroutine.hh"
#include "psyqo/scene.hh"

class LoadingScene final : public psyqo::Scene
{
    void start(StartReason reason) override;
    void frame() override;

    uint16_t m_loadFilesCount = 0;
    uint16_t m_loadFilesLoadedCount = 0;

public:
    psyqo::Coroutine<> LoadFiles(eastl::vector<LoadQueue> files, bool dumpExisting);
};

#endif

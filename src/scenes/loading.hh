#ifndef _LOADING_SCENE_H
#define _LOADING_SCENE_H

#include "../helpers/load_queue.hh"
#include "psyqo/coroutine.hh"
#include "psyqo/scene.hh"

/*
 * this loading scene has been added for convenience.
 * it will automatically be called when you use
 * `co_await g_madnightEngine.HardLoadingScreen`
 * if you want to modify how the loading screen looks
 * then take a look at `LoadingScene::frame` and
 * `Renderer::RenderLoadingScreen`.
 */
class LoadingScene final : public psyqo::Scene
{
    void start(StartReason reason) override;
    void frame() override;

    eastl::vector<LoadQueue> m_queue;
    uint16_t m_loadFilesCount = 0;
    uint16_t m_loadFilesLoadedCount = 0;

public:
    psyqo::Coroutine<> LoadFiles(eastl::vector<LoadQueue> &&files, bool dumpExisting);
};

#endif

#pragma once

#include <psyqo/scene.hh>

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
};

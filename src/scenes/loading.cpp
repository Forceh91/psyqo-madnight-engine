#include "loading.hh"
#include "../render/renderer.hh"

void LoadingScene::start(StartReason reason)
{
    Renderer::Instance().StartScene();
}

void LoadingScene::frame()
{
    uint32_t deltaTime = Renderer::Instance().Process();

    Renderer::Instance().RenderLoadingScreen();
}

psyqo::Coroutine<> LoadingScene::InitializeLoading(void)
{
}

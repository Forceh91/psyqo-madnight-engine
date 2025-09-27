/* based off the psyqo cube example */

#include "psyqo/scene.hh"

#include "madnight.hh"
#include "helpers/cdrom.hh"
#include "render/renderer.hh"
#include "render/camera.hh"
#include "core/debug/debug_menu.hh"
#include "helpers/load_queue.hh"

#include "scenes/loading.hh"
#include "game.hh"

using namespace psyqo::fixed_point_literals;

static constexpr psyqo::Matrix33 identity = {{{1.0_fp, 0.0_fp, 0.0_fp}, {0.0_fp, 1.0_fp, 0.0_fp}, {0.0_fp, 0.0_fp, 1.0_fp}}};

// Our global application object. This is the only global
// object in this whole example. It will hold all of the
// other necessary classes.
MadnightEngine g_madnightEngine;

static LoadingScene loadingScene;

void MadnightEngine::prepare()
{
    // gpu config comes first, along with initialize.
    // once we call initialize then we can start using the vram etc.
    psyqo::GPU::Configuration gpu_config;
    gpu_config.set(psyqo::GPU::Resolution::W320).set(psyqo::GPU::VideoMode::NTSC).set(psyqo::GPU::ColorMode::C15BITS).set(psyqo::GPU::Interlace::PROGRESSIVE);
    gpu().initialize(gpu_config);

    // hardware inits
    CDRomHelper::init();
    // Unlike the `SimplePad` class, the `AdvancedPad` class doesn't need to be initialized
    // in the `start` method of the root `Scene` object. It can be initialized here.
    // PollingMode::Fast is used to reduce input lag, but it will increase CPU usage.
    // PollingMode::Normal is the default, and will poll one port per frame.
    m_input.initialize(psyqo::AdvancedPad::PollingMode::Fast);

    // gpu inits
    Renderer::Init(gpu());

    // our application inits
    CameraManager::init();
    DebugMenu::Init();
}

void MadnightEngine::createScene()
{
    m_initialLoadRoutine = InitialLoad();
    m_initialLoadRoutine.resume();
}

psyqo::Coroutine<> MadnightEngine::InitialLoad(void)
{
    co_await g_madnightEngineGame.InitialLoad();
}

void MadnightEngine::SwitchScene(psyqo::Scene *newScene, bool keepPrevious)
{
    if (!keepPrevious)
        popScene();

    pushScene(newScene);
}

psyqo::Coroutine<> MadnightEngine::HardLoadingScreen(eastl::vector<LoadQueue> &&files, psyqo::Scene *postLoadScene)
{
    popScene();
    pushScene(&loadingScene);

    co_await loadingScene.LoadFiles(eastl::move(files), true);

    popScene();
    pushScene(postLoadScene);
}

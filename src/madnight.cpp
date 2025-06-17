/* based off the psyqo cube example */

#include "psyqo/scene.hh"
#include "psyqo/xprintf.h"

#include "madnight.hh"
#include "helpers/cdrom.hh"
#include "render/renderer.hh"
#include "render/camera.hh"
#include "core/object/gameobject_manager.hh"
#include "core/debug/debug_menu.hh"
#include "helpers/load_queue.hh"

#include "scenes/loading.hh"
#include "scenes/gameplay.hh"

using namespace psyqo::fixed_point_literals;
using namespace psyqo::trig_literals;

static constexpr psyqo::Matrix33 identity = {{{1.0_fp, 0.0_fp, 0.0_fp}, {0.0_fp, 1.0_fp, 0.0_fp}, {0.0_fp, 0.0_fp, 1.0_fp}}};

// Our global application object. This is the only global
// object in this whole example. It will hold all of the
// other necessary classes.
MadnightEngine g_madnightEngine;

static GameplayScene gameplayScene;
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
    InitialLoad().resume();
}

psyqo::Coroutine<> MadnightEngine::InitialLoad(void)
{
    eastl::vector<LoadQueue> queue = {{.name = "TEXTURES/STREET.TIM", .type = LoadFileType::TEXTURE, .x = 320, .y = 0, .clutX = 0, .clutY = 240},
                                      {.name = "MODELS/STREET.MB", .type = LoadFileType::OBJECT}};

    // show loading screen
    co_await HardLoadingScreen(eastl::move(queue));

    // create a game object
    auto gameObject = GameObjectManager::CreateGameObject("STREET", {0, 0, 0}, {0, 0, 0}, GameObjectTag::ENVIRONMENT);
    if (gameObject != nullptr)
    {
        gameObject->SetQuadType(GameObjectQuadType::GouraudTextureQuad);
        gameObject->SetMesh("MODELS/STREET.MB");
        gameObject->SetTexture("TEXTURES/STREET.TIM");
    }

    auto gameObject2 = GameObjectManager::CreateGameObject("STREET2", {0.2_fp, 0, 0.5_fp}, {0, 1.0_pi, 0.25_pi}, GameObjectTag::ENVIRONMENT);
    if (gameObject2 != nullptr)
    {
        gameObject2->SetQuadType(GameObjectQuadType::GouraudTextureQuad);
        gameObject2->SetMesh("MODELS/STREET.MB");
        gameObject2->SetTexture("TEXTURES/STREET.TIM");
    }

    // regardless, switch to gameplay?
    SwitchToGameplay();
}

psyqo::Coroutine<> MadnightEngine::HardLoadingScreen(eastl::vector<LoadQueue> files)
{
    popScene();
    pushScene(&loadingScene);

    co_await loadingScene.LoadFiles(&files, true);
}

void MadnightEngine::SwitchToGameplay(void)
{
    popScene();
    pushScene(&gameplayScene);
}

int main() { return g_madnightEngine.run(); }

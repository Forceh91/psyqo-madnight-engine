/* based off the psyqo cube example */

#include "psyqo/fixed-point.hh"
#include "psyqo/fragments.hh"
#include "psyqo/primitives/common.hh"
#include "psyqo/primitives/quads.hh"
#include "psyqo/scene.hh"
#include "psyqo/vector.hh"
#include "psyqo/xprintf.h"
#include "helpers/cdrom.hh"
#include "mesh/mesh_manager.hh"
#include "render/camera.hh"
#include "madnight.hh"
#include "render/renderer.hh"
#include "textures/texture_manager.hh"
#include "core/raycast.hh"
#include "core/debug/debug_menu.hh"
#include "core/object/gameobject_manager.hh"

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
    pushScene(&loadingScene);

    // gameplayScene.LoadGameObject().resume();
}

// psyqo::Coroutine<> MadnightEngineScene::LoadGameObject()
// {
//     auto a = GameObjectManager::CreateGameObject("STREET", {0, 0, 0}, {0, 0, 0}, GameObjectTag::ENVIRONMENT);

//     co_await a->SetTexture("TEXTURES/STREET.TIM", 320, 0, 0, 240);
//     co_await a->SetMesh("MODELS/STREET.MB");
// }

int main() { return g_madnightEngine.run(); }

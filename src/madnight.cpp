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

using namespace psyqo::fixed_point_literals;
using namespace psyqo::trig_literals;

static constexpr psyqo::Matrix33 identity = {{{1.0_fp, 0.0_fp, 0.0_fp}, {0.0_fp, 1.0_fp, 0.0_fp}, {0.0_fp, 0.0_fp, 1.0_fp}}};

class MadnightEngineScene final : public psyqo::Scene
{
    void start(StartReason reason) override;
    void frame() override;

    psyqo::Angle m_rot = 0;
    psyqo::Vec3 m_camera_pos;
    psyqo::Vec3 m_camera_rot;
    uint32_t m_last_frame_counter = 0;

public:
    psyqo::Coroutine<> LoadGameObject();
};

static MadnightEngineScene engineScene;

// Our global application object. This is the only global
// object in this whole example. It will hold all of the
// other necessary classes.
MadnightEngine g_madnightEngine;

void MadnightEngine::prepare()
{
    // todo: move rendering into renderer class
    Renderer::Init(gpu());
    psyqo::GPU::Configuration gpu_config;
    gpu_config.set(psyqo::GPU::Resolution::W320).set(psyqo::GPU::VideoMode::NTSC).set(psyqo::GPU::ColorMode::C15BITS).set(psyqo::GPU::Interlace::PROGRESSIVE);
    gpu().initialize(gpu_config);
    CDRomHelper::init();

    // Unlike the `SimplePad` class, the `AdvancedPad` class doesn't need to be initialized
    // in the `start` method of the root `Scene` object. It can be initialized here.
    // PollingMode::Fast is used to reduce input lag, but it will increase CPU usage.
    // PollingMode::Normal is the default, and will poll one port per frame.
    m_input.initialize(psyqo::AdvancedPad::PollingMode::Fast);

    CameraManager::init();
    DebugMenu::Init();
}

void MadnightEngine::createScene()
{
    pushScene(&engineScene);

    engineScene.LoadGameObject().resume();
}

psyqo::Coroutine<> MadnightEngineScene::LoadGameObject()
{
    auto a = GameObjectManager::CreateGameObject("STREET", {0, 0, 0}, {0, 0, 0}, GameObjectTag::ENVIRONMENT);

    co_await a->SetTexture("TEXTURES/STREET.TIM", 320, 0, 0, 240);
    co_await a->SetMesh("MODELS/STREET.MB");
}

void MadnightEngineScene::start(StartReason reason)
{
    // clear translation registers
    psyqo::GTE::clear<psyqo::GTE::Register::TRX, psyqo::GTE::Unsafe>();
    psyqo::GTE::clear<psyqo::GTE::Register::TRY, psyqo::GTE::Unsafe>();
    psyqo::GTE::clear<psyqo::GTE::Register::TRZ, psyqo::GTE::Unsafe>();

    // set the screen offset in the GTE (half of the screen x/y resolution is the standard)
    psyqo::GTE::write<psyqo::GTE::Register::OFX, psyqo::GTE::Unsafe>(psyqo::FixedPoint<16>(160.0).raw());
    psyqo::GTE::write<psyqo::GTE::Register::OFY, psyqo::GTE::Unsafe>(psyqo::FixedPoint<16>(120.0).raw());

    // write the projection plane distance
    psyqo::GTE::write<psyqo::GTE::Register::H, psyqo::GTE::Unsafe>(120);

    // set the scaling for z averaging
    psyqo::GTE::write<psyqo::GTE::Register::ZSF3, psyqo::GTE::Unsafe>(ORDERING_TABLE_SIZE / 3);
    psyqo::GTE::write<psyqo::GTE::Register::ZSF4, psyqo::GTE::Unsafe>(ORDERING_TABLE_SIZE / 4);
}

void MadnightEngineScene::frame()
{
    uint32_t begin_frame = gpu().now();
    uint32_t current_frame_counter = gpu().getFrameCount();
    uint32_t delta_time = current_frame_counter - m_last_frame_counter;
    if (delta_time == 0)
        return;

    m_last_frame_counter = current_frame_counter;

    // process camera inputs
    CameraManager::process(delta_time);

    // process debug menu
    DebugMenu::Process();

    // the central point for rendering gameobjects etc
    Renderer::Instance().Render();
}

int main() { return g_madnightEngine.run(); }

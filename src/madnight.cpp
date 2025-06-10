/* based off the psyqo cube example */

#include "psyqo/fixed-point.hh"
#include "psyqo/fragments.hh"
#include "psyqo/gpu.hh"
#include "psyqo/gte-kernels.hh"
#include "psyqo/gte-registers.hh"
#include "psyqo/primitives/common.hh"
#include "psyqo/primitives/quads.hh"
#include "psyqo/scene.hh"
#include "psyqo/soft-math.hh"
#include "psyqo/vector.hh"
#include "psyqo/xprintf.h"
#include "helpers/cdrom.hh"
#include "mesh/mesh_manager.hh"
#include "helpers/camera.hh"
#include "madnight.hh"
#include "render/clip.hh"
#include "render/renderer.hh"
#include "textures/texture_manager.hh"

using namespace psyqo::fixed_point_literals;
using namespace psyqo::trig_literals;

static constexpr unsigned ORDERING_TABLE_SIZE = 1024;

static constexpr psyqo::Matrix33 identity = {{{1.0_fp, 0.0_fp, 0.0_fp}, {0.0_fp, 1.0_fp, 0.0_fp}, {0.0_fp, 0.0_fp, 1.0_fp}}};
static constexpr psyqo::Rect screen_space = {.pos = {0, 0}, .size = {320, 240}};

class MadnightEngineScene final : public psyqo::Scene
{
    void start(StartReason reason) override;
    void frame() override;

    psyqo::Angle m_rot = 0;
    psyqo::Vec3 m_camera_pos;
    psyqo::Vec3 m_camera_rot;
    uint32_t m_last_frame_counter = 0;

    // create 2 ordering tables, one for each frame buffer
    psyqo::OrderingTable<ORDERING_TABLE_SIZE> m_orderingTables[2];

    // when using ordering tables we also need to sort fill commands as well
    psyqo::Fragments::SimpleFragment<psyqo::Prim::FastFill> m_clear[2];

    static constexpr psyqo::Color c_backgroundColour = {.r = 63, .g = 63, .b = 63};

    // the mesh to display and its quads
    MESH *m_mesh = nullptr;
    psyqo::Vec3 m_mesh_pos = {0, 0, 0};
    eastl::array<psyqo::Fragments::SimpleFragment<psyqo::Prim::GouraudTexturedQuad>, MAX_FACES_PER_MESH> m_quads;

public:
    void fetch_cube_from_cdrom()
    {
        MeshManager::load_mesh_from_cdrom("MODELS/STREET.MB", [this](MESH *mesh)
                                          {
                                            if (mesh != nullptr)
                                                m_mesh = mesh;
                                            else
                                                printf("FETCH CUBE: No space to load into mesh manager\n");

                                            TextureManager::LoadTIMFromCDRom("TEXTURES/STREET.TIM", 320, 0, 0,240, [this](TimFile timFile)
                                         { m_mesh->tim = timFile; }); });
    }
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
}

void MadnightEngine::createScene()
{
    pushScene(&engineScene);
    engineScene.fetch_cube_from_cdrom();
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

    eastl::array<psyqo::Vertex, 4> projected;

    // get the frame buffer we're currently rendering
    int frame_buffer = gpu().getParity();

    // current ordering tables and fill command
    auto &ot = m_orderingTables[frame_buffer];
    auto &clear = m_clear[frame_buffer];

    // chain the fill command to clear the buffer
    gpu().getNextClear(clear.primitive, c_backgroundColour);
    gpu().chain(clear);

    // process camera inputs
    // todo: can we use m_input events for this? that doesn't support holding the button down?
    CameraManager::process(delta_time);

    // make sure we have a mesh
    if (m_mesh == nullptr)
    {
        gpu().chain(ot);
        return;
    }

    // clear TRX/Y/Z safely
    psyqo::GTE::clear<psyqo::GTE::Register::TRX, psyqo::GTE::Safe>();
    psyqo::GTE::clear<psyqo::GTE::Register::TRY, psyqo::GTE::Safe>();
    psyqo::GTE::clear<psyqo::GTE::Register::TRZ, psyqo::GTE::Safe>();

    // rotate camera translation vector by the camera rotation (matrix)
    psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::Rotation>(CameraManager::get_rotation_matrix());
    psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(-CameraManager::get_pos());

    // multiply vector by matrix and add vector
    psyqo::GTE::Kernels::mvmva<psyqo::GTE::Kernels::MX::RT, psyqo::GTE::Kernels::MV::V0, psyqo::GTE::Kernels::TV::TR>();
    psyqo::Vec3 camera_pos = psyqo::GTE::readSafe<psyqo::GTE::PseudoRegister::SV>();

    // rotate the object translation vector by the camera rotation
    psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(m_mesh_pos);
    psyqo::GTE::Kernels::mvmva<psyqo::GTE::Kernels::MX::RT, psyqo::GTE::Kernels::MV::V0, psyqo::GTE::Kernels::TV::TR>();
    psyqo::Vec3 object_pos = psyqo::GTE::readSafe<psyqo::GTE::PseudoRegister::SV>();

    // adjust object position by camera position
    object_pos += camera_pos;

    // set up the rotation for the spinning cube
    auto transform = psyqo::SoftMath::generateRotationMatrix33(m_rot, psyqo::SoftMath::Axis::X, g_madnightEngine.m_trig);
    auto rot = psyqo::SoftMath::generateRotationMatrix33(m_rot, psyqo::SoftMath::Axis::Y, g_madnightEngine.m_trig);

    // combine object and camera rotations
    psyqo::Matrix33 final_matrix;
    psyqo::SoftMath::multiplyMatrix33(CameraManager::get_rotation_matrix(), rot, &final_matrix);

    // write the object position and final matrix to the GTE
    psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::Translation>(object_pos);
    psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::Rotation>(final_matrix);

    for (int i = 0; i < m_mesh->faces_num; i++)
    {
        // load the first 3 verts into the GTE. remember it can only handle 3 at a time
        psyqo::GTE::writeUnsafe<psyqo::GTE::PseudoRegister::V0>(m_mesh->vertices[m_mesh->indices[i].v0]);
        psyqo::GTE::writeUnsafe<psyqo::GTE::PseudoRegister::V1>(m_mesh->vertices[m_mesh->indices[i].v1]);
        psyqo::GTE::writeUnsafe<psyqo::GTE::PseudoRegister::V2>(m_mesh->vertices[m_mesh->indices[i].v2]);

        // perform the rtpt (perspective transformation) on these three
        psyqo::GTE::Kernels::rtpt();

        // nclip determines the winding order of the vertices. if they are clockwise then it is facing towards us
        psyqo::GTE::Kernels::nclip();

        // read the result of this and skip rendering if its backfaced
        int32_t mac0 = 0;
        psyqo::GTE::read<psyqo::GTE::Register::MAC0>(reinterpret_cast<uint32_t *>(&mac0));
        if (mac0 <= 0)
            continue;

        // store these verts so we can read the last one in
        psyqo::GTE::read<psyqo::GTE::Register::SXY0>(&projected[0].packed);
        psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(m_mesh->vertices[m_mesh->indices[i].v3]);

        // again we need to rtps it
        psyqo::GTE::Kernels::rtps();

        // average z index for ordering
        psyqo::GTE::Kernels::avsz4();
        int32_t z_index = 0;
        psyqo::GTE::read<psyqo::GTE::Register::OTZ>(reinterpret_cast<uint32_t *>(&z_index));

        // make sure we dont go out of bounds
        if (z_index <= 0 || z_index >= ORDERING_TABLE_SIZE)
            continue;

        // if its out of the screen space we can clip too
        if (quad_clip(&screen_space, &projected[0], &projected[1], &projected[2], &projected[3]))
            continue;

        // get the three remaining verts from the GTE
        psyqo::GTE::read<psyqo::GTE::Register::SXY0>(&projected[1].packed);
        psyqo::GTE::read<psyqo::GTE::Register::SXY1>(&projected[2].packed);
        psyqo::GTE::read<psyqo::GTE::Register::SXY2>(&projected[3].packed);

        // now take a quad fragment from our array and:
        // set its vertices
        auto &quad = m_quads[i];
        quad.primitive.pointA = projected[0];
        quad.primitive.pointB = projected[1];
        quad.primitive.pointC = projected[2];
        quad.primitive.pointD = projected[3];

        // set its tpage
        quad.primitive.tpage = TextureManager::GetTPageAttr(m_mesh->tim);

        // set its clut if it has one
        if (m_mesh->tim.hasClut)
        {
            psyqo::PrimPieces::ClutIndex clut(m_mesh->tim.clutX, m_mesh->tim.clutY);
            quad.primitive.clutIndex = clut;
        }

        // set its uv coords
        psyqo::Rect offset = TextureManager::GetTPageUVForTim(m_mesh->tim);
        quad.primitive.uvA.u = offset.pos.x + m_mesh->uvs[m_mesh->uv_indices[i].v0].u;
        quad.primitive.uvA.v = offset.pos.y + (m_mesh->tim.height - 1 - m_mesh->uvs[m_mesh->uv_indices[i].v0].v);
        quad.primitive.uvB.u = offset.pos.x + m_mesh->uvs[m_mesh->uv_indices[i].v1].u;
        quad.primitive.uvB.v = offset.pos.y + (m_mesh->tim.height - 1 - m_mesh->uvs[m_mesh->uv_indices[i].v1].v);
        quad.primitive.uvC.u = offset.pos.x + m_mesh->uvs[m_mesh->uv_indices[i].v2].u;
        quad.primitive.uvC.v = offset.pos.y + (m_mesh->tim.height - 1 - m_mesh->uvs[m_mesh->uv_indices[i].v2].v);
        quad.primitive.uvD.u = offset.pos.x + m_mesh->uvs[m_mesh->uv_indices[i].v3].u;
        quad.primitive.uvD.v = offset.pos.y + (m_mesh->tim.height - 1 - m_mesh->uvs[m_mesh->uv_indices[i].v3].v);

        // set its colour, and make it opaque
        // TODO: make objects decide if they are gouraud shaded or not? saves processing time
        quad.primitive.setColorA({128, 128, 128});
        quad.primitive.setColorB({128, 128, 128});
        quad.primitive.setColorC({128, 128, 128});
        quad.primitive.setColorD({128, 128, 128});
        quad.primitive.setOpaque();

        // insert the quad fragment into the ordering table at the calculated z index
        ot.insert(quad, z_index);
    };

    // send the entire ordering table as a DMA chain to the GPU
    gpu().chain(ot);
    // m_rot += 0.005_pi;
}

int main() { return g_madnightEngine.run(); }

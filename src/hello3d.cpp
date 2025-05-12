/* based off the psyqo cube example */

#include "psyqo/application.hh"
#include "psyqo/fixed-point.hh"
#include "psyqo/fragments.hh"
#include "psyqo/gpu.hh"
#include "psyqo/gte-kernels.hh"
#include "psyqo/gte-registers.hh"
#include "psyqo/primitives/common.hh"
#include "psyqo/primitives/quads.hh"
#include "psyqo/scene.hh"
#include "psyqo/soft-math.hh"
#include "psyqo/trigonometry.hh"
#include "psyqo/vector.hh"
#include "psyqo/xprintf.h"
#include "helpers/cdrom.hh"
#include "mesh/mesh_manager.hh"

using namespace psyqo::fixed_point_literals;
using namespace psyqo::trig_literals;

static constexpr unsigned ORDERING_TABLE_SIZE = 1024;

static constexpr psyqo::Matrix33 identity = {{{1.0_fp, 0.0_fp, 0.0_fp}, {0.0_fp, 1.0_fp, 0.0_fp}, {0.0_fp, 0.0_fp, 1.0_fp}}};

class MadnightEngine final : public psyqo::Application
{
    void prepare() override;
    void createScene() override;

public:
    psyqo::Trig<> m_trig;
};

class MadnightEngineScene final : public psyqo::Scene
{
    void start(StartReason reason) override;
    void frame() override;

    psyqo::Angle m_rot = 0;

    // create 2 ordering tables, one for each frame buffer
    psyqo::OrderingTable<ORDERING_TABLE_SIZE> m_orderingTables[2];

    // when using ordering tables we also need to sort fill commands as well
    psyqo::Fragments::SimpleFragment<psyqo::Prim::FastFill> m_clear[2];

    static constexpr psyqo::Color c_backgroundColour = {.r = 63, .g = 63, .b = 63};

    // the mesh to display and its quads
    MESH *m_mesh = nullptr;
    eastl::array<psyqo::Fragments::SimpleFragment<psyqo::Prim::Quad>, MAX_FACES_PER_MESH> m_quads;

public:
    void fetch_cube_from_cdrom()
    {
        MeshManager::load_mesh_from_cdrom("MODELS/CUBE.MB", [this](MESH *mesh)
                                          { if (mesh != nullptr) m_mesh = mesh; else printf("FETCH CUBE: No space to load into mesh manager\n"); });
    }
};

static MadnightEngine engine;
static MadnightEngineScene engineScene;

void MadnightEngine::prepare()
{
    psyqo::GPU::Configuration gpu_config;
    gpu_config.set(psyqo::GPU::Resolution::W320).set(psyqo::GPU::VideoMode::NTSC).set(psyqo::GPU::ColorMode::C15BITS).set(psyqo::GPU::Interlace::PROGRESSIVE);
    gpu().initialize(gpu_config);
    CDRomHelper::init();
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
    eastl::array<psyqo::Vertex, 4> projected;

    // get the frame buffer we're currently rendering
    int frame_buffer = gpu().getParity();

    // current ordering tables and fill command
    auto &ot = m_orderingTables[frame_buffer];
    auto &clear = m_clear[frame_buffer];

    // chain the fill command to clear the buffer
    gpu().getNextClear(clear.primitive, c_backgroundColour);
    gpu().chain(clear);

    // make sure we have a mesh
    if (m_mesh == nullptr)
    {
        gpu().chain(ot);
        return;
    }

    // make the cube appear slightly further away
    psyqo::GTE::write<psyqo::GTE::Register::TRZ, psyqo::GTE::Unsafe>(10);

    // set up the rotation for the spinning cube
    auto transform = psyqo::SoftMath::generateRotationMatrix33(m_rot, psyqo::SoftMath::Axis::X, engine.m_trig);
    auto rot = psyqo::SoftMath::generateRotationMatrix33(m_rot, psyqo::SoftMath::Axis::Y, engine.m_trig);

    // multiply these matricies together
    psyqo::SoftMath::multiplyMatrix33(transform, rot, &transform);

    // generate a z-axis rotation matrix (this is empty as an example)
    psyqo::SoftMath::generateRotationMatrix33(&rot, 0, psyqo::SoftMath::Axis::Z, engine.m_trig);

    // apply the combined rotation and write it (to the pseudo register)
    psyqo::SoftMath::multiplyMatrix33(transform, rot, &transform);
    psyqo::GTE::writeUnsafe<psyqo::GTE::PseudoRegister::Rotation>(transform);

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
        if (z_index < 0 || z_index >= ORDERING_TABLE_SIZE)
            continue;

        // get the three remaining verts from the GTE
        psyqo::GTE::read<psyqo::GTE::Register::SXY0>(&projected[1].packed);
        psyqo::GTE::read<psyqo::GTE::Register::SXY1>(&projected[2].packed);
        psyqo::GTE::read<psyqo::GTE::Register::SXY2>(&projected[3].packed);

        // now take a quad fragment from our array and:
        // set its vertices, colour, and make it opaque
        auto &quad = m_quads[i];
        quad.primitive.setPointA(projected[0]);
        quad.primitive.setPointB(projected[1]);
        quad.primitive.setPointC(projected[2]);
        quad.primitive.setPointD(projected[3]);
        quad.primitive.setColor({0 + (i * 50), 0 + (i * 50), 128 + (i * 25)});
        quad.primitive.setOpaque();

        // insert the quad fragment into the ordering table at the calculated z index
        ot.insert(quad, z_index);
    };

    // send the entire ordering table as a DMA chain to the GPU
    gpu().chain(ot);
    m_rot += 0.005_pi;
}

int main() { return engine.run(); }

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
#include "helpers/cdrom.hh"
#include "psyqo/xprintf.h"

using namespace psyqo::fixed_point_literals;
using namespace psyqo::trig_literals;

static constexpr unsigned NUM_CUBE_VERTICES = 8;
static constexpr unsigned NUM_CUBE_FACES = 6;
static constexpr unsigned ORDERING_TABLE_SIZE = 240;

typedef struct
{
    uint8_t vertices[4];
    psyqo::Color color;
} Face;

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

    // our cube quads
    eastl::array<psyqo::Fragments::SimpleFragment<psyqo::Prim::Quad>, 6> m_quads;

    static constexpr psyqo::Color c_backgroundColour = {.r = 63, .g = 63, .b = 63};
    static constexpr psyqo::Vec3 c_cubeVerts[NUM_CUBE_VERTICES] = {
        {.x = -0.05, .y = -0.05, .z = -0.05}, {.x = 0.05, .y = -0.05, .z = -0.05}, {.x = -0.05, .y = 0.05, .z = -0.05}, {.x = 0.05, .y = 0.05, .z = -0.05}, {.x = -0.05, .y = -0.05, .z = 0.05}, {.x = 0.05, .y = -0.05, .z = 0.05}, {.x = -0.05, .y = 0.05, .z = 0.05}, {.x = 0.05, .y = 0.05, .z = 0.05}};

    static constexpr Face c_cubeFaces[NUM_CUBE_FACES] = {
        {.vertices = {0, 1, 2, 3}, .color = {0, 0, 255}}, {.vertices = {6, 7, 4, 5}, .color = {0, 255, 0}}, {.vertices = {4, 5, 0, 1}, .color = {0, 255, 255}}, {.vertices = {7, 6, 3, 2}, .color = {255, 0, 0}}, {.vertices = {6, 4, 2, 0}, .color = {255, 0, 255}}, {.vertices = {5, 7, 1, 3}, .color = {255, 255, 0}}};

public:
    void fetch_cube_from_cdrom()
    {
        // whatever you called the thing in the iso.xml
        char iso_file_name[MAX_FILE_NAME_LEN];
        CDRomHelper::get_iso_file_name("MODELS/CUBE.MB", iso_file_name);

        CDRomHelper::load_file(iso_file_name, [](void *data, size_t size)
                               { printf("loaded MB file with %d bytes of data\n", size); });
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

    // make the cube appear slightly further away
    psyqo::GTE::write<psyqo::GTE::Register::TRZ, psyqo::GTE::Unsafe>(512);

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

    int face_num = 0;
    for (Face face : c_cubeFaces)
    {
        // load the first 3 verts into the GTE. remember it can only handle 3 at a time
        psyqo::GTE::writeUnsafe<psyqo::GTE::PseudoRegister::V0>(c_cubeVerts[face.vertices[0]]);
        psyqo::GTE::writeUnsafe<psyqo::GTE::PseudoRegister::V1>(c_cubeVerts[face.vertices[1]]);
        psyqo::GTE::writeUnsafe<psyqo::GTE::PseudoRegister::V2>(c_cubeVerts[face.vertices[2]]);

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
        psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(c_cubeVerts[face.vertices[3]]);

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
        auto &quad = m_quads[face_num];
        quad.primitive.setPointA(projected[0]);
        quad.primitive.setPointB(projected[1]);
        quad.primitive.setPointC(projected[2]);
        quad.primitive.setPointD(projected[3]);
        quad.primitive.setColor(face.color);
        quad.primitive.setOpaque();

        // insert the quad fragment into the ordering table at the calculated z index
        ot.insert(quad, z_index);
        face_num++;
    };

    // send the entire ordering table as a DMA chain to the GPU
    gpu().chain(ot);
    m_rot += 0.005_pi;
}

int main() { return engine.run(); }

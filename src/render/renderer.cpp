#include "renderer.hh"
#include "camera.hh"
#include "clip.hh"
#include "colour.hh"

#include "../core/debug/debug_menu.hh"
#include "../core/object/gameobject_manager.hh"

#include "psyqo/gte-kernels.hh"
#include "psyqo/gte-registers.hh"
#include "psyqo/soft-math.hh"

Renderer *Renderer::m_instance = nullptr;
psyqo::Font<> Renderer::m_kromFont;
static constexpr psyqo::Rect screen_space = {.pos = {0, 0}, .size = {320, 240}};

void Renderer::Init(psyqo::GPU &gpuInstance)
{
    if (m_instance != nullptr)
        return;

    m_instance = new Renderer(gpuInstance);
    m_kromFont.uploadKromFont(m_instance->GPU());
}

void Renderer::VRamUpload(const uint16_t *data, int16_t x, int16_t y, int16_t width, int16_t height)
{
    psyqo::Rect vramRegion = {.pos = {x, y}, .size = {width, height}};
    m_gpu.uploadToVRAM(data, vramRegion);
}

void Renderer::StartScene(void)
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

uint32_t Renderer::Process(void)
{
    uint32_t beginFrameTimestamp = m_gpu.now(), currentFrameCount = m_gpu.getFrameCount();

    // figure out delta time (last frame count minus current frame count)
    uint32_t deltaTime = currentFrameCount - m_lastFrameCounter;
    if (deltaTime == 0)
        return 0;

    // update last frame count
    m_lastFrameCounter = currentFrameCount;

    // give back the delta time
    return deltaTime;
}

psyqo::Vec3 Renderer::SetupCamera(void)
{
    // clear TRX/Y/Z safely
    psyqo::GTE::clear<psyqo::GTE::Register::TRX, psyqo::GTE::Safe>();
    psyqo::GTE::clear<psyqo::GTE::Register::TRY, psyqo::GTE::Safe>();
    psyqo::GTE::clear<psyqo::GTE::Register::TRZ, psyqo::GTE::Safe>();

    // rotate camera translation vector by the camera rotation (matrix)
    psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::Rotation>(CameraManager::get_rotation_matrix());
    psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(-CameraManager::get_pos());

    // multiply vector by matrix and add vector
    psyqo::GTE::Kernels::mvmva<psyqo::GTE::Kernels::MX::RT, psyqo::GTE::Kernels::MV::V0, psyqo::GTE::Kernels::TV::TR>();
    return psyqo::GTE::readSafe<psyqo::GTE::PseudoRegister::SV>();
}

void Renderer::Render(void)
{
    // create a quad fragment array for it
    eastl::array<psyqo::Fragments::SimpleFragment<psyqo::Prim::GouraudTexturedQuad>, MAX_FACES_PER_MESH> quads;
    eastl::array<psyqo::Vertex, 4> projected;

    // get the frame buffer we're currently rendering
    int frame_buffer = m_gpu.getParity();

    // current ordering tables and fill command
    auto &ot = m_orderingTables[frame_buffer];
    auto &clear = m_clear[frame_buffer];

    // chain the fill command to clear the buffer
    m_gpu.getNextClear(clear.primitive, c_backgroundColour);
    m_gpu.chain(clear);

    // get game objects. if there's nothing to render then just early return
    auto gameObjects = GameObjectManager::GetGameObjects();
    if (gameObjects.empty())
    {
        m_gpu.chain(ot);
        return;
    }

    // setup camera
    auto cameraPos = SetupCamera();

    // now for each object...
    for (auto &gameObject : gameObjects)
    {
        // rotate the object translation vector by the camera rotation
        psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(gameObject->pos());
        psyqo::GTE::Kernels::mvmva<psyqo::GTE::Kernels::MX::RT, psyqo::GTE::Kernels::MV::V0, psyqo::GTE::Kernels::TV::TR>();
        psyqo::Vec3 objectPos = psyqo::GTE::readSafe<psyqo::GTE::PseudoRegister::SV>();

        // adjust object position by camera position
        objectPos += cameraPos;

        // get the rotation matrix for the game object and then combine with the camera rotations
        psyqo::Matrix33 finalMatrix = {0};
        psyqo::SoftMath::multiplyMatrix33(CameraManager::get_rotation_matrix(), gameObject->rotationMatrix(), &finalMatrix);

        // write the object position and final matrix to the GTE
        psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::Translation>(objectPos);
        psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::Rotation>(finalMatrix);

        // now we've done all this we can render the mesh and apply texture (if needed)
        auto mesh = gameObject->mesh();
        for (int i = 0; i < mesh->faces_num; i++)
        {
            // load the first 3 verts into the GTE. remember it can only handle 3 at a time
            psyqo::GTE::writeUnsafe<psyqo::GTE::PseudoRegister::V0>(mesh->vertices[mesh->indices[i].v0]);
            psyqo::GTE::writeUnsafe<psyqo::GTE::PseudoRegister::V1>(mesh->vertices[mesh->indices[i].v1]);
            psyqo::GTE::writeUnsafe<psyqo::GTE::PseudoRegister::V2>(mesh->vertices[mesh->indices[i].v2]);

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
            psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(mesh->vertices[mesh->indices[i].v3]);

            // again we need to rtps it
            psyqo::GTE::Kernels::rtps();

            // average z index for ordering
            psyqo::GTE::Kernels::avsz4();
            int32_t z_index = 0;
            psyqo::GTE::read<psyqo::GTE::Register::OTZ>(reinterpret_cast<uint32_t *>(&z_index));

            // make sure we dont go out of bounds
            if (z_index <= 0 || z_index >= ORDERING_TABLE_SIZE)
                continue;

            // get the three remaining verts from the GTE
            psyqo::GTE::read<psyqo::GTE::Register::SXY0>(&projected[1].packed);
            psyqo::GTE::read<psyqo::GTE::Register::SXY1>(&projected[2].packed);
            psyqo::GTE::read<psyqo::GTE::Register::SXY2>(&projected[3].packed);

            // if its out of the screen space we can clip too
            if (quad_clip(&screen_space, &projected[0], &projected[1], &projected[2], &projected[3]))
                continue;

            // now take a quad fragment from our array and:
            // set its vertices
            auto &quad = quads[i];
            quad.primitive.pointA = projected[0];
            quad.primitive.pointB = projected[1];
            quad.primitive.pointC = projected[2];
            quad.primitive.pointD = projected[3];

            // set its tpage
            auto texture = gameObject->texture();
            if (texture != nullptr)
            {
                quad.primitive.tpage = TextureManager::GetTPageAttr(*texture);

                // set its clut if it has one
                if (texture->hasClut)
                {
                    psyqo::PrimPieces::ClutIndex clut(texture->clutX, texture->clutY);
                    quad.primitive.clutIndex = clut;
                }

                // set its uv coords
                psyqo::Rect offset = TextureManager::GetTPageUVForTim(*texture);
                quad.primitive.uvA.u = offset.pos.x + mesh->uvs[mesh->uv_indices[i].v0].u;
                quad.primitive.uvA.v = offset.pos.y + (texture->height - 1 - mesh->uvs[mesh->uv_indices[i].v0].v);
                quad.primitive.uvB.u = offset.pos.x + mesh->uvs[mesh->uv_indices[i].v1].u;
                quad.primitive.uvB.v = offset.pos.y + (texture->height - 1 - mesh->uvs[mesh->uv_indices[i].v1].v);
                quad.primitive.uvC.u = offset.pos.x + mesh->uvs[mesh->uv_indices[i].v2].u;
                quad.primitive.uvC.v = offset.pos.y + (texture->height - 1 - mesh->uvs[mesh->uv_indices[i].v2].v);
                quad.primitive.uvD.u = offset.pos.x + mesh->uvs[mesh->uv_indices[i].v3].u;
                quad.primitive.uvD.v = offset.pos.y + (texture->height - 1 - mesh->uvs[mesh->uv_indices[i].v3].v);
            }

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
    }

    // send the entire ordering table as a DMA chain to the gpu
    m_gpu.chain(ot);

    // do this last incase it gets more complex and needs to go on top
    DebugMenu::Draw(m_gpu);
}

void Renderer::RenderLoadingScreen(uint16_t loadPercentage)
{
    uint32_t frameBuffer = m_gpu.getParity();
    auto &ot = m_orderingTables[frameBuffer];
    auto &clear = m_clear[frameBuffer];

    // clear the buffer
    m_gpu.getNextClear(clear.primitive, c_loadingBackgroundColour);
    m_gpu.chain(clear);

    // render the actual loading sprite/font/whatever
    m_kromFont.printf(m_gpu, {.x = 10, .y = 220}, COLOUR_WHITE, "Loading... (%d%%)", loadPercentage);
}

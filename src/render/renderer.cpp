#include "renderer.hh"
#include "clip.hh"
#include "colour.hh"

#include "../core/debug/debug_menu.hh"
#include "../core/object/gameobject_manager.hh"
#include "../math/gte-math.hh"
#include "../math/matrix.hh"

#include "psyqo/fixed-point.hh"
#include "psyqo/fragments.hh"
#include "psyqo/gte-kernels.hh"
#include "psyqo/gte-registers.hh"
#include "psyqo/matrix.hh"
#include "psyqo/primitives/common.hh"
#include "psyqo/primitives/control.hh"
#include "psyqo/primitives/lines.hh"
#include "psyqo/vector.hh"
#include "psyqo/xprintf.h"

Renderer *Renderer::m_instance = nullptr;
psyqo::Font<> Renderer::m_kromFont;
psyqo::Font<> Renderer::m_systemFont;
static constexpr psyqo::Rect screen_space = {.pos = {0, 0}, .size = {320, 240}};
static constexpr psyqo::Color boneColours[MAX_BONES] = {
    // Spine + neck
    {255, 255, 100}, // 0: Hips
    {255, 255, 120}, // 1: Spine
    {255, 255, 140}, // 2: Spine1
    {255, 255, 160}, // 3: Spine2
    {255, 255, 180}, // 4: Neck

    // Head
    {64, 224, 208},  // 5: Head (turquoise)
    {72, 239, 223},  // 6: HeadTop_End

    // Left Arm (blue-heavy)
    {0, 100, 255},   // 7: LeftShoulder
    {255, 120, 255},   // 8: LeftArm
    {125, 140, 255},   // 9: LeftForeArm
    {80, 160, 255},   // 10: LeftHand
    {0, 180, 255},   // 11: LeftHandThumb1
    {0, 200, 255},   // 12: LeftHandThumb2
    {0, 220, 255},   // 13: LeftHandThumb3
    {0, 240, 255},   // 14: LeftHandThumb4
    {0, 255, 220},   // 15: LeftHandIndex1
    {0, 255, 200},   // 16: LeftHandIndex2
    {0, 255, 180},   // 17: LeftHandIndex3
    {0, 255, 160},   // 18: LeftHandIndex4

    // Right Arm (green-heavy)
    {0, 255, 120},   // 19: RightShoulder
    {255, 255, 100},   // 20: RightArm
    {125, 220, 100},   // 21: RightForeArm
    {80, 200, 100},   // 22: RightHand
    {0, 180, 120},   // 23: RightHandThumb1
    {0, 160, 140},   // 24: RightHandThumb2
    {0, 140, 160},   // 25: RightHandThumb3
    {0, 120, 180},   // 26: RightHandThumb4
    {0, 100, 200},   // 27: RightHandIndex1
    {0, 100, 220},   // 28: RightHandIndex2
    {0, 100, 240},   // 29: RightHandIndex3
    {0, 120, 255},   // 30: RightHandIndex4

    // Left Leg (red/orange)
    {255, 100, 0},   // 31: LeftUpLeg
    {255, 120, 0},   // 32: LeftLeg
    {255, 140, 0},   // 33: LeftFoot
    {255, 160, 0},   // 34: LeftToeBase
    {255, 180, 0},   // 35: LeftToe_End

    // Right Leg (red/orange variant)
    {255, 100, 50},  // 36: RightUpLeg
    {255, 120, 50},  // 37: RightLeg
    {255, 140, 50},  // 38: RightFoot
    {255, 160, 50},  // 39: RightToeBase
    {255, 180, 50}   // 40: RightToe_End
};

void Renderer::Init(psyqo::GPU &gpuInstance) {
  if (m_instance != nullptr)
    return;

  m_instance = new Renderer(gpuInstance);
  m_kromFont.uploadKromFont(m_instance->GPU());
  m_systemFont.uploadSystemFont(m_instance->GPU(), {960, 256});
}

void Renderer::VRamUpload(const uint16_t *data, int16_t x, int16_t y, int16_t width, int16_t height) {
  psyqo::Rect vramRegion = {.pos = {x, y}, .size = {width, height}};
  m_gpu.uploadToVRAM(data, vramRegion);
}

void Renderer::StartScene(void) {
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

/* this must be called at the start of each frame */
uint32_t Renderer::Process(void) {
  uint32_t currentFrameCount = m_gpu.getFrameCount();

  // figure out delta time (last frame count minus current frame count)
  uint32_t deltaTime = currentFrameCount - m_lastFrameCounter;
  if (deltaTime == 0)
    return 0;

  // update last frame count
  m_lastFrameCounter = currentFrameCount;

  // reset what sprite/tpage we're drawing
  m_currentSpriteFragment = 0;

  // give back the delta time
  return deltaTime;
}

psyqo::Vec3 Renderer::SetupCamera(const psyqo::Matrix33 &camRotationMatrix, const psyqo::Vec3 &negativeCamPos) {
  // clear TRX/Y/Z safely
  psyqo::GTE::clear<psyqo::GTE::Register::TRX, psyqo::GTE::Safe>();
  psyqo::GTE::clear<psyqo::GTE::Register::TRY, psyqo::GTE::Safe>();
  psyqo::GTE::clear<psyqo::GTE::Register::TRZ, psyqo::GTE::Safe>();

  // rotate camera translation vector by the camera rotation (matrix)
  psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::Rotation>(camRotationMatrix);
  psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(negativeCamPos);

  // multiply vector by matrix and add vector
  psyqo::GTE::Kernels::mvmva<psyqo::GTE::Kernels::MX::RT, psyqo::GTE::Kernels::MV::V0, psyqo::GTE::Kernels::TV::TR>();
  return psyqo::GTE::readSafe<psyqo::GTE::PseudoRegister::SV>();
}

void Renderer::Render(void) { Render(1); }

void Renderer::Render(uint32_t deltaTime) {
  // create a quad fragment array for it
  eastl::array<psyqo::Vertex, 4> projected;

  // get the frame buffer we're currently rendering
  int frameBuffer = m_gpu.getParity();

  // current ordering tables and fill command
  auto &ot = m_orderingTables[frameBuffer];
  auto &clear = m_clear[frameBuffer];
  auto &quads = m_quads[frameBuffer];
  auto &lines = m_lines[frameBuffer];

  // chain the fill command to clear the buffer
  m_gpu.getNextClear(clear.primitive, c_backgroundColour);
  m_gpu.chain(clear);

  // get game objects. if there's nothing to render then just early return
  const auto &gameObjects = GameObjectManager::GetActiveGameObjects();
  if (gameObjects.empty())
    return;

  // make use of `gpu.pumpCallbacks` at some point in here
  // so that the gpu timers, and subsequently the modplayer
  // are able to get their timer ticks fired off on time
  // rendering the objects will be the most expensive part of the rendering
  m_gpu.pumpCallbacks();

  // fallback if we don't have an active camera set
  psyqo::Vec3 gteCameraPos = {0, 0, 0};
  psyqo::Matrix33 cameraRotationMatrix = {{0, 0, 0}};
  if (m_activeCamera != nullptr) {
    cameraRotationMatrix = m_activeCamera->rotationMatrix();
    gteCameraPos = SetupCamera(cameraRotationMatrix, -m_activeCamera->pos());
  }

  // now for each object...
  int quadFragment = 0;
  int lineFragment = 0;
  uint32_t zIndex = 0;
  psyqo::PrimPieces::TPageAttr tpage;
  psyqo::Rect offset = {0};

  for (const auto &gameObject : gameObjects) {
    // dont overflow our quads/faces/whatever
    if (quadFragment >= QUAD_FRAGMENT_SIZE)
      break;

    // we dont need to get mesh data for every single vert since it wont change, so lets only do that once
    const auto mesh = gameObject->mesh();
    if (mesh == nullptr)
      continue;

    // if we've got a skeleton on this mesh
    if (mesh->hasSkeleton) {
      SkeletonController::PlayAnimation(&mesh->skeleton, deltaTime);

      // update the bone/rotation matrices
      SkeletonController::UpdateSkeletonBoneMatrices(&mesh->skeleton);

      // adjust pos of verts that are attached to bones
      for (int i = 0; i < mesh->vertexCount; i++) {
        auto &bone = mesh->skeleton.bones[mesh->boneForVertex[i]];

        if (!bone.isDirty) continue;

        psyqo::Matrix33 rotationOffset;
        GTEMath::MultiplyMatrix33(bone.worldMatrix.rotationMatrix, bone.bindPoseInverse.rotationMatrix,
                                  &rotationOffset);

        // translation offset = world_T + (R_world * bindPoseInverse.translation)
        psyqo::Vec3 translationOffset;
        GTEMath::MultiplyMatrixVec3(bone.worldMatrix.rotationMatrix, bone.bindPoseInverse.translation,
                                    &translationOffset);
        translationOffset += bone.worldMatrix.translation;

        // final position
        psyqo::Vec3 rotatedVert;
        GTEMath::MultiplyMatrixVec3(rotationOffset, mesh->vertices[i], &rotatedVert);
        mesh->verticesOnBonePos[i] = rotatedVert + translationOffset;
      }

      // mark all bones as clean
      SkeletonController::MarkBonesClean(&mesh->skeleton);
    }

    // clear TRX/Y/Z safely
    psyqo::GTE::clear<psyqo::GTE::Register::TRX, psyqo::GTE::Safe>();
    psyqo::GTE::clear<psyqo::GTE::Register::TRY, psyqo::GTE::Safe>();
    psyqo::GTE::clear<psyqo::GTE::Register::TRZ, psyqo::GTE::Safe>();

    psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(gameObject->pos());
    psyqo::GTE::Kernels::rt();
    psyqo::Vec3 objectPos = psyqo::GTE::readSafe<psyqo::GTE::PseudoRegister::SV>();

    // adjust object position by camera position
    objectPos += gteCameraPos;

    // get the rotation matrix for the game object and then combine with the camera rotations
    psyqo::Matrix33 finalMatrix = {0};
    GTEMath::MultiplyMatrix33(cameraRotationMatrix, gameObject->rotationMatrix(), &finalMatrix);

    // write the object position and final matrix to the GTE
    psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::Translation>(objectPos);
    psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::Rotation>(finalMatrix);

    // now we've done all this we can render the mesh and apply texture (if needed)
    // we dont need to get texture data for every single vert since it wont change, so lets only do that once
    // if its not a nullptr fill out some data so we don't have to do it every face
    // NOTE: the nullptr check here is a free 0.2ms if you want it but it really should be done
    const auto texture = gameObject->texture();
    if (texture) {
      // get the tpage and uv offset info
      tpage = TextureManager::GetTPageAttr(texture);
      offset = TextureManager::GetTPageUVForTim(texture);
      offset.pos.y += (texture->height - 1);
    }

    for (int i = 0; i < mesh->facesCount; i++) {
      // dont overflow our quads/faces/whatever
      if (quadFragment >= QUAD_FRAGMENT_SIZE)
        break;

      // load the first 3 verts into the GTE. remember it can only handle 3 at a time
      psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(mesh->verticesOnBonePos[mesh->vertexIndices[i].i1]);
      psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V1>(mesh->verticesOnBonePos[mesh->vertexIndices[i].i2]);
      psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V2>(mesh->verticesOnBonePos[mesh->vertexIndices[i].i3]);

      // perform the rtpt (perspective transformation) on these three
      psyqo::GTE::Kernels::rtpt();

      // nclip determines the winding order of the vertices. if they are clockwise then it is facing towards us
      psyqo::GTE::Kernels::nclip();

      // read the result of this and skip rendering if its backfaced
      if (psyqo::GTE::readRaw<psyqo::GTE::Register::MAC0>() == 0)
        continue;

      // store these verts so we can read the last one in
      psyqo::GTE::read<psyqo::GTE::Register::SXY0>(&projected[0].packed);
      psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(mesh->verticesOnBonePos[mesh->vertexIndices[i].i4]);

      // again we need to rtps it
      psyqo::GTE::Kernels::rtps();

      // average z index for ordering
      psyqo::GTE::Kernels::avsz4();
      zIndex = psyqo::GTE::readRaw<psyqo::GTE::Register::OTZ>();

      // make sure we dont go out of bounds
      if (zIndex == 0 || zIndex >= ORDERING_TABLE_SIZE)
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
      auto &quad = quads[quadFragment++];
      quad.primitive.pointA = projected[0];
      quad.primitive.pointB = projected[1];
      quad.primitive.pointC = projected[2];
      quad.primitive.pointD = projected[3];

      // set its colour, and make it opaque
      // TODO: make objects decide if they are gouraud shaded or not? saves processing time
      quad.primitive.setColorA({128, 128, 128});
      quad.primitive.setColorB({128, 128, 128});
      quad.primitive.setColorC({128, 128, 128});
      quad.primitive.setColorD({128, 128, 128});

      quad.primitive.setOpaque();

      // do we have a texture for this?
      if (texture) {
        // set its tpage
        quad.primitive.tpage = tpage;

        // set its clut if it has one
        if (texture->hasClut)
          quad.primitive.clutIndex = {texture->clutX, texture->clutY};

        // set its uv coords
        auto uvA = mesh->uvs[mesh->uvIndices[i].i1];
        quad.primitive.uvA.u = offset.pos.x + uvA.u;
        quad.primitive.uvA.v = offset.pos.y - uvA.v;

        auto uvB = mesh->uvs[mesh->uvIndices[i].i2];
        quad.primitive.uvB.u = offset.pos.x + uvB.u;
        quad.primitive.uvB.v = offset.pos.y - uvB.v;

        auto uvC = mesh->uvs[mesh->uvIndices[i].i3];
        quad.primitive.uvC.u = offset.pos.x + uvC.u;
        quad.primitive.uvC.v = offset.pos.y - uvC.v;

        auto uvD = mesh->uvs[mesh->uvIndices[i].i4];
        quad.primitive.uvD.u = offset.pos.x + uvD.u;
        quad.primitive.uvD.v = offset.pos.y - uvD.v;
      }

      // finally we can insert the quad fragment into the ordering table at the calculated z-index
      ot.insert(quad, zIndex);
    };

    if (mesh->hasSkeleton && mesh->skeleton.numBones > 0) {
      for (int j = 0; j < mesh->skeleton.numBones; j++) {
        if (lineFragment >= QUAD_FRAGMENT_SIZE) break;
        
        auto &bone = mesh->skeleton.bones[j];
        psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(bone.startPos);
        psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V1>(bone.endPos);
        psyqo::GTE::Kernels::rtpt();
      
        psyqo::GTE::read<psyqo::GTE::Register::SXY0>(&projected[0].packed);
        psyqo::GTE::read<psyqo::GTE::Register::SXY1>(&projected[1].packed);

        auto &line = lines[lineFragment++];
        line.primitive.pointA = projected[0];
        line.primitive.pointB = projected[1];

        line.primitive.setColorA(boneColours[j]);
        line.primitive.setColorB(boneColours[j]);
        line.primitive.setOpaque();

        ot.insert(line, 1);
      }
    }
  }

  // send the entire ordering table as a DMA chain to the gpu
  m_gpu.chain(ot);

  // do this last incase it gets more complex and needs to go on top
  DebugMenu::Draw(m_gpu);
}

void Renderer::RenderLoadingScreen(uint16_t loadPercentage) {
  uint32_t frameBuffer = m_gpu.getParity();
  auto &clear = m_clear[frameBuffer];

  // clear the buffer
  m_gpu.getNextClear(clear.primitive, c_loadingBackgroundColour);
  m_gpu.chain(clear);

  // render the actual loading sprite/font/whatever
  m_kromFont.chainprintf(m_gpu, {10, 220}, COLOUR_WHITE, "Loading... (%d%%)", loadPercentage);
}

void Renderer::Clear() {
  uint32_t frameBuffer = m_gpu.getParity();
  auto &clear = m_clear[frameBuffer];

  // clear the buffer
  m_gpu.getNextClear(clear.primitive, c_loadingBackgroundColour);
  m_gpu.chain(clear);
}

void Renderer::RenderSprite(const TimFile *texture, const psyqo::Rect rect, const psyqo::PrimPieces::UVCoords uv) {
  // create a quad fragment array for it
  eastl::array<psyqo::Vertex, 4> projected;

  // get the frame buffer we're currently rendering
  int frameBuffer = m_gpu.getParity();

  // current ordering tables and fill command
  auto &clear = m_clear[frameBuffer];
  auto &tpages = m_tpages[frameBuffer];
  auto &sprites = m_sprites[frameBuffer];

  // chain tpage info over
  auto tpageAttr = TextureManager::GetTPageAttr(texture);
  auto &tpage = tpages[m_currentSpriteFragment];
  tpage.primitive.attr = tpageAttr;
  m_gpu.chain(tpage);

  // fix this so it actually renders more than one sprite
  auto &sprite = sprites[m_currentSpriteFragment++];
  sprite.primitive.position = rect.pos;
  sprite.primitive.size = rect.size;

  // set its clut if it has one
  if (texture->hasClut)
    sprite.primitive.texInfo.clut = psyqo::PrimPieces::ClutIndex(texture->clutX, texture->clutY);

  // set the uv data
  psyqo::Rect uvOffset = TextureManager::GetTPageUVForTim(texture);
  sprite.primitive.texInfo.u = uv.u; // uvOffset.pos.x + uv.u;
  sprite.primitive.texInfo.v = uv.v; // uvOffset.pos.y + (rect.size.y - 1 - uv.v);

  m_gpu.chain(sprite);
}

void Renderer::SetActiveCamera(Camera *camera) { m_activeCamera = camera; }

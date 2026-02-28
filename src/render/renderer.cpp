#include "renderer.hh"
#include "EASTL/algorithm.h"
#include "clip.hh"
#include "colour.hh"

#include "../core/debug/debug_menu.hh"
#include "../core/object/gameobject_manager.hh"
#include "../core/billboard/billboard_manager.hh"
#include "../core/particles/particle_manager.hh"
#include "../math/gte-math.hh"

#include "psyqo/fixed-point.hh"
#include "psyqo/fragments.hh"
#include "psyqo/gte-kernels.hh"
#include "psyqo/gte-registers.hh"
#include "psyqo/matrix.hh"
#include "psyqo/primitives/common.hh"
#include "psyqo/primitives/control.hh"
#include "psyqo/primitives/quads.hh"
#include "psyqo/primitives/sprites.hh"
#include "psyqo/vector.hh"
#include "../defs.hh"
#include <cstdint>


Renderer *Renderer::m_instance = nullptr;
psyqo::Font<> Renderer::m_kromFont;
psyqo::Font<> Renderer::m_systemFont;
static constexpr psyqo::Rect screen_space = {.pos = {0, 0}, .size = {320, 240}};
static constexpr psyqo::Matrix33 identityMatrix = {
    {{1.0_fp, 0.0_fp, 0.0_fp}, {0.0_fp, 1.0_fp, 0.0_fp}, {0.0_fp, 0.0_fp, 1.0_fp}}};
static constexpr uint8_t projectionDistance = 120;

#if ENABLE_BONE_DEBUG
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
#endif

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
  psyqo::GTE::write<psyqo::GTE::Register::H, psyqo::GTE::Unsafe>(projectionDistance);

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

  // get the current allocator so we can reset it
  auto &allocator = m_allocators[frameBuffer];
  allocator.reset();
  
  // chain the fill command to clear the buffer
  auto &clear = m_clear[frameBuffer];
  m_gpu.getNextClear(clear.primitive, c_backgroundColour);
  m_gpu.chain(clear);

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

  RenderGameObjects(deltaTime, gteCameraPos, cameraRotationMatrix);

  RenderBillboards(deltaTime, gteCameraPos, cameraRotationMatrix);

  RenderParticles(deltaTime, gteCameraPos, cameraRotationMatrix);

  // send the entire ordering table as a DMA chain to the gpu
  m_gpu.chain(m_orderingTables[frameBuffer]);

  // do this last incase it gets more complex and needs to go on top
  DebugMenu::Draw(m_gpu);
}

void Renderer::RenderGameObjects(uint32_t deltaTime, const psyqo::Vec3 gteCameraPos, const psyqo::Matrix33 &cameraRotationMatrix) {
  eastl::array<psyqo::Vertex, 4> projected;
  uint32_t zIndex = 0;
  psyqo::PrimPieces::TPageAttr tpage;
  psyqo::Rect offset = {0,0};

  auto frameBuffer = m_gpu.getParity();
  auto &allocator = m_allocators[frameBuffer];
  auto &ot = m_orderingTables[frameBuffer];

  // get game objects. if there's nothing to render then just early return
  const auto &gameObjects = GameObjectManager::GetActiveGameObjects();
  if (!gameObjects.empty()) {
    // now for each object...
    int lineFragment = 0;
    uint32_t zIndex = 0;
    psyqo::PrimPieces::TPageAttr tpage;
    psyqo::Rect offset = {0};

    for (const auto &gameObject : gameObjects) {
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

      // restore camera rotation for delta transform
      psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::Rotation>(cameraRotationMatrix);
      psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::Translation>(gteCameraPos);      

      // calculate delta pos of the object compared to the camera
      auto objDeltaPos = gameObject->pos() - m_activeCamera->pos();
      psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(objDeltaPos);
      psyqo::GTE::Kernels::rt();

      // get the view space translation and write it back for the meshes
      auto finalObjPos = psyqo::GTE::readSafe<psyqo::GTE::PseudoRegister::SV>();

      // get the rotation matrix for the game object and then combine with the camera rotations
      psyqo::Matrix33 finalCameraMatrix = {0};
      GTEMath::MultiplyMatrix33(cameraRotationMatrix, gameObject->rotationMatrix(), &finalCameraMatrix);

      // write the final rotation + translation
      psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::Rotation>(finalCameraMatrix);
      psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::Translation>(finalObjPos);

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
        auto &quad = allocator.allocateFragment<psyqo::Prim::GouraudTexturedQuad>();
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

  #if ENABLE_BONE_DEBUG
      if (mesh->hasSkeleton && mesh->skeleton.numBones > 0) {
        for (int j = 0; j < mesh->skeleton.numBones; j++) {         
          auto &bone = mesh->skeleton.bones[j];
          psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(bone.startPos);
          psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V1>(bone.endPos);
          psyqo::GTE::Kernels::rtpt();
        
          psyqo::GTE::read<psyqo::GTE::Register::SXY0>(&projected[0].packed);
          psyqo::GTE::read<psyqo::GTE::Register::SXY1>(&projected[1].packed);

          auto &line = allocator.allocateFragment<psyqo::Prim::Line>();
          line.primitive.pointA = projected[0];
          line.primitive.pointB = projected[1];

          line.primitive.setColorA(boneColours[j]);
          line.primitive.setColorB(boneColours[j]);
          line.primitive.setOpaque();

          ot.insert(line, 1);
        }
      }
  #endif
    }
  }
}

void Renderer::RenderBillboards(uint32_t deltaTime, const psyqo::Vec3 gteCameraPos, const psyqo::Matrix33 &cameraRotationMatrix) {
  eastl::array<psyqo::Vertex, 4> projected;
  uint32_t zIndex = 0;
  psyqo::PrimPieces::TPageAttr tpage;
  psyqo::Rect offset = {0,0};

  auto frameBuffer = m_gpu.getParity();
  auto &allocator = m_allocators[frameBuffer];
  auto &ot = m_orderingTables[frameBuffer];
  
  auto const &billboards = BillboardManager::GetActiveBillboards();
  if (billboards.empty())
    return;

  // billboards just use the inverse of the camera rotation matrix as rotation
  psyqo::Matrix33 finalCameraMatrix = {0};
  GTEMath::MultiplyMatrix33(cameraRotationMatrix, m_activeCamera->inverseRotationMatrix(), &finalCameraMatrix);

  for (auto const &billboard : billboards) {
    // clear TRX/Y/Z safely
    psyqo::GTE::clear<psyqo::GTE::Register::TRX, psyqo::GTE::Safe>();
    psyqo::GTE::clear<psyqo::GTE::Register::TRY, psyqo::GTE::Safe>();
    psyqo::GTE::clear<psyqo::GTE::Register::TRZ, psyqo::GTE::Safe>();

    psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::Rotation>(cameraRotationMatrix);
    psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(billboard->pos());
    psyqo::GTE::Kernels::rt();
    psyqo::Vec3 billboardPos = psyqo::GTE::readSafe<psyqo::GTE::PseudoRegister::SV>();

    billboardPos += gteCameraPos;

    // write the object position and camera rotation matrix
    psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::Translation>(billboardPos);
    psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::Rotation>(finalCameraMatrix);

    const auto texture = billboard->pTexture();
    if (texture) {
      tpage = TextureManager::GetTPageAttr(texture);
      offset = TextureManager::GetTPageUVForTim(texture);
      offset.pos.y += (texture->height - 1);
    }

    // load first 3 verts into GTE
    psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(billboard->corners()[0]);
    psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V1>(billboard->corners()[1]);
    psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V2>(billboard->corners()[2]);

    psyqo::GTE::Kernels::rtpt();
    psyqo::GTE::Kernels::nclip();

    if (!psyqo::GTE::readRaw<psyqo::GTE::Register::MAC0>())
      continue;

    // store the first vert so we can read the last one in
    psyqo::GTE::read<psyqo::GTE::Register::SXY0>(&projected[0].packed);
    psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(billboard->corners()[3]);

    psyqo::GTE::Kernels::rtps();

    psyqo::GTE::Kernels::avsz4();
    zIndex = psyqo::GTE::readRaw<psyqo::GTE::Register::OTZ>();

    if (zIndex == 0 || zIndex >= ORDERING_TABLE_SIZE)
      continue;

    // read the last three verts from GTE
    psyqo::GTE::read<psyqo::GTE::Register::SXY0>(&projected[1].packed);
    psyqo::GTE::read<psyqo::GTE::Register::SXY1>(&projected[2].packed);
    psyqo::GTE::read<psyqo::GTE::Register::SXY2>(&projected[3].packed);

    // out of screen space, it can be clipped
    if (quad_clip(&screen_space, &projected[0], &projected[1], &projected[2], &projected[3]))
      continue;

    // generate its points
    if (!texture) {
      auto &quad = allocator.allocateFragment<psyqo::Prim::GouraudQuad>();
      quad.primitive.pointA = projected[0];
      quad.primitive.pointB = projected[1];
      quad.primitive.pointC = projected[2];
      quad.primitive.pointD = projected[3];

      // set colour
      quad.primitive.setColorA(billboard->colour());
      quad.primitive.setColorB(billboard->colour());
      quad.primitive.setColorC(billboard->colour());
      quad.primitive.setColorD(billboard->colour());
      
      // make opaque
      quad.primitive.setOpaque();

      // insert into OT
      ot.insert(quad, zIndex);
    } else {
      auto &quad = allocator.allocateFragment<psyqo::Prim::GouraudTexturedQuad>();
      quad.primitive.pointA = projected[0];
      quad.primitive.pointB = projected[1];
      quad.primitive.pointC = projected[2];
      quad.primitive.pointD = projected[3];

      // set colour
      quad.primitive.setColorA(billboard->colour());
      quad.primitive.setColorB(billboard->colour());
      quad.primitive.setColorC(billboard->colour());
      quad.primitive.setColorD(billboard->colour());
      
      // make opaque
      quad.primitive.setOpaque();

      // set its tpage
      quad.primitive.tpage = tpage;

      // set its clut if it has one
      if (texture->hasClut)
        quad.primitive.clutIndex = {texture->clutX, texture->clutY};

      // set its uv coords
      auto uvA = billboard->uv()[0];
      quad.primitive.uvA.u = offset.pos.x + uvA.u;
      quad.primitive.uvA.v = offset.pos.y - uvA.v;

      auto uvB = billboard->uv()[1];
      quad.primitive.uvB.u = offset.pos.x + uvB.u;
      quad.primitive.uvB.v = offset.pos.y - uvB.v;

      auto uvC = billboard->uv()[2];
      quad.primitive.uvC.u = offset.pos.x + uvC.u;
      quad.primitive.uvC.v = offset.pos.y - uvC.v;

      auto uvD = billboard->uv()[3];
      quad.primitive.uvD.u = offset.pos.x + uvD.u;
      quad.primitive.uvD.v = offset.pos.y - uvD.v;

      // insert into OT
      ot.insert(quad, zIndex);
    }
  }
}

// TODO: somethings not quite right with rotation?
void Renderer::RenderParticles(uint32_t deltaTime, const psyqo::Vec3 gteCameraPos, const psyqo::Matrix33 &cameraRotationMatrix) {
  eastl::array<psyqo::Vertex, 4> projected;
  uint32_t zIndex = 0;
  psyqo::PrimPieces::TPageAttr tpage;
  psyqo::Rect offset = {0,0};

  auto frameBuffer = m_gpu.getParity();
  auto &allocator = m_allocators[frameBuffer];
  auto &ot = m_orderingTables[frameBuffer];
  
  auto const &emitters = ParticleEmitterManager::GetActiveEmitters();
  if (emitters.empty())
    return;

  // particles just use the inverse of the camera rotation matrix as rotation (when in 3d mode)
  psyqo::Matrix33 finalCameraMatrix = {0};
  GTEMath::MultiplyMatrix33(cameraRotationMatrix, m_activeCamera->inverseRotationMatrix(), &finalCameraMatrix);

  for (auto const &emitter : emitters) {
    auto const particles = emitter->particles();

    // send tpage info to gpu
    auto texture = emitter->pParticleTexture();
    if (texture) {
      auto tpageAttr = TextureManager::GetTPageAttr(texture);
      auto &tpage = allocator.allocateFragment<psyqo::Prim::TPage>();
      tpage.primitive.attr = tpageAttr;
      m_gpu.chain(tpage);
    }

    for (auto const &particle : particles) {
      // restore camera rotation for delta transform
      psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::Rotation>(cameraRotationMatrix);
      psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::Translation>(gteCameraPos);      

      // calculate delta pos of the object compared to the camera
      auto particleDeltaPos = particle.pos() - m_activeCamera->pos();
      psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(particleDeltaPos);
      psyqo::GTE::Kernels::rt();

      // get the view space translation and write it back for the meshes
      psyqo::Vec3 finalParticlePos = psyqo::GTE::readSafe<psyqo::GTE::PseudoRegister::SV>();

      if (emitter->AreParticles2D()) {
        if (finalParticlePos.z <= 0)
          continue;

        // write the object position and camera rotation matrix
        psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::Rotation>(identityMatrix);
        psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::Translation>(finalParticlePos);

        // we dont need to offset it
        psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(psyqo::Vec3{0,0,0});

        // single point rotation/translation
        psyqo::GTE::Kernels::rtps();

        // keep it within the OT
        zIndex = psyqo::GTE::readRaw<psyqo::GTE::Register::OTZ>();
        if (zIndex >= ORDERING_TABLE_SIZE)
          continue;
        
        // read in sxy2
        psyqo::Vertex vertex;
        psyqo::GTE::read<psyqo::GTE::Register::SXY2>(&vertex.packed);

        // calculate scaled size and make sure it doesnt go below 1
        auto projectedSize = particle.size() * (1.0_fp * projectionDistance) / finalParticlePos.z;
        auto scaledSize = psyqo::Vertex{static_cast<int16_t>(projectedSize.x.integer()), static_cast<int16_t>(projectedSize.y.integer())};
        scaledSize = {eastl::clamp<int16_t>(scaledSize.x, 1, scaledSize.x), eastl::clamp<int16_t>(scaledSize.y, 1, scaledSize.y)};

        // make sure pos is sane
        auto pos = psyqo::Vertex{static_cast<int16_t>(vertex.x - scaledSize.x / 2), static_cast<int16_t>(vertex.y - scaledSize.y / 2)};
        if (pos.x < 0 || pos.x >= screen_space.size.x || pos.y < 0 || pos.y >= screen_space.size.y)
          continue;

        auto &sprite = allocator.allocateFragment<psyqo::Prim::Sprite>();
        if (texture) {
          // update sprite with clut/uv data
          sprite.primitive.texInfo.clut = psyqo::PrimPieces::ClutIndex(texture->clutX, texture->clutY);
          
          // as particles can be quads they have 4 lots of uv data, so just take the first one
          sprite.primitive.texInfo.u = particle.uv()[0].u;
          sprite.primitive.texInfo.v = particle.uv()[0].v;
        }
        
        sprite.primitive.position = pos;
        sprite.primitive.size = scaledSize;
        sprite.primitive.setColor(particle.colour());

        ot.insert(sprite, zIndex);
      } else {
        // write the object position and camera rotation matrix
        psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::Rotation>(finalCameraMatrix);
        psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::Translation>(finalParticlePos);

        if (texture) {
          tpage = TextureManager::GetTPageAttr(texture);
          offset = TextureManager::GetTPageUVForTim(texture);
          offset.pos.y += (texture->height - 1);
        }

        // load first 3 verts into GTE
        psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(particle.corners()[0]);
        psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V1>(particle.corners()[1]);
        psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V2>(particle.corners()[2]);

        psyqo::GTE::Kernels::rtpt();
        psyqo::GTE::Kernels::nclip();

        if (!psyqo::GTE::readRaw<psyqo::GTE::Register::MAC0>())
          continue;

        // store the first vert so we can read the last one in
        psyqo::GTE::read<psyqo::GTE::Register::SXY0>(&projected[0].packed);
        psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(particle.corners()[3]);

        psyqo::GTE::Kernels::rtps();

        psyqo::GTE::Kernels::avsz4();
        zIndex = psyqo::GTE::readRaw<psyqo::GTE::Register::OTZ>();

        if (zIndex == 0 || zIndex >= ORDERING_TABLE_SIZE)
          continue;

        // read the last three verts from GTE
        psyqo::GTE::read<psyqo::GTE::Register::SXY0>(&projected[1].packed);
        psyqo::GTE::read<psyqo::GTE::Register::SXY1>(&projected[2].packed);
        psyqo::GTE::read<psyqo::GTE::Register::SXY2>(&projected[3].packed);

        // out of screen space, it can be clipped
        if (quad_clip(&screen_space, &projected[0], &projected[1], &projected[2], &projected[3]))
          continue;

        // generate its points
        if (!texture) {
          auto &quad = allocator.allocateFragment<psyqo::Prim::GouraudQuad>();
          quad.primitive.pointA = projected[0];
          quad.primitive.pointB = projected[1];
          quad.primitive.pointC = projected[2];
          quad.primitive.pointD = projected[3];

          // set colour
          quad.primitive.setColorA(particle.colour());
          quad.primitive.setColorB(particle.colour());
          quad.primitive.setColorC(particle.colour());
          quad.primitive.setColorD(particle.colour());
          
          // make opaque
          quad.primitive.setOpaque();

          // insert into OT
          ot.insert(quad, zIndex);
        } else {
          auto &quad = allocator.allocateFragment<psyqo::Prim::GouraudTexturedQuad>();
          quad.primitive.pointA = projected[0];
          quad.primitive.pointB = projected[1];
          quad.primitive.pointC = projected[2];
          quad.primitive.pointD = projected[3];

          // set colour
          quad.primitive.setColorA(particle.colour());
          quad.primitive.setColorB(particle.colour());
          quad.primitive.setColorC(particle.colour());
          quad.primitive.setColorD(particle.colour());
          
          // make opaque
          quad.primitive.setOpaque();

          // set its tpage
          quad.primitive.tpage = tpage;

          // set its clut if it has one
          if (texture->hasClut)
            quad.primitive.clutIndex = {texture->clutX, texture->clutY};

          // set its uv coords
          auto uvA = particle.uv()[0];
          quad.primitive.uvA.u = offset.pos.x + uvA.u;
          quad.primitive.uvA.v = offset.pos.y - uvA.v;

          auto uvB = particle.uv()[1];
          quad.primitive.uvB.u = offset.pos.x + uvB.u;
          quad.primitive.uvB.v = offset.pos.y - uvB.v;

          auto uvC = particle.uv()[2];
          quad.primitive.uvC.u = offset.pos.x + uvC.u;
          quad.primitive.uvC.v = offset.pos.y - uvC.v;

          auto uvD = particle.uv()[3];
          quad.primitive.uvD.u = offset.pos.x + uvD.u;
          quad.primitive.uvD.v = offset.pos.y - uvD.v;

          // insert into OT
          ot.insert(quad, zIndex);
        }
      }
    }
  }
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

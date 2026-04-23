#include "renderer.hh"
#include "EASTL/algorithm.h"
#include "clip.hh"
#include "colour.hh"

#include "../core/debug/debug_menu.hh"
#include "../core/object/gameobject_manager.hh"
#include "../core/billboard/billboard_manager.hh"
#include "../core/particles/particle_manager.hh"
#include "../core/debug/perf_monitor.hh"
#include "../math/gte-math.hh"
#include "../defs.hh"

#include "psyqo/fixed-point.hh"
#include "psyqo/fragment-concept.hh"
#include "psyqo/fragments.hh"
#include "psyqo/gte-kernels.hh"
#include "psyqo/gte-registers.hh"
#include "psyqo/matrix.hh"
#include "psyqo/primitive-concept.hh"
#include "psyqo/primitives/common.hh"
#include "psyqo/primitives/control.hh"
#include "psyqo/primitives/quads.hh"
#include "psyqo/primitives/sprites.hh"
#include "psyqo/primitives/triangles.hh"
#include "psyqo/vector.hh"

Renderer *Renderer::m_instance = nullptr;
psyqo::Font<> Renderer::m_systemFont;
static constexpr psyqo::Rect SCREEN_SPACE = {.pos = {0, 0}, .size = {320, 240}};
static constexpr psyqo::Matrix33 identityMatrix = {
    {{1.0_fp, 0.0_fp, 0.0_fp}, {0.0_fp, 1.0_fp, 0.0_fp}, {0.0_fp, 0.0_fp, 1.0_fp}}};
static constexpr uint8_t PROJECTION_DISTANCE = 120;

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
  psyqo::GTE::write<psyqo::GTE::Register::H, psyqo::GTE::Unsafe>(PROJECTION_DISTANCE);

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

psyqo::Vec3 Renderer::TransformObjectToViewSpace(const psyqo::Vec3 &pos, const psyqo::Matrix33 &cameraRotationMatrix, const psyqo::Matrix33 &finalCameraMatrix) {
  // restore camera rotation for delta transform
  psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::Rotation>(cameraRotationMatrix);
  psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::Translation>(m_gteCameraPos);

  // calculate delta pos of the object compared to the camera
  // hacky fix to get follow camera working: add on a delta offset before doing the final object delta pos
  // deltaOffset will return {0, 0, 0} when not in follow mode
  auto objDeltaPos = m_activeCamera->deltaOffset() + (pos - m_activeCamera->pos());
  psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(objDeltaPos);
  psyqo::GTE::Kernels::rt();

  // get the view space translation and write it back for the meshes
  auto finalObjPos = psyqo::GTE::readSafe<psyqo::GTE::PseudoRegister::SV>();

  // write the final rotation + translation
  psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::Rotation>(finalCameraMatrix);
  psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::Translation>(finalObjPos);

  return finalObjPos;
}

void Renderer::Render(void) { Render(1); }

void Renderer::Render(uint32_t deltaTime) {
  // get the frame buffer we're currently rendering
  int frameBuffer = m_gpu.getParity();

  // get the current allocator so we can reset it
  auto &allocator = m_allocators[frameBuffer];
  allocator.reset();
  
  // chain the fill command to clear the buffer
  auto &clear = m_clear[frameBuffer];
  m_gpu.getNextClear(clear.primitive, DEFAULT_CLEAR_COLOR);
  m_gpu.chain(clear);

  // make use of `gpu.pumpCallbacks` at some point in here
  // so that the gpu timers, and subsequently the modplayer
  // are able to get their timer ticks fired off on time
  // rendering the objects will be the most expensive part of the rendering
  m_gpu.pumpCallbacks();

  // fallback if we don't have an active camera set
  psyqo::Matrix33 cameraRotationMatrix = {{0, 0, 0}};
  if (m_activeCamera != nullptr) {
    cameraRotationMatrix = m_activeCamera->rotationMatrix();
    m_gteCameraPos = SetupCamera(cameraRotationMatrix, -m_activeCamera->pos());
  }

  RenderGameObjects(deltaTime, cameraRotationMatrix);

  RenderBillboards(deltaTime, cameraRotationMatrix);

  RenderParticles(deltaTime, cameraRotationMatrix);

  // send the entire ordering table as a DMA chain to the gpu
  m_gpu.chain(m_orderingTables[frameBuffer]);

  // do this last incase it gets more complex and needs to go on top
  DebugMenu::Draw(m_gpu);
}

void Renderer::RenderGameObjects(uint32_t deltaTime, const psyqo::Matrix33 &cameraRotationMatrix) {
  eastl::array<psyqo::Vertex, 4> projected;

  uint32_t zIndex = 0;
  psyqo::PrimPieces::TPageAttr tpage;
  psyqo::Rect offset = {0,0};
  psyqo::Matrix33 finalCameraMatrix = {0};

  auto frameBuffer = m_gpu.getParity();
  auto &allocator = m_allocators[frameBuffer];
  auto &ot = m_orderingTables[frameBuffer];

  // get game objects. if there's nothing to render then just early return
  const auto &gameObjects = GameObjectManager::GetActiveGameObjects();
  if (gameObjects.empty())
    return;

  // now for each object...
  int renderedObjects = 0;
  for (const auto &gameObject : gameObjects) {
    // we dont need to get mesh data for every single vert since it wont change, so lets only do that once
    const auto mesh = gameObject->mesh();
    if (!mesh)
      continue;

    // get the rotation matrix for the game object and then combine with the camera rotations
    GTEMath::MultiplyMatrix33(cameraRotationMatrix, gameObject->rotationMatrix(), &finalCameraMatrix);
  
    // see if the entire game object will be visible based off its aabb centre
    psyqo::Vec3 centre = gameObject->mesh()->bsphere.centre + gameObject->pos();
    auto deltaCentre = TransformObjectToViewSpace(centre, cameraRotationMatrix, finalCameraMatrix);
    if (!IsGameObjectVisible(deltaCentre, gameObject->mesh()->collisionBox, gameObject->mesh()->bsphere.radius))
      continue;

    // transform the game object into view space 
    renderedObjects++;
    TransformObjectToViewSpace(gameObject->pos(), cameraRotationMatrix, finalCameraMatrix);
      
    // if we've got a skeleton on this mesh
    if (mesh->hasSkeleton) {
      SkeletonController::PlayAnimation(mesh->skeleton, deltaTime);

      // update the bone/rotation matrices
      SkeletonController::UpdateSkeletonBoneMatrices(mesh->skeleton);

      // adjust pos of verts that are attached to bones
      for (int32_t i = 0; i < mesh->vertexCount; i++) {
        auto &bone = mesh->skeleton->bones[mesh->boneForVertex[i]];

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
      SkeletonController::MarkBonesClean(mesh->skeleton);
    }

    // now we've done all this we can render the mesh and apply texture (if needed)
    // we dont need to get texture data for every single vert since it wont change, so lets only do that once
    // if its not a nullptr fill out some data so we don't have to do it every face
    const auto texture = gameObject->texture();
    if (texture) {
      // get the tpage and uv offset info
      tpage = TextureManager::GetTPageAttr(texture);
      offset = TextureManager::GetTPageUVForTim(texture);
      offset.pos.y += (texture->height - 1);
    }

    auto renderVerts = mesh->hasSkeleton ? mesh->verticesOnBonePos : mesh->vertices;
    for (int32_t i = 0; i < mesh->facesCount; i++) {
      auto isQuad = mesh->vertexIndices[i].i2 != -1;

      // load the first 3 verts into the GTE. remember it can only handle 3 at a time
      psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(renderVerts[mesh->vertexIndices[i].i1]);
      psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V1>(isQuad ? renderVerts[mesh->vertexIndices[i].i2] : renderVerts[mesh->vertexIndices[i].i3]);
      psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V2>(isQuad ? renderVerts[mesh->vertexIndices[i].i3] : renderVerts[mesh->vertexIndices[i].i4]);

      // perform the rtpt (perspective transformation) on these three
      psyqo::GTE::Kernels::rtpt();

      // nclip determines the winding order of the vertices. if they are clockwise then it is facing towards us
      psyqo::GTE::Kernels::nclip();

      // read the result of this and skip rendering if its backfaced
      if (psyqo::GTE::readRaw<psyqo::GTE::Register::MAC0>() == 0)
        continue;

      // store these verts so we can read the last one in
      psyqo::GTE::read<psyqo::GTE::Register::SXY0>(&projected[0].packed);

      if (isQuad) {
        psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(renderVerts[mesh->vertexIndices[i].i4]);

        // again we need to rtps it
        psyqo::GTE::Kernels::rtps();

        // average z index for ordering
        psyqo::GTE::Kernels::avsz4();
      } else {
        // average z index for ordering
        psyqo::GTE::Kernels::avsz3();
      }
    
      zIndex = psyqo::GTE::readRaw<psyqo::GTE::Register::OTZ>();
      // make sure we dont go out of bounds
      if (zIndex == 0 || (m_isSimpleFogEnabled && zIndex >= FULL_FOG_DISTANCE) || zIndex >= ORDERING_TABLE_SIZE)
        continue;

      // get the three remaining verts from the GTE
      if (isQuad) {
        psyqo::GTE::read<psyqo::GTE::Register::SXY0>(&projected[1].packed);
        psyqo::GTE::read<psyqo::GTE::Register::SXY1>(&projected[2].packed);
        psyqo::GTE::read<psyqo::GTE::Register::SXY2>(&projected[3].packed);
      } else {
        psyqo::GTE::read<psyqo::GTE::Register::SXY1>(&projected[1].packed);
        psyqo::GTE::read<psyqo::GTE::Register::SXY2>(&projected[2].packed);        
      }

      // if its out of the screen space we can clip too
      if ((isQuad && quad_clip(&SCREEN_SPACE, &projected[0], &projected[1], &projected[2], &projected[3])) || !isQuad && tri_clip(&SCREEN_SPACE, &projected[0], &projected[1], &projected[2]))
        continue;

      auto applyUV = [&](auto& uvDest, int index) {
        auto uv = mesh->uvs[index];
        uvDest.u = offset.pos.x + uv.u;
        uvDest.v = offset.pos.y - uv.v;
      };

      if (isQuad) {
        // now take a quad fragment from our array and:
        // set its vertices
        auto &quad = allocator.allocateFragment<psyqo::Prim::GouraudTexturedQuad>();
        quad.primitive.pointA = projected[0];
        quad.primitive.pointB = projected[1];
        quad.primitive.pointC = projected[2];
        quad.primitive.pointD = projected[3];

        psyqo::Color colA = {mesh->vertexColours[mesh->vertexIndices[i].i1].r, mesh->vertexColours[mesh->vertexIndices[i].i1].g, mesh->vertexColours[mesh->vertexIndices[i].i1].b},
          colB = {mesh->vertexColours[mesh->vertexIndices[i].i2].r, mesh->vertexColours[mesh->vertexIndices[i].i2].g, mesh->vertexColours[mesh->vertexIndices[i].i2].b},
          colC = {mesh->vertexColours[mesh->vertexIndices[i].i3].r, mesh->vertexColours[mesh->vertexIndices[i].i3].g, mesh->vertexColours[mesh->vertexIndices[i].i3].b},
          colD = {mesh->vertexColours[mesh->vertexIndices[i].i4].r, mesh->vertexColours[mesh->vertexIndices[i].i4].g, mesh->vertexColours[mesh->vertexIndices[i].i4].b};

        if (m_isSimpleFogEnabled) {
          ApplyFogToColour(&colA, GetFogFactor(psyqo::GTE::readRaw<psyqo::GTE::Register::SZ0>()));
          ApplyFogToColour(&colB, GetFogFactor(psyqo::GTE::readRaw<psyqo::GTE::Register::SZ1>()));
          ApplyFogToColour(&colC, GetFogFactor(psyqo::GTE::readRaw<psyqo::GTE::Register::SZ2>()));
          ApplyFogToColour(&colD, GetFogFactor(psyqo::GTE::readRaw<psyqo::GTE::Register::SZ3>()));
        }

        // set its colour, and make it opaque
        quad.primitive.setColorA(colA);
        quad.primitive.setColorB(colB);
        quad.primitive.setColorC(colC);
        quad.primitive.setColorD(colD);
        quad.primitive.setOpaque();

        // do we have a texture for this?
        if (texture) {
          // set its tpage
          quad.primitive.tpage = tpage;

          // set its clut if it has one
          if (texture->hasClut)
            quad.primitive.clutIndex = {texture->clutX, texture->clutY};

          // set its uv coords
          applyUV(quad.primitive.uvA, mesh->uvIndices[i].i1);
          applyUV(quad.primitive.uvB, mesh->uvIndices[i].i2);
          applyUV(quad.primitive.uvC, mesh->uvIndices[i].i3);
          applyUV(quad.primitive.uvD, mesh->uvIndices[i].i4);
        }

        // finally we can insert the quad fragment into the ordering table at the calculated z-index
        if (zIndex <= SUBDIVISION_DISTANCE)
          SubdivideTexturedQuad(&quad, zIndex, &ot, 1);
        else
          ot.insert(quad, zIndex);
      } else {
        // now take a tri fragment from our array and:
        // set its vertices
        auto &tri = allocator.allocateFragment<psyqo::Prim::GouraudTexturedTriangle>();
        tri.primitive.pointA = projected[0];
        tri.primitive.pointB = projected[1];
        tri.primitive.pointC = projected[2];

        psyqo::Color colA = {mesh->vertexColours[mesh->vertexIndices[i].i1].r, mesh->vertexColours[mesh->vertexIndices[i].i1].g, mesh->vertexColours[mesh->vertexIndices[i].i1].b},
          colB = {mesh->vertexColours[mesh->vertexIndices[i].i3].r, mesh->vertexColours[mesh->vertexIndices[i].i3].g, mesh->vertexColours[mesh->vertexIndices[i].i3].b},
          colC = {mesh->vertexColours[mesh->vertexIndices[i].i4].r, mesh->vertexColours[mesh->vertexIndices[i].i4].g, mesh->vertexColours[mesh->vertexIndices[i].i4].b};

        if (m_isSimpleFogEnabled) {
          ApplyFogToColour(&colA, GetFogFactor(psyqo::GTE::readRaw<psyqo::GTE::Register::SZ0>()));
          ApplyFogToColour(&colB, GetFogFactor(psyqo::GTE::readRaw<psyqo::GTE::Register::SZ1>()));
          ApplyFogToColour(&colC, GetFogFactor(psyqo::GTE::readRaw<psyqo::GTE::Register::SZ2>()));
        }

        // set its colour, and make it opaque
        tri.primitive.setColorA(colA);
        tri.primitive.setColorB(colB);
        tri.primitive.setColorC(colC);
        tri.primitive.setOpaque();

        // do we have a texture for this?
        if (texture) {
          // set its tpage
          tri.primitive.tpage = tpage;

          // set its clut if it has one
          if (texture->hasClut)
            tri.primitive.clutIndex = {texture->clutX, texture->clutY};

          // set its uv coords
          applyUV(tri.primitive.uvA, mesh->uvIndices[i].i3);
          applyUV(tri.primitive.uvB, mesh->uvIndices[i].i1);
          applyUV(tri.primitive.uvC, mesh->uvIndices[i].i4);
        }

        // finally we can insert the quad fragment into the ordering table at the calculated z-index
        if (zIndex <= SUBDIVISION_DISTANCE)
          SubdivideTexturedTri(&tri, zIndex, &ot, 1);
        else
          ot.insert(tri, zIndex);
      }
    };

#if ENABLE_BONE_DEBUG
    if (mesh->hasSkeleton && mesh->skeleton.numBones > 0) {
      for (int j = 0; j < mesh->skeleton->numBones; j++) {         
        auto &bone = mesh->skeleton->bones[j];
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

  PerfMonitor::SetRenderedGameObjects(renderedObjects, gameObjects.size());
}

void Renderer::RenderBillboards(uint32_t deltaTime, const psyqo::Matrix33 &cameraRotationMatrix) {
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
    TransformObjectToViewSpace(billboard->pos(), cameraRotationMatrix, finalCameraMatrix);

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
    if (quad_clip(&SCREEN_SPACE, &projected[0], &projected[1], &projected[2], &projected[3]))
      continue;

    // generate its points
    if (!texture) {
      auto &quad = allocator.allocateFragment<psyqo::Prim::GouraudQuad>();
      quad.primitive.pointA = projected[0];
      quad.primitive.pointB = projected[1];
      quad.primitive.pointC = projected[2];
      quad.primitive.pointD = projected[3];

      // handle fog
      auto colour = billboard->colour();
      if (m_isSimpleFogEnabled) {
        auto sz = psyqo::GTE::readRaw<psyqo::GTE::Register::SZ1>();
        ApplyFogToColour(&colour, GetFogFactor(sz));
      }

      // set colour
      quad.primitive.setColorA(colour);
      quad.primitive.setColorB(colour);
      quad.primitive.setColorC(colour);
      quad.primitive.setColorD(colour);
      
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

      // handle fog
      auto colour = billboard->colour();
      if (m_isSimpleFogEnabled) {
        auto sz = psyqo::GTE::readRaw<psyqo::GTE::Register::SZ1>();
        ApplyFogToColour(&colour, GetFogFactor(sz));
      }

      // set colour
      quad.primitive.setColorA(colour);
      quad.primitive.setColorB(colour);
      quad.primitive.setColorC(colour);
      quad.primitive.setColorD(colour);
      
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
void Renderer::RenderParticles(uint32_t deltaTime, const psyqo::Matrix33 &cameraRotationMatrix) {
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
      auto finalParticlePos = TransformObjectToViewSpace(particle.pos(), cameraRotationMatrix, finalCameraMatrix);

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
        auto projectedSize = particle.size() * (1.0_fp * PROJECTION_DISTANCE) / finalParticlePos.z;
        auto scaledSize = psyqo::Vertex{static_cast<int16_t>(projectedSize.x.integer()), static_cast<int16_t>(projectedSize.y.integer())};
        scaledSize = {eastl::clamp<int16_t>(scaledSize.x, 1, scaledSize.x), eastl::clamp<int16_t>(scaledSize.y, 1, scaledSize.y)};

        // make sure pos is sane
        auto pos = psyqo::Vertex{static_cast<int16_t>(vertex.x - scaledSize.x / 2), static_cast<int16_t>(vertex.y - scaledSize.y / 2)};
        if (pos.x < 0 || pos.x >= SCREEN_SPACE.size.x || pos.y < 0 || pos.y >= SCREEN_SPACE.size.y)
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

        // handle fog
        auto colour = particle.colour();
        if (m_isSimpleFogEnabled) {
          auto sz = psyqo::GTE::readRaw<psyqo::GTE::Register::SZ1>();
          ApplyFogToColour(&colour, GetFogFactor(sz));
        }

        // set colour
        sprite.primitive.setColor(colour);

        ot.insert(sprite, zIndex);
      } else {
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
        if (quad_clip(&SCREEN_SPACE, &projected[0], &projected[1], &projected[2], &projected[3]))
          continue;

        // generate its points
        if (!texture) {
          auto &quad = allocator.allocateFragment<psyqo::Prim::GouraudQuad>();
          quad.primitive.pointA = projected[0];
          quad.primitive.pointB = projected[1];
          quad.primitive.pointC = projected[2];
          quad.primitive.pointD = projected[3];

          // handle fog
          auto colour = particle.colour();
          if (m_isSimpleFogEnabled) {
            auto sz = psyqo::GTE::readRaw<psyqo::GTE::Register::SZ1>();
            ApplyFogToColour(&colour, GetFogFactor(sz));
          }

          // set colour
          quad.primitive.setColorA(colour);
          quad.primitive.setColorB(colour);
          quad.primitive.setColorC(colour);
          quad.primitive.setColorD(colour);
          
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

          // handle fog
          auto colour = particle.colour();
          if (m_isSimpleFogEnabled) {
            auto sz = psyqo::GTE::readRaw<psyqo::GTE::Register::SZ1>();
            ApplyFogToColour(&colour, GetFogFactor(sz));
          }

          // set colour
          quad.primitive.setColorA(colour);
          quad.primitive.setColorB(colour);
          quad.primitive.setColorC(colour);
          quad.primitive.setColorD(colour);
          
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
  m_systemFont.chainprintf(m_gpu, {10, 210}, COLOUR_WHITE, "Loading... (%d%%)", loadPercentage);
}

void Renderer::Clear(psyqo::Color color) {
  uint32_t frameBuffer = m_gpu.getParity();
  auto &clear = m_clear[frameBuffer];

  // clear the buffer
  m_gpu.getNextClear(clear.primitive, color);
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

bool Renderer::IsGameObjectVisible(const psyqo::Vec3& cameraPos, const AABBCollision& collisionBox, const int32_t& boundingSphereRadius) {
    int32_t cx = cameraPos.x.value;
    int32_t cy = cameraPos.y.value;
    int32_t cz = cameraPos.z.value;
    int32_t r = boundingSphereRadius;

    // reject if sphere is entirely behind near plane
    if (cz + r <= 0) return false;

    // clamp to near plane to avoid divide by zero
    if (cz < 1) cz = 1;

    // project centre onto screen
    int32_t sx = (cx * PROJECTION_DISTANCE) / cz + SCREEN_SPACE.size.x / 2;
    int32_t sy = (cy * PROJECTION_DISTANCE) / cz + SCREEN_SPACE.size.y / 2;

    // screen space radius
    int32_t sr = (r * PROJECTION_DISTANCE) / cz;

    // reject if sphere projection does not overlap viewport
    if (sx + sr < 0 || sx - sr > SCREEN_SPACE.size.x)
      return false;

    if (sy + sr < 0 || sy - sr > SCREEN_SPACE.size.y)
      return false;


    return true;
}

void Renderer::SubdivideTexturedQuad(psyqo::Fragments::SimpleFragment<psyqo::Prim::GouraudTexturedQuad>* texturedQuad, uint32_t zIndex, psyqo::OrderingTable<ORDERING_TABLE_SIZE>* ot, uint8_t maxDepth) {
  auto& q = texturedQuad->primitive;
  auto& balloc = m_allocators[m_gpu.getParity()];

  if (balloc.remaining() < sizeof(psyqo::Prim::GouraudTexturedQuad) + 20) {
    ot->insert(*texturedQuad, zIndex);
    return;
  }

  // out of recursion depth, just insert as-is
  if (maxDepth == 0) {
    ot->insert(*texturedQuad, zIndex);
    return;
  }

  // check if quad is small enough to just insert
  auto width = eastl::max(q.pointA.x, eastl::max(q.pointB.x, eastl::max(q.pointC.x, q.pointD.x))) -
                eastl::min(q.pointA.x, eastl::min(q.pointB.x, eastl::min(q.pointC.x, q.pointD.x)));
  auto height = eastl::max(q.pointA.y, eastl::max(q.pointB.y, eastl::max(q.pointC.y, q.pointD.y))) -
                eastl::min(q.pointA.y, eastl::min(q.pointB.y, eastl::min(q.pointC.y, q.pointD.y)));

  if (width < 32 && height < 32) {
    ot->insert(*texturedQuad, zIndex);
    return;
  }

  // Z order: A=TL, B=TR, C=BL, D=BR
  // compare top edge (AB) vs left edge (AC) to decide split axis
  int16_t spanAB_x = q.pointB.x - q.pointA.x;
  int16_t spanAB_y = q.pointB.y - q.pointA.y;
  int16_t spanAC_x = q.pointC.x - q.pointA.x;
  int16_t spanAC_y = q.pointC.y - q.pointA.y;

  int32_t lenAB = spanAB_x * spanAB_x + spanAB_y * spanAB_y;
  int32_t lenAC = spanAC_x * spanAC_x + spanAC_y * spanAC_y;

  if (lenAB >= lenAC) {
    // split left/right: midpoints of AB (top) and CD (bottom)
    psyqo::Vertex midAB = {int16_t((q.pointA.x + q.pointB.x) >> 1), int16_t((q.pointA.y + q.pointB.y) >> 1)};
    psyqo::Vertex midCD = {int16_t((q.pointC.x + q.pointD.x) >> 1), int16_t((q.pointC.y + q.pointD.y) >> 1)};

    psyqo::PrimPieces::UVCoords uvAB = {uint8_t((q.uvA.u + q.uvB.u) >> 1), uint8_t((q.uvA.v + q.uvB.v) >> 1)};
    psyqo::PrimPieces::UVCoords uvCD = {uint8_t((q.uvC.u + q.uvD.u) >> 1), uint8_t((q.uvC.v + q.uvD.v) >> 1)};

    psyqo::Color colAB = {uint8_t((q.getColorA().r + q.colorB.r) >> 1), uint8_t((q.getColorA().g + q.colorB.g) >> 1), uint8_t((q.getColorA().b + q.colorB.b) >> 1)};
    psyqo::Color colCD = {uint8_t((q.colorC.r + q.colorD.r) >> 1), uint8_t((q.colorC.g + q.colorD.g) >> 1), uint8_t((q.colorC.b + q.colorD.b) >> 1)};

    // save original data before overwriting q
    auto origPointB = q.pointB;
    auto origUvB = q.uvB;
    auto origColorB = q.colorB;
    auto origPointD = q.pointD;
    auto origUvD = q.uvD;
    auto origColorD = q.colorD;

    // reuse original as left half: A(TL), midAB(TM), C(BL), midCD(BM)
    q.pointB = midAB;     q.uvB = uvAB;     q.setColorB(colAB);
    q.pointD = midCD;     q.uvD = {uvCD.u, uvCD.v, 0};     q.setColorD(colCD);
    // pointA and pointC stay the same

    // allocate one new quad for the right half: midAB(TM), B(TR), midCD(BM), D(BR)
    auto& q2 = balloc.allocateFragment<psyqo::Prim::GouraudTexturedQuad>();
    q2.primitive.pointA = midAB;        q2.primitive.uvA = uvAB;   q2.primitive.setColorA(colAB);
    q2.primitive.pointB = origPointB;   q2.primitive.uvB = origUvB;               q2.primitive.setColorB(origColorB);
    q2.primitive.pointC = midCD;        q2.primitive.uvC = {uvCD.u, uvCD.v, 0};   q2.primitive.setColorC(colCD);
    q2.primitive.pointD = origPointD;     q2.primitive.uvD = origUvD;                 q2.primitive.setColorD(origColorD);
    q2.primitive.tpage = q.tpage;       q2.primitive.clutIndex = q.clutIndex;
    q2.primitive.setOpaque();

    SubdivideTexturedQuad(texturedQuad, zIndex, ot, maxDepth - 1);
    SubdivideTexturedQuad(&q2, zIndex, ot, maxDepth - 1);
  } else {
    // split top/bottom: midpoints of AC (left) and BD (right)
    psyqo::Vertex midAC = {int16_t((q.pointA.x + q.pointC.x) >> 1), int16_t((q.pointA.y + q.pointC.y) >> 1)};
    psyqo::Vertex midBD = {int16_t((q.pointB.x + q.pointD.x) >> 1), int16_t((q.pointB.y + q.pointD.y) >> 1)};

    psyqo::PrimPieces::UVCoords uvAC = {uint8_t((q.uvA.u + q.uvC.u) >> 1), uint8_t((q.uvA.v + q.uvC.v) >> 1)};
    psyqo::PrimPieces::UVCoords uvBD = {uint8_t((q.uvB.u + q.uvD.u) >> 1), uint8_t((q.uvB.v + q.uvD.v) >> 1)};

    psyqo::Color colAC = {uint8_t((q.getColorA().r + q.colorC.r) >> 1), uint8_t((q.getColorA().g + q.colorC.g) >> 1), uint8_t((q.getColorA().b + q.colorC.b) >> 1)};
    psyqo::Color colBD = {uint8_t((q.colorB.r + q.colorD.r) >> 1), uint8_t((q.colorB.g + q.colorD.g) >> 1), uint8_t((q.colorB.b + q.colorD.b) >> 1)};

    // save original data before overwriting q
    auto origPointC = q.pointC;
    auto origUvC = q.uvC;
    auto origColorC = q.colorC;
    auto origPointD = q.pointD;
    auto origUvD = q.uvD;
    auto origColorD = q.colorD;

    // reuse original as top half: A(TL), B(TR), midAC(ML), midBD(MR)
    q.pointC = midAC;     q.uvC = {uvAC.u, uvAC.v, 0};     q.setColorC(colAC);
    q.pointD = midBD;     q.uvD = {uvBD.u, uvBD.v, 0};     q.setColorD(colBD);
    // pointA and pointB stay the same

    // allocate one new quad for the bottom half: midAC(ML), midBD(MR), C(BL), D(BR)
    auto& q2 = balloc.allocateFragment<psyqo::Prim::GouraudTexturedQuad>();
    q2.primitive.pointA = midAC;        q2.primitive.uvA = uvAC;   q2.primitive.setColorA(colAC);
    q2.primitive.pointB = midBD;        q2.primitive.uvB = uvBD;   q2.primitive.setColorB(colBD);
    q2.primitive.pointC = origPointC;   q2.primitive.uvC = origUvC;               q2.primitive.setColorC(origColorC);
    q2.primitive.pointD = origPointD;   q2.primitive.uvD = origUvD;               q2.primitive.setColorD(origColorD);
    q2.primitive.tpage = q.tpage;       q2.primitive.clutIndex = q.clutIndex;
    q2.primitive.setOpaque();

    SubdivideTexturedQuad(texturedQuad, zIndex, ot, maxDepth - 1);
    SubdivideTexturedQuad(&q2, zIndex, ot, maxDepth - 1);
  }
}


void Renderer::SubdivideTexturedTri(psyqo::Fragments::SimpleFragment<psyqo::Prim::GouraudTexturedTriangle>* tri, uint32_t zIndex, psyqo::OrderingTable<ORDERING_TABLE_SIZE>* ot, uint8_t maxDepth) {
  auto& t = tri->primitive;
  auto& balloc = m_allocators[m_gpu.getParity()];

  if (balloc.remaining() < sizeof(psyqo::Prim::GouraudTexturedTriangle) + 20) {
    ot->insert(*tri, zIndex);
    return;
  }

  if (maxDepth == 0) {
    ot->insert(*tri, zIndex);
    return;
  }

  // bounding box size check
  auto minX = eastl::min(t.pointA.x, eastl::min(t.pointB.x, t.pointC.x));
  auto maxX = eastl::max(t.pointA.x, eastl::max(t.pointB.x, t.pointC.x));
  auto minY = eastl::min(t.pointA.y, eastl::min(t.pointB.y, t.pointC.y));
  auto maxY = eastl::max(t.pointA.y, eastl::max(t.pointB.y, t.pointC.y));

  auto width = maxX - minX;
  auto height = maxY - minY;

  if (width < 32 && height < 32) {
    ot->insert(*tri, zIndex);
    return;
  }

  // edges: AB, BC, CA
  int16_t abx = t.pointB.x - t.pointA.x;
  int16_t aby = t.pointB.y - t.pointA.y;
  int16_t bcx = t.pointC.x - t.pointB.x;
  int16_t bcy = t.pointC.y - t.pointB.y;
  int16_t cax = t.pointA.x - t.pointC.x;
  int16_t cay = t.pointA.y - t.pointC.y;

  int32_t lenAB = abx * abx + aby * aby;
  int32_t lenBC = bcx * bcx + bcy * bcy;
  int32_t lenCA = cax * cax + cay * cay;

  // find longest edge
  if (lenAB >= lenBC && lenAB >= lenCA) {
    // split AB

    psyqo::Vertex midAB = {
      int16_t((t.pointA.x + t.pointB.x) >> 1),
      int16_t((t.pointA.y + t.pointB.y) >> 1)
    };

    psyqo::PrimPieces::UVCoords uvAB = {
      uint8_t((t.uvA.u + t.uvB.u) >> 1),
      uint8_t((t.uvA.v + t.uvB.v) >> 1)
    };

    psyqo::Color colAB = {
      uint8_t((t.getColorA().r + t.colorB.r) >> 1),
      uint8_t((t.getColorA().g + t.colorB.g) >> 1),
      uint8_t((t.getColorA().b + t.colorB.b) >> 1)
    };

    // save originals
    auto origB = t.pointB;
    auto origUvB = t.uvB;
    auto origColB = t.colorB;

    // reuse original: A, midAB, C
    t.pointB = midAB;
    t.uvB = uvAB;
    t.setColorB(colAB);

    // new tri: midAB, B, C
    auto& t2 = balloc.allocateFragment<psyqo::Prim::GouraudTexturedTriangle>();
    t2.primitive.pointA = midAB;     t2.primitive.uvA = uvAB;     t2.primitive.setColorA(colAB);
    t2.primitive.pointB = origB;     t2.primitive.uvB = origUvB;  t2.primitive.setColorB(origColB);
    t2.primitive.pointC = t.pointC;  t2.primitive.uvC = t.uvC;    t2.primitive.setColorC(t.colorC);
    t2.primitive.tpage = t.tpage;
    t2.primitive.clutIndex = t.clutIndex;
    t2.primitive.setOpaque();

    SubdivideTexturedTri(tri, zIndex, ot, maxDepth - 1);
    SubdivideTexturedTri(&t2, zIndex, ot, maxDepth - 1);
  }
  else if (lenBC >= lenCA) {
    // split BC

    psyqo::Vertex midBC = {
      int16_t((t.pointB.x + t.pointC.x) >> 1),
      int16_t((t.pointB.y + t.pointC.y) >> 1)
    };

    psyqo::PrimPieces::UVCoords uvBC = {
      uint8_t((t.uvB.u + t.uvC.u) >> 1),
      uint8_t((t.uvB.v + t.uvC.v) >> 1)
    };

    psyqo::Color colBC = {
      uint8_t((t.colorB.r + t.colorC.r) >> 1),
      uint8_t((t.colorB.g + t.colorC.g) >> 1),
      uint8_t((t.colorB.b + t.colorC.b) >> 1)
    };

    auto origC = t.pointC;
    auto origUvC = t.uvC;
    auto origColC = t.colorC;

    // reuse original: A, B, midBC
    t.pointC = midBC;
    t.uvC = {uvBC.u, uvBC.v, 0};
    t.setColorC(colBC);

    // new tri: A, midBC, C
    auto& t2 = balloc.allocateFragment<psyqo::Prim::GouraudTexturedTriangle>();
    t2.primitive.pointA = t.pointA;  t2.primitive.uvA = t.uvA;  t2.primitive.setColorA(t.getColorA());
    t2.primitive.pointB = midBC;     t2.primitive.uvB = uvBC;   t2.primitive.setColorB(colBC);
    t2.primitive.pointC = origC;     t2.primitive.uvC = origUvC; t2.primitive.setColorC(origColC);
    t2.primitive.tpage = t.tpage;
    t2.primitive.clutIndex = t.clutIndex;
    t2.primitive.setOpaque();

    SubdivideTexturedTri(tri, zIndex, ot, maxDepth - 1);
    SubdivideTexturedTri(&t2, zIndex, ot, maxDepth - 1);
  }
  else {
    // split CA

    psyqo::Vertex midCA = {
      int16_t((t.pointC.x + t.pointA.x) >> 1),
      int16_t((t.pointC.y + t.pointA.y) >> 1)
    };

    psyqo::PrimPieces::UVCoords uvCA = {
      uint8_t((t.uvC.u + t.uvA.u) >> 1),
      uint8_t((t.uvC.v + t.uvA.v) >> 1)
    };

    psyqo::Color colCA = {
      uint8_t((t.colorC.r + t.getColorA().r) >> 1),
      uint8_t((t.colorC.g + t.getColorA().g) >> 1),
      uint8_t((t.colorC.b + t.getColorA().b) >> 1)
    };

    auto origA = t.pointA;
    auto origUvA = t.uvA;
    auto origColA = t.getColorA();

    // reuse original: midCA, B, C
    t.pointA = midCA;
    t.uvA = uvCA;
    t.setColorA(colCA);

    // new tri: A, B, midCA
    auto& t2 = balloc.allocateFragment<psyqo::Prim::GouraudTexturedTriangle>();
    t2.primitive.pointA = origA;     t2.primitive.uvA = origUvA; t2.primitive.setColorA(origColA);
    t2.primitive.pointB = t.pointB;  t2.primitive.uvB = t.uvB;   t2.primitive.setColorB(t.colorB);
    t2.primitive.pointC = midCA;     t2.primitive.uvC = {uvCA.u, uvCA.v, 0};    t2.primitive.setColorC(colCA);
    t2.primitive.tpage = t.tpage;
    t2.primitive.clutIndex = t.clutIndex;
    t2.primitive.setOpaque();

    SubdivideTexturedTri(tri, zIndex, ot, maxDepth - 1);
    SubdivideTexturedTri(&t2, zIndex, ot, maxDepth - 1);
  }
}

psyqo::FixedPoint<> Renderer::GetFogFactor(uint32_t z) {
  if (z <= NEAR_FOG_DISTANCE) return 0.0_fp;
  if (z >= FULL_FOG_DISTANCE) return 1.0_fp;

  return ((z - NEAR_FOG_DISTANCE) * 1.0_fp) / (FULL_FOG_DISTANCE - NEAR_FOG_DISTANCE);
}

void Renderer::ApplyFogToColour(psyqo::Color* col, psyqo::FixedPoint<> fogFactor) {
  col->r = ((col->r * (1.0_fp - fogFactor)).value) >> 12;
  col->g = ((col->g * (1.0_fp - fogFactor)).value) >> 12;
  col->b = ((col->b * (1.0_fp - fogFactor)).value) >> 12;
}

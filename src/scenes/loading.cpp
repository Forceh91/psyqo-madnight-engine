#include "loading.hh"
#include "../animation/animation_manager.hh"
#include "../mesh/mesh_manager.hh"
#include "../render/renderer.hh"
#include "../sound/sound_manager.hh"
#include "../textures/texture_manager.hh"


#include "psyqo/fixed-point.hh"
#include "psyqo/xprintf.h"

void LoadingScene::start(StartReason reason) { Renderer::Instance().StartScene(); }

void LoadingScene::frame() {
  uint32_t deltaTime = Renderer::Instance().Process();
  if (deltaTime == 0)
    return;

  auto loaded = psyqo::FixedPoint<>(int32_t(m_loadFilesLoadedCount), int32_t(0));
  auto total = psyqo::FixedPoint<>(int32_t(m_loadFilesCount), int32_t(0));
  auto percent = loaded > 0 ? loaded / total * 100 : 0;
  Renderer::Instance().RenderLoadingScreen(percent.integer());
}

psyqo::Coroutine<> LoadingScene::LoadFiles(eastl::vector<LoadQueue> &&files, bool dumpExisting) {
  // most likely we want to do this, but this will dump everything we know
  // about meshes and textures, ready for a fresh scene
  if (dumpExisting) {
    MeshManager::Dump();
    TextureManager::Dump();
  }

  m_queue = eastl::move(files);
  m_loadFilesCount = m_queue.size();
  m_loadFilesLoadedCount = 0;

  for (auto &file : m_queue) {
    MeshBin *mesh = nullptr;
    TimFile *tim = nullptr;
    ModSoundFile *modSound = nullptr;
    if (file.type == LoadFileType::OBJECT)
      co_await MeshManager::LoadMeshFromCDROM(file.name.c_str(), &mesh);
    if (file.type == LoadFileType::TEXTURE)
      co_await TextureManager::LoadTIMFromCDRom(file.name.c_str(), file.x, file.y, file.clutX, file.clutY, &tim);
    if (file.type == LoadFileType::MOD_FILE)
      co_await SoundManager::LoadMODSoundFromCDRom(file.name.c_str(), &modSound);
    if (file.type == LoadFileType::ANIMATION)
      co_await AnimationManager::LoadAnimationFromCDRom(file.name.c_str());

    // total loaded files
    m_loadFilesLoadedCount++;
  }

  m_queue.clear();
}

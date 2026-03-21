#include "loading.hh"
#include "../animation/animation_manager.hh"
#include "../core/debug/perf_monitor.hh"
#include "../mesh/colbin_manager.hh"
#include "../mesh/mesh_manager.hh"
#include "../core/object/gameobject_manager.hh"
#include "../render/renderer.hh"
#include "../sound/sound_manager.hh"
#include "../textures/texture_manager.hh"

#include "psyqo/fixed-point.hh"

void LoadingScene::start(StartReason reason) { Renderer::Instance().StartScene(); }

void LoadingScene::frame() {
  uint32_t deltaTime = Renderer::Instance().Process();
	if (deltaTime == 0)
		return;

	auto loaded = psyqo::FixedPoint<>(int32_t(m_loadFilesLoadedCount), int32_t(0));
	auto total = psyqo::FixedPoint<>(int32_t(m_loadFilesCount), int32_t(0));
	auto percent = loaded > 0 ? loaded / total * 100 : 0;

	Renderer::Instance().RenderLoadingScreen(percent.integer());
	PerfMonitor::Render(deltaTime);
}

psyqo::Coroutine<> LoadingScene::LoadFiles(eastl::vector<LoadQueue> &&files, bool dumpExisting) {
	// most likely we want to do this, but this will dump everything we know
	// about meshes and textures, ready for a fresh scene
	if (dumpExisting) {
		GameObjectManager::Dump();
		MeshManager::Dump();
		TextureManager::Dump();
		ColbinManager::Dump();
	}

	m_queue = eastl::move(files);
	m_loadFilesCount = m_queue.size();
	m_loadFilesLoadedCount = 0;

	// load it backwards so we can erase as we go
	// for (int i = m_queue.size() - 1; i >= 0; i--) {
	for (auto const &file : m_queue) {
		switch (file.type) {
			case LoadFileType::OBJECT: {
				MeshBin *mesh = nullptr;
				co_await MeshManager::LoadMeshFromCDROM(file.name.c_str(), &mesh);
				break;
			}

			case LoadFileType::TEXTURE: {
				TimFile *tim = nullptr;
				co_await TextureManager::LoadTIMFromCDRom(file.name.c_str(), file.x, file.y, file.clutX, file.clutY, &tim);
				break;
			}

			case LoadFileType::MOD_FILE: {
				ModSoundFile *modSound = nullptr;
				co_await SoundManager::LoadMODSoundFromCDRom(file.name.c_str(), &modSound);
				break;
			}

			case LoadFileType::ANIMATION:
				co_await AnimationManager::LoadAnimationFromCDRom(file.name.c_str());
				break;

			case LoadFileType::COLBIN: {
				ColBin *colbin = nullptr;
				co_await ColbinManager::LoadColbin(file.name, &colbin);
				break;
			}
		}

		m_loadFilesLoadedCount++;
	}

	m_queue.clear();
}

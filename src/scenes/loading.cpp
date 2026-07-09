#include "loading.hh"
#include "../animation/animation_manager.hh"
#include "../core/debug/perf_monitor.hh"
#include "../mesh/colbin_manager.hh"
#include "../mesh/mesh_manager.hh"
#include "../core/object/gameobject_manager.hh"
#include "../render/renderer.hh"
#include "../sound/sound_manager.hh"
#include "../sound/mod_sound_manager.hh"
#include "../textures/texture_manager.hh"

#include "EASTL/vector.h"
#include "psyqo/fixed-point.hh"
#include "scene_loader.hh"

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
		SoundManager::Dump();
	}

	m_queue = eastl::move(files);
	m_loadFilesLoadedCount = 0;
	m_loadFilesCount = m_queue.size();

	if (!m_queue.size())
		co_return;

	// FIFO worklist - both the manifest (initial m_queue) and any nested scene's contents load in reverse order
	// nothing in here should depend on each other existing, or, if they do, then put them backwards in the manifest
	while (m_loadFilesLoadedCount != m_loadFilesCount) {
		if (m_loadFilesLoadedCount >= m_queue.size()) {
			printf("LOADER: index out of range, aborting.\n");
			break;
		}
		
		auto const &file = m_queue[m_loadFilesLoadedCount];

		switch (file.type) {
			case LoadFileType::OBJECT: {
				MeshBin *mesh = nullptr;
				co_await MeshManager::LoadMesh(file.name.c_str(), &mesh);
				break;
			}

			case LoadFileType::TEXTURE: {
				TimFile *tim = nullptr;
				co_await TextureManager::LoadTIM(file.name.c_str(), file.x, file.y, file.clutX, file.clutY, &tim);
				break;
			}

			case LoadFileType::MOD_FILE: {
				ModSoundFile *modSound = nullptr;
				co_await ModSoundManager::LoadMODSound(file.name.c_str(), &modSound);
				break;
			}

			case LoadFileType::ANIMATION:
				co_await AnimationManager::LoadAnimation(file.name.c_str());
				break;

			case LoadFileType::COLBIN: {
				ColBin *colbin = nullptr;
				co_await ColbinManager::LoadColbin(file.name, &colbin);
				break;
			}

			case LoadFileType::VAG: {
				VagEntry* vag = nullptr;
				co_await SoundManager::LoadVAGFile(file.name, &vag);
				break;
			}

			case LoadFileType::SCENE: {
				auto before = m_queue.size();
				co_await SceneLoader::LoadScene(file.name, m_queue);
				m_loadFilesCount += m_queue.size() - before;
				break;
			}
		}

		m_loadFilesLoadedCount++;
	}
}

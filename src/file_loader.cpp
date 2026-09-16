#include "file_loader.hh"
#include "animation/animation_manager.hh"
#include "core/object/gameobject_manager.hh"
#include "madnight.hh"
#include "mesh/colbin_manager.hh"
#include "mesh/mesh_manager.hh"
#include "scenes/scene_loader.hh"
#include "sound/mod_sound.hh"
#include "sound/sound_manager.hh"

#include <psyqo/xprintf.h>

eastl::vector<LoadQueue> FileLoader::m_queue;
uint16_t FileLoader::m_totalFiles = 0;
uint16_t FileLoader::m_loadedFiles = 0;

psyqo::Coroutine<> FileLoader::LoadFiles(eastl::vector<LoadQueue>&& files, bool clearPools) {
	if (clearPools) {
		g_madnightEngine.m_gameObjectManager.Dump();
		g_madnightEngine.m_meshManager.Dump();
		g_madnightEngine.m_textureManager.Dump();
		g_madnightEngine.m_colbinManager.Dump();
		g_madnightEngine.m_soundManager.Dump();
	}

	if (!files.size()) {
		co_return;
	}

	// copy the queue over to the class and begin FIFO work
	m_queue = eastl::move(files);
	m_totalFiles = m_queue.size();
	m_loadedFiles = 0;

	// FIFO worklist, both the initial m_queue and any nested scene's contents load in reverse order
	// nothing in here should depend on the other existing.
	while (m_loadedFiles != m_totalFiles) {
		if (m_loadedFiles >= m_totalFiles) {
			printf("[FILE_LOADER] Index is out of range, aborting.\n");
			break;
		}

		auto const& file = m_queue[m_loadedFiles];
		switch (file.type) {
		case OBJECT: {
			MeshBin* out = nullptr;
			co_await g_madnightEngine.m_meshManager.LoadMesh(file.name, &out);
			break;
		}

		case TEXTURE: {
			TimFile* out = nullptr;
			co_await g_madnightEngine.m_textureManager.LoadTIM(file.name, file.x, file.y, file.clutX, file.clutY, &out);
			break;
		}

		case MOD_FILE: {
			ModSoundFile* out = nullptr;
			co_await g_madnightEngine.m_modSoundManager.LoadMODSound(file.name, &out);
			break;
		}

		case ANIMATION: {
			co_await g_madnightEngine.m_animationManager.LoadAnimation(file.name);
			break;
		}

		case COLBIN: {
			ColBin* out = nullptr;
			co_await g_madnightEngine.m_colbinManager.LoadColbin(file.name, &out);
			break;
		}

		case VAG: {
			VagEntry* out = nullptr;
			co_await g_madnightEngine.m_soundManager.LoadVAGFile(file.name, &out);
			break;
		}

		case SCENE: {
			auto before = m_totalFiles;
			co_await SceneLoader::LoadScene(file.name, m_queue);
			m_totalFiles += m_queue.size() - before;
			break;
		}
		}

		m_loadedFiles++;
	}

	m_queue.set_capacity(0);
}

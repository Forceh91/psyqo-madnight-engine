#include "file_loader.hh"
#include "animation/animation_manager.hh"
#include "core/object/gameobject_manager.hh"
#include "mesh/colbin_manager.hh"
#include "mesh/mesh_manager.hh"
#include "scenes/scene_loader.hh"
#include "sound/mod_sound_manager.hh"
#include "sound/sound_manager.hh"

#include <psyqo/xprintf.h>

eastl::vector<LoadQueue> FileLoader::m_queue;
uint16_t FileLoader::m_totalFiles = 0;
uint16_t FileLoader::m_loadedFiles = 0;

psyqo::Coroutine<> FileLoader::LoadFiles(eastl::vector<LoadQueue>&& files, bool clearPools) {
    if (clearPools) {
        GameObjectManager::Dump();
        MeshManager::Dump();
        TextureManager::Dump();
        ColbinManager::Dump();
        SoundManager::Dump();
    }

    if (!files.size())
        co_return;
    
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
                co_await MeshManager::LoadMesh(file.name.c_str(), nullptr);
                break;
            }

            case TEXTURE: {
                co_await TextureManager::LoadTIM(file.name.c_str(), file.x, file.y, file.clutX, file.clutY, nullptr);
                break;
            }

            case MOD_FILE: {
                co_await ModSoundManager::LoadMODSound(file.name.c_str(), nullptr);
                break;
            }

            case ANIMATION: {
                co_await AnimationManager::LoadAnimation(file.name.c_str());
                break;
            }

            case COLBIN: {
                co_await ColbinManager::LoadColbin(file.name, nullptr);
                break;
            }

            case VAG: {
                co_await SoundManager::LoadVAGFile(file.name, nullptr);
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

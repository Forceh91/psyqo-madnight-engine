#pragma once

#include "helpers/load_queue.hh"
#include <psyqo/coroutine.hh>

class FileLoader final {
public:
    // will load the provided queue, `clearPools` will dump all existing game objects, textures,
    // meshes, colbins, and sfx, from memory. it will not check they are in-use before it does this.
    // this is a FIFO worklist and the queue will be loaded backwards.
    static psyqo::Coroutine<> LoadFiles(eastl::vector<LoadQueue>&& files, bool clearPools = true);
    // how many files in total *will* be loaded. this number will increase when a scene file is reached
    static uint16_t TotalFiles(void) { return m_totalFiles; }
    // how many files in total *have* been loaded.
    static uint16_t LoadedFiles(void) { return m_loadedFiles; }
private:
    // the file(s) that will be loaded
    static eastl::vector<LoadQueue> m_queue;
    // how many files in total will be loaded
    static uint16_t m_totalFiles;
    // how many have been loaded
    static uint16_t m_loadedFiles;
};

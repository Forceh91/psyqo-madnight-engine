#pragma once
#include "../helpers/archive.hh"
#include "../helpers/load_queue.hh"
#include "psyqo/coroutine.hh"
#include "EASTL/vector.h"

class SceneLoader final {
public:
    static psyqo::Coroutine<> LoadScene(const eastl::fixed_string<char, MAX_ARCHIVE_FILE_NAME_LEN>& sceneFile, eastl::vector<LoadQueue>* loadQueueOut);
private:
};

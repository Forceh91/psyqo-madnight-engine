#include "scene_loader.hh"
#include "EASTL/fixed_string.h"
#include "psyqo/xprintf.h"
#include <cstdint>

psyqo::Coroutine<> SceneLoader::LoadScene(const eastl::fixed_string<char, MAX_ARCHIVE_FILE_NAME_LEN> &sceneFile, eastl::vector<LoadQueue>** loadQueueOut) {
    if (loadQueueOut == nullptr) {
        printf("SCENE: Invalid out queue.\n");
        co_return;        
    }
    
    (*loadQueueOut)->clear();

    // load the file from the archive
    auto buffer = co_await ArchiveHelper::LoadFile(sceneFile.c_str());
    uint8_t* data = buffer.data();
    size_t size = buffer.size();

    if (!data || !size) {
        buffer.clear();
        printf("SCENE: Failed to load file or it has no file size.\n");
        co_return;
    }

    uint8_t* ptr = data;

    // magic check
    eastl::fixed_string<char, 8> magic(reinterpret_cast<char*>(ptr), 8);
    if (magic.compare("SCENEBIN")) {
        printf("SCENE: Header magic is invalid, aborting.\n");
        buffer.clear();

        co_return;
    }
    ptr += 8;

    // number of files
    uint8_t fileCount = 0;
    __builtin_memcpy(&fileCount, ptr, sizeof(uint8_t));
    ptr += sizeof(uint8_t);

    for (int i = 0; i < fileCount; i++) {
        // file type
        LoadFileType type;
        __builtin_memcpy(&type, ptr, sizeof(uint8_t));
        ptr += sizeof(uint8_t);

        // how long is the file name
        uint8_t nameLen = 0;
        __builtin_memcpy(&nameLen, ptr, sizeof(uint8_t));
        ptr += sizeof(uint8_t);
        
        if (nameLen > MAX_ARCHIVE_FILE_NAME_LEN) {
            printf("SCENE: Corrupt entry %d, name too long (%d).\n", i, nameLen);
            buffer.clear();
            co_return;
        }

        // file name
        eastl::fixed_string<char, MAX_ARCHIVE_FILE_NAME_LEN + 1> fileName(reinterpret_cast<char*>(ptr), nameLen);
        ptr += nameLen;

        // add this to the out queue
        (*loadQueueOut)->push_back({fileName.c_str(), type});
    }

    buffer.clear();
    printf("SCENE: Successfully added %d files to the load queue.\n", (*loadQueueOut)->size());
}

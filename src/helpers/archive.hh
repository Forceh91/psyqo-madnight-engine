#pragma once

#include "psyqo/coroutine.hh"
#include "psyqo/buffer.hh"
#ifdef PCDRV
#include "psyqo/cdrom-pcdrv.hh"
#endif
#include "psyqo-paths/archive-manager.hh"

constexpr uint8_t MAX_ARCHIVE_FILE_NAME_LEN = 255;

class ArchiveHelper final {
public:
    static void init();
    static psyqo::Coroutine<psyqo::Buffer<uint8_t>> LoadFile(const char* fileName);
private:
#ifdef PCDRV
    static psyqo::CDRomPCDrv m_cdrom;
#endif
    static psyqo::paths::ArchiveManager m_archiveManager;
    static bool m_archiveManagerInit;
    static char m_loadingFileName[MAX_ARCHIVE_FILE_NAME_LEN];
};

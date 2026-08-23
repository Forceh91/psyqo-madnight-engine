#pragma once

#include "psyqo/coroutine.hh"
#include "psyqo/buffer.hh"
#ifdef PCDRV
#include "psyqo/cdrom-pcdrv.hh"
#endif
#include "psyqo-paths/archive-manager.hh"
#include "common/util/djbhash.h"
#include <EASTL/string_view.h>

constexpr uint8_t MAX_ARCHIVE_FILE_NAME_LEN = 255;

// hashes a name the same way psyqo-paths hashes its own archive index, so pool entries can store
// and compare names as a uint64_t instead of a full eastl::fixed_string. this overload handles
// runtime strings (fixed_string, string_view, const char*).
static inline uint64_t HashName(eastl::string_view name) {
    return djb::hash<uint64_t>(name.data(), name.size());
}

// this overload handles string literals, and folds to a constant at compile time.
template <unsigned S>
static inline constexpr uint64_t HashName(const char (&name)[S]) {
    return djb::hash<uint64_t>(name);
}

class ArchiveHelper final {
public:
    static void init(eastl::function<void()> cb);
    static psyqo::Coroutine<psyqo::Buffer<uint8_t>> LoadFile(const char* fileName);
private:
#ifdef PCDRV
    static psyqo::CDRomPCDrv m_cdrom;
#endif
    static psyqo::paths::ArchiveManager m_archiveManager;
    static bool m_archiveManagerInit;
    static char m_loadingFileName[MAX_ARCHIVE_FILE_NAME_LEN];
};

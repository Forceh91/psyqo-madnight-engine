/*
 * Copyright (C) 2025-2026 Matt Hadden / Madnight Games
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include "common/util/djbhash.h"

#include <cstddef>
#include <psyqo/buffer.hh>
#include <psyqo/coroutine.hh>
#ifdef PCDRV
#include <psyqo/cdrom-pcdrv.hh>
#endif
#include <EASTL/string_view.h>
#include <psyqo-paths/archive-manager.hh>

extern const char* g_gameIsoName;

constexpr uint8_t MAX_ARCHIVE_FILE_NAME_LEN = 255;

// hashes a name the same way psyqo-paths hashes its own archive index, so pool entries can store
// and compare names as a uint64_t instead of a full eastl::fixed_string. this overload handles
// runtime strings (fixed_string, string_view, const char*).
static inline uint64_t HashName(eastl::string_view name) { return djb::hash<uint64_t>(name.data(), name.size()); }

// this overload handles string literals, and folds to a constant at compile time.
template <unsigned S> static inline constexpr uint64_t HashName(const char (&name)[S]) {
	return djb::hash<uint64_t>(name);
}

class ArchiveHelper final {
  public:
	void init(eastl::function<void()> cb);
	psyqo::Coroutine<psyqo::Buffer<uint8_t>> LoadFile(const eastl::string_view& fileName);

  private:
#ifdef PCDRV
	psyqo::CDRomPCDrv m_cdrom = psyqo::CDRomPCDrv(g_gameIsoName);
#endif
	psyqo::paths::ArchiveManager m_archiveManager;
	bool m_archiveManagerInit = false;
	char m_loadingFileName[MAX_ARCHIVE_FILE_NAME_LEN];
};

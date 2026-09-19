/*
 * Copyright (C) 2025-2026 Matt Hadden / Madnight Games
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <EASTL/string_view.h>
#include <psyqo/buffer.hh>
#include <psyqo/coroutine.hh>

#ifndef PCDRV
#include "psyqo-paths/cdrom-loader.hh"
#include "psyqo/cdrom-device.hh"
#include "psyqo/iso9660-parser.hh"
#endif

[[deprecated("Replaced with `ArchiveHelper`")]];
class CDRomHelper final {
  public:
	void init(eastl::function<void()> cb);
	psyqo::Coroutine<psyqo::Buffer<uint8_t>> LoadFile(const eastl::string_view& fileName);

#ifndef PCDRV
	psyqo::CDRomDevice& CDRomDevice() { return m_cdrom; }

  private:
	psyqo::CDRomDevice m_cdrom;
	psyqo::ISO9660Parser m_isoParser = psyqo::ISO9660Parser(&m_cdrom);
	psyqo::paths::CDRomLoader m_cdromLoader;
	char m_loadingFileName[32];
	void get_iso_file_name(const char* file_name, char* iso_filename);
#endif
};

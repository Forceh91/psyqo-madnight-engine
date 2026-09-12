#pragma once

#include <EASTL/string_view.h>
#include <psyqo/buffer.hh>
#include <psyqo/coroutine.hh>

#ifndef PCDRV
#include "psyqo-paths/cdrom-loader.hh"
#include "psyqo/cdrom-device.hh"
#include "psyqo/iso9660-parser.hh"
#endif

class CDRomHelper final {
  public:
	static void init(eastl::function<void()> cb);
	static psyqo::Coroutine<psyqo::Buffer<uint8_t>> LoadFile(const eastl::string_view& fileName);

#ifndef PCDRV
	static psyqo::CDRomDevice& CDRomDevice() { return m_cdrom; }

  private:
	static psyqo::CDRomDevice m_cdrom;
	static psyqo::ISO9660Parser m_isoParser;
	static psyqo::paths::CDRomLoader m_cdromLoader;
	static char m_loadingFileName[32];
	static void get_iso_file_name(const char* file_name, char* iso_filename);
#endif
};

#pragma once

#include <stdint.h>
#include "psyqo/coroutine.hh"
#include "psyqo/buffer.hh"

#ifndef PCDRV
#include "psyqo/cdrom-device.hh"
#include "psyqo/iso9660-parser.hh"
#include "psyqo-paths/cdrom-loader.hh"
#include "file_defs.hh"
#endif

class CDRomHelper final
{
public:
    static void init();
    static psyqo::Coroutine<psyqo::Buffer<uint8_t>> LoadFile(const char *fileName);

private:
#ifndef PCDRV
    static psyqo::CDRomDevice m_cdrom;
    static psyqo::ISO9660Parser m_isoParser;
    static psyqo::paths::CDRomLoader m_cdromLoader;
    static char m_loadingFileName[MAX_CDROM_FILE_NAME_LEN];
    static void get_iso_file_name(const char *file_name, char *iso_filename);
#endif
};

#ifndef _CDROM_H
#define _CDROM_H

#include "psyqo/cdrom-device.hh"
#include "psyqo/coroutine.hh"
#include "psyqo/iso9660-parser.hh"
#include "psyqo-paths/cdrom-loader.hh"
#include "file_defs.hh"

class CDRomHelper final
{
    static psyqo::CDRomDevice m_cdrom;
    static psyqo::ISO9660Parser m_isoParser;
    static psyqo::paths::CDRomLoader m_cdromLoader;
    static char m_loadingFileName[MAX_CDROM_FILE_NAME_LEN];

public:
    static void init();
    static psyqo::Coroutine<psyqo::Buffer<uint8_t>> LoadFile(const char *fileName);
    static void get_iso_file_name(const char *file_name, char *iso_filename);
};

#endif

#ifndef _CDROM_H
#define _CDROM_H

#include "psyqo/cdrom-device.hh"
#include "psyqo/iso9660-parser.hh"
#include "psyqo/xprintf.h"
#include "psyqo-paths/cdrom-loader.hh"
#include "file_defs.hh"

class CDRomHelper final
{
    static psyqo::CDRomDevice m_cdrom;
    static psyqo::ISO9660Parser m_isoParser;
    static psyqo::paths::CDRomLoader m_cdromLoader;

public:
    static void init();
    static void load_file(const char *file_name, eastl::function<void(void *, size_t)> callback);
    static void get_iso_file_name(const char *file_name, char *iso_filename);
};

#endif

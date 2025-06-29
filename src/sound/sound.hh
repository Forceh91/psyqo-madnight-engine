#ifndef _SOUND_H
#define _SOUND_H

#include <EASTL/fixed_string.h>
#include "../helpers/file_defs.hh"

extern "C"
{
#include "modplayer/modplayer.h"
}

struct ModSoundFile
{
    eastl::fixed_string<char, MAX_CDROM_FILE_NAME_LEN> name;
    uint32_t size;
    bool isLoaded;
};

#endif

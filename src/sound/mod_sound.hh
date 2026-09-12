#pragma once

#include <EASTL/fixed_string.h>
#include "../helpers/archive.hh"

extern "C"
{
#include "modplayer/modplayer.h"
}

struct ModSoundFile
{
    eastl::fixed_string<char, MAX_ARCHIVE_FILE_NAME_LEN> name;
    uint32_t size;
    bool isLoaded;
};

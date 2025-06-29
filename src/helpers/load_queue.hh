#ifndef _LOAD_QUEUE_H
#define _LOAD_QUEUE_H

#include "EASTL/fixed_string.h"
#include "file_defs.hh"

enum LoadFileType
{
    OBJECT,
    TEXTURE,
    MOD_FILE
};

typedef struct _LOAD_QUEUE
{
    eastl::fixed_string<char, MAX_CDROM_FILE_NAME_LEN> name;
    LoadFileType type;
    // used for textures (type == TEXTURE)
    union
    {
        struct
        {
            uint16_t x, y, clutX, clutY;
        };
    };
} LoadQueue;

#endif

#ifndef _LOAD_QUEUE_H
#define _LOAD_QUEUE_H

#include "EASTL/fixed_string.h"
#include "file_defs.hh"
#include "archive.hh"

enum LoadFileType { OBJECT, TEXTURE, MOD_FILE, ANIMATION, COLBIN, VAG, SCENE = 255 };

typedef struct _LOAD_QUEUE {
  eastl::fixed_string<char, MAX_ARCHIVE_FILE_NAME_LEN> name;
  LoadFileType type;
  // used for textures (type == TEXTURE)
  union {
    struct {
      uint16_t x, y, clutX, clutY;
    };
  };
} LoadQueue;

#endif

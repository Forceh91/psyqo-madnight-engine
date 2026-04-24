#ifndef _FILE_DEFS_H
#define _FILE_DEFS_H

#include <cstdint>

/*
* All file names are case-insensitive and must
* be in 8.3 format, i.e. no more than 8 characters for the name and 3
* for the optional extension.
* We also need an extra 3 for the version (`;1`)
* Adding on an extra bit to hold the directory name
*/
static constexpr uint8_t MAX_CDROM_FILE_NAME_LEN = 32;

#endif

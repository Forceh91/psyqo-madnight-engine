#ifndef _CDROM_H
#define _CDROM_H

#include <stddef.h>

size_t cdrom_load_file(const char *file_name, void **buffer);

#endif

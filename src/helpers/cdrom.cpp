#include <string.h>
#include "psyqo/alloc.h"
#include "psyqo/cdrom-device.hh"
#include "psyqo/xprintf.h"
#include "psyqo-paths/cdrom-loader.hh"

psyqo::CDRomDevice m_cdrom;
psyqo::ISO9660Parser m_isoParser = psyqo::ISO9660Parser(&m_cdrom);
psyqo::paths::CDRomLoader m_cdromLoader;

// reads a file off of the cd rom into memory (most likely a file full of binary data)
size_t cdrom_load_file(const char *file_name, void **buffer)
{
    // modify the given file name into the correct format for the ps1 to read it
    char ps1_safe_file_name[16]; // 8 + 3 + 3 extra
    sprintf(ps1_safe_file_name, "\\%s;1", file_name);

    // read the file off of the disc
    psyqo::Buffer<uint8_t> local_buffer;
    m_cdromLoader.readFile(ps1_safe_file_name, m_isoParser, [&](psyqo::Buffer<uint8_t> &&buffer)
                           { local_buffer = eastl::move(buffer); });

    // the file isnt there or its empty
    if (local_buffer.empty())
    {
        printf("CD ROM: File not found on CD or file is empty\n");
        return 0;
    }

    // allocate the ptr
    size_t size = local_buffer.size();
    void *_ptr = psyqo_malloc(size);
    if (!_ptr)
    {
        printf("CD ROM: Failed to malloc\n");
        return 0;
    }

    // return the data (dont forget to free it when you're done with it!)
    memcpy(_ptr, local_buffer.data(), size);
    *buffer = _ptr;

    // clear buffer
    local_buffer.clear();

    // return size
    return size;
}

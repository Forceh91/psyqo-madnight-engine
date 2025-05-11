#include <string.h>
#include "psyqo/alloc.h"
#include "psyqo/xprintf.h"
#include "cdrom.hh"

psyqo::CDRomDevice CDRomHelper::m_cdrom;
psyqo::ISO9660Parser CDRomHelper::m_isoParser = psyqo::ISO9660Parser(&m_cdrom);
psyqo::paths::CDRomLoader CDRomHelper::m_cdromLoader;

void CDRomHelper::init()
{
    m_cdrom.prepare();
}

// reads a file off of the cd rom into memory (most likely a file full of binary data)
// dont forget to free the returned data when you're done with it
void CDRomHelper::load_file(const char *file_name, eastl::function<void(void *, size_t)> callback)
{
    printf("CD ROM: Attempting to read %s...\n", file_name);

    // read the file off of the disc
    m_cdromLoader.readFile("MODELS/CUBE.MB;1", m_isoParser,
                           [file_name, callback](psyqo::Buffer<uint8_t> &&buffer)
                           {
                               printf("CD ROM: Finished reading file from cd rom.\n");
                               psyqo::Buffer<uint8_t> local_buffer = eastl::move(buffer);

                               // the file isnt there or its empty
                               if (local_buffer.empty())
                               {
                                   printf("CD ROM: File %s not found on CD or file is empty\n", file_name);
                                   return;
                               }

                               // allocate the ptr
                               size_t size = local_buffer.size();
                               void *ptr = psyqo_malloc(size);
                               if (!ptr)
                               {
                                   printf("CD ROM: Failed to malloc\n");
                                   return;
                               }

                               // return the data (dont forget to free it when you're done with it!)
                               memcpy(ptr, local_buffer.data(), size);

                               // callback to the loader
                               callback(ptr, local_buffer.size());

                               // clear the local buffer
                               local_buffer.clear();
                           });
}

void CDRomHelper::get_iso_file_name(const char *file_name, char *iso_filename)
{
    // modify the given file name into the correct format for the ps1 to read it
    snprintf(iso_filename, MAX_FILE_NAME_LEN, "%s;1", file_name);
}

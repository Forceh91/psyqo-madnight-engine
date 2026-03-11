#include "colbin_manager.hh"
#include "../helpers/cdrom.hh"
#include "psyqo/alloc.h"
#include "psyqo/coroutine.hh"
#include "psyqo/xprintf.h"
#include <cstdint>

ColBin ColbinManager::m_colbin  = {"", 0, 0, 0, nullptr, nullptr};

psyqo::Coroutine<> ColbinManager::LoadColbin(const eastl::fixed_string<char, MAX_CDROM_FILE_NAME_LEN> &name, ColBin **colbinOut) {
    // just incase something goes wrong
    *colbinOut = nullptr;

    auto buffer = co_await CDRomHelper::LoadFile(name.c_str());
    void *data = buffer.data();
    size_t size = buffer.size();

    if (data == nullptr || size == 0) {
        buffer.clear();
        printf("COLBIN: Failed to load colbin or it has no file size.\n");
        co_return;
    }

    // prepare the struct
    __builtin_memset(&m_colbin, 0, sizeof(ColBin));

    // grab the ptr and start using it
    uint8_t *ptr = (uint8_t*)data;

    // header
    m_colbin.header.magic.assign(reinterpret_cast<char*>(ptr), 6);
    ptr += 6;

    if (m_colbin.header.magic.compare("COLBIN") != 0) {
        printf("COLBIN: Header is invalid, aborting.\n");
        buffer.clear();
        co_return;
    }

    // version + counts
    __builtin_memcpy(&m_colbin.header.version, ptr++, 1);
    __builtin_memcpy(&m_colbin.header.floorTriCount, ptr, sizeof(m_colbin.header.floorTriCount));
    ptr += sizeof(m_colbin.header.floorTriCount);

    __builtin_memcpy(&m_colbin.header.wallOBBCount, ptr, sizeof(m_colbin.header.wallOBBCount));
    ptr += sizeof(m_colbin.header.wallOBBCount);

    // handle floor tris
    size_t floorTrisSize = sizeof(FloorTri) * m_colbin.header.floorTriCount;
    m_colbin.floors = (FloorTri*)psyqo_malloc(floorTrisSize);

    for (int i = 0; i < m_colbin.header.floorTriCount; i++) {
        // first vertex
        __builtin_memcpy(&m_colbin.floors[i].v0.x.value, ptr, sizeof(int32_t));
        ptr += sizeof(int32_t);

        __builtin_memcpy(&m_colbin.floors[i].v0.y.value, ptr, sizeof(int32_t));
        ptr += sizeof(int32_t);
        
        __builtin_memcpy(&m_colbin.floors[i].v0.z.value, ptr, sizeof(int32_t));
        ptr += sizeof(int32_t);

        // second vertex
        __builtin_memcpy(&m_colbin.floors[i].v1.x.value, ptr, sizeof(int32_t));
        ptr += sizeof(int32_t);

        __builtin_memcpy(&m_colbin.floors[i].v1.y.value, ptr, sizeof(int32_t));
        ptr += sizeof(int32_t);
        
        __builtin_memcpy(&m_colbin.floors[i].v1.z.value, ptr, sizeof(int32_t));
        ptr += sizeof(int32_t);

        // third vertex
        __builtin_memcpy(&m_colbin.floors[i].v2.x.value, ptr, sizeof(int32_t));
        ptr += sizeof(int32_t);

        __builtin_memcpy(&m_colbin.floors[i].v2.y.value, ptr, sizeof(int32_t));
        ptr += sizeof(int32_t);
        
        __builtin_memcpy(&m_colbin.floors[i].v2.z.value, ptr, sizeof(int32_t));
        ptr += sizeof(int32_t);
        
        // normals
        __builtin_memcpy(&m_colbin.floors[i].n[0], ptr, sizeof(int16_t));
        ptr += sizeof(int16_t);

        __builtin_memcpy(&m_colbin.floors[i].n[1], ptr, sizeof(int16_t));
        ptr += sizeof(int16_t);
        
        __builtin_memcpy(&m_colbin.floors[i].n[2], ptr, sizeof(int16_t));
        ptr += sizeof(int16_t);

        // skip over the padding
        ptr += 2;
    }

    // now we can move onto the OBB walls
    size_t wallOBBSize = sizeof(WallOBB) * m_colbin.header.wallOBBCount;
    m_colbin.walls = (WallOBB*)psyqo_malloc(wallOBBSize);

    for (int i = 0; i < m_colbin.header.wallOBBCount; i++) {
        // centres
        __builtin_memcpy(&m_colbin.walls[i].center.x.value, ptr, sizeof(int32_t));
        ptr += sizeof(int32_t);

        __builtin_memcpy(&m_colbin.walls[i].center.y.value, ptr, sizeof(int32_t));
        ptr += sizeof(int32_t);
        
        __builtin_memcpy(&m_colbin.walls[i].center.z.value, ptr, sizeof(int32_t));
        ptr += sizeof(int32_t);
    

        // axes
        for (int j = 0; j < 3; j++) {
            __builtin_memcpy(&m_colbin.walls[i].axes[j].x.value, ptr, sizeof(int16_t));
            ptr += sizeof(int16_t);

            __builtin_memcpy(&m_colbin.walls[i].axes[j].y.value, ptr, sizeof(int16_t));
            ptr += sizeof(int16_t);
            
            __builtin_memcpy(&m_colbin.walls[i].axes[j].z.value, ptr, sizeof(int16_t));
            ptr += sizeof(int16_t);

            // skip over padding
            ptr += 2;            
        }

        // half extents
        __builtin_memcpy(&m_colbin.walls[i].halfExtents.x.value, ptr, sizeof(int32_t));
        ptr += sizeof(int32_t);

        __builtin_memcpy(&m_colbin.walls[i].halfExtents.y.value, ptr, sizeof(int32_t));
        ptr += sizeof(int32_t);
        
        __builtin_memcpy(&m_colbin.walls[i].halfExtents.z.value, ptr, sizeof(int32_t));
        ptr += sizeof(int32_t);

        // flags
        __builtin_memcpy(&m_colbin.walls[i].flags, ptr, sizeof(int32_t));
        ptr += sizeof(int32_t);
    }

    buffer.clear();
    *colbinOut = &m_colbin;
    printf("COLBIN: Successfully loaded COLBIN of %d bytes into memory.\n", size);
}

// dump the colbin in memory and start fresh
// this is used when switching to a loading screen for instance.
// this is a dangerous function as it wont check if anything is used
void ColbinManager::Dump(void) {
    psyqo_free(m_colbin.floors);
    psyqo_free(m_colbin.walls);
    m_colbin = {"", 0, 0, 0, nullptr, nullptr};
}


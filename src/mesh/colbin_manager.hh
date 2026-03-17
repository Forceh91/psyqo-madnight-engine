#ifndef _COLBIN_MANAGER_HH
#define _COLBIN_MANAGER_HH

#include "../helpers/file_defs.hh"
#include "../core/collision_types.hh"
#include "EASTL/fixed_string.h"
#include "EASTL/span.h"
#include "psyqo/coroutine.hh"
#include "psyqo/vector.hh"

struct Header {
    eastl::fixed_string<char, 6> magic; // COLBIN
    uint8_t version; // 2
    uint32_t floorTriCount;
    uint32_t wallOBBCount;
};

struct GridHeader {
    int32_t originX;
    int32_t originZ;
    uint32_t cellSize;
    uint16_t gridWidth;
    uint16_t gridHeight;
};

struct GridCell {
    uint16_t count;
    uint16_t *indices;
};

struct FloorTri {
    psyqo::Vec3 v0;    // vertex 0 (x, y, z) — scaled by 128
    psyqo::Vec3 v1;    // vertex 1 (x, y, z) — scaled by 128
    psyqo::Vec3 v2;    // vertex 2 (x, y, z) — scaled by 128
    int16_t n[3];     // face normal (x, y, z) — FP12 (scaled by 4096)
};

struct ColBin {   
    Header header;
    GridHeader gridHeader;
    GridCell* gridCells;
    FloorTri *floors;
    OBB *walls;
};

class ColbinManager {
public:
    static psyqo::Coroutine<> LoadColbin(const eastl::fixed_string<char, MAX_CDROM_FILE_NAME_LEN> &name, ColBin **colbinOut);
    static ColBin *Colbin(void) { return &m_colbin; }
    static void Dump(void);
    static eastl::span<OBB> walls(void) { return {m_colbin.walls, m_colbin.header.wallOBBCount}; };

private:
    static ColBin m_colbin;
};

#endif


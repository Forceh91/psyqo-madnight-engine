#ifndef _COLBIN_MANAGER_HH
#define _COLBIN_MANAGER_HH

#include "EASTL/fixed_string.h"
#include "../helpers/file_defs.hh"
#include "psyqo/coroutine.hh"
#include "psyqo/vector.hh"

struct Header {
    eastl::fixed_string<char, 6> magic; // COLBIN
    uint8_t version; // 1
    uint32_t floorTriCount;
    uint32_t wallOBBCount;
};

struct FloorTri {
    psyqo::Vec3 v0;    // vertex 0 (x, y, z) — scaled by 128
    psyqo::Vec3 v1;    // vertex 1 (x, y, z) — scaled by 128
    psyqo::Vec3 v2;    // vertex 2 (x, y, z) — scaled by 128
    int16_t n[3];     // face normal (x, y, z) — FP12 (scaled by 4096)
};

struct WallOBB {
    psyqo::Vec3 center;      // OBB center (x, y, z) — scaled by 128
    int16_t axes[3][3];     // 3 orthogonal axes — FP12 (scaled by 4096)
                            //   axes[0] = along width
                            //   axes[1] = face normal
                            //   axes[2] = along height
    psyqo::Vec3 halfExtents; // half-size along each axis — scaled by 128
                            // minimum thickness of 4 units on degenerate axis
    uint32_t flags;         // reserved (0)
};

struct ColBin {   
    Header header;
    FloorTri *floors;
    WallOBB *walls;
};

class ColbinManager {
public:
    static psyqo::Coroutine<> LoadColbin(const eastl::fixed_string<char, MAX_CDROM_FILE_NAME_LEN> &name, ColBin **colbinOut);
    const static ColBin *Colbin(void) { return &m_colbin; }
    static void Dump(void);

private:
    static ColBin m_colbin;
};

#endif


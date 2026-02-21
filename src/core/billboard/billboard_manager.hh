#ifndef _BILLBOARD_MANAGER_H
#define _BILLBOARD_MANAGER_H

#include "defs.hh"
#include "billboard.hh"

#include "EASTL/array.h"
#include "EASTL/fixed_string.h"
#include "EASTL/fixed_vector.h"
#include "psyqo/vector.hh"

class BillboardManager final {
public:
    static Billboard* CreateBillboard(const eastl::fixed_string<char, MAX_BILLBOARD_NAME_LENGTH> name, psyqo::Vec3 pos, psyqo::Vec2 size);
    static void DestroyBillboard(Billboard* billboard);

    static const eastl::fixed_vector<Billboard*, MAX_BILLBOARDS> &GetActiveBillboards(void);
    static const eastl::array<Billboard, MAX_BILLBOARDS> &GetBillboards(void) { return m_billboards; }
    static Billboard* GetBillboardByName(const eastl::fixed_string<char, MAX_BILLBOARD_NAME_LENGTH> name);

private:
    static eastl::array<Billboard, MAX_BILLBOARDS> m_billboards;
    static eastl::fixed_vector<Billboard*, MAX_BILLBOARDS> m_activeBillboards;
    
    static int8_t GetFreeIndex(void);
};

#endif

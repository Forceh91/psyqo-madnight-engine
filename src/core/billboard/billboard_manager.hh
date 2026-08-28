#pragma once

#include "../../pool/pool.hh"
#include "defs.hh"
#include "billboard.hh"

#include <EASTL/array.h>
#include <EASTL/fixed_string.h>
#include <EASTL/fixed_vector.h>
#include <psyqo/vector.hh>

class BillboardManager final {
public:
    static Billboard* CreateBillboard(const eastl::fixed_string<char, MAX_BILLBOARD_NAME_LENGTH>& name, psyqo::Vec3 pos, psyqo::Vec2 size);
    static void DestroyBillboard(Billboard* billboard);

    static const eastl::fixed_vector<Billboard*, MAX_BILLBOARDS>& GetActiveBillboards(void);
    static const Billboard* GetBillboards(void) { return m_pool.Entries(); }
    static Billboard* GetBillboardByName(const eastl::fixed_string<char, MAX_BILLBOARD_NAME_LENGTH>&name );
    static Billboard* GetBillboardByName(uint64_t nameHash);

private:
    static Pool<Billboard, MAX_BILLBOARDS> m_pool;
    static eastl::fixed_vector<Billboard*, MAX_BILLBOARDS> m_activeBillboards;
};

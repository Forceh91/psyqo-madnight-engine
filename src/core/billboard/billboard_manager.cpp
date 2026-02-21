#include "billboard_manager.hh"
#include "EASTL/array.h"
#include "EASTL/fixed_vector.h"
#include "billboard.hh"
#include "defs.hh"
#include <cstdint>

eastl::array<Billboard, MAX_BILLBOARDS> BillboardManager::m_billboards;
eastl::fixed_vector<Billboard*, MAX_BILLBOARDS> BillboardManager::m_activeBillboards;

Billboard *BillboardManager::CreateBillboard(const eastl::fixed_string<char, MAX_BILLBOARD_NAME_LENGTH> name, psyqo::Vec3 pos, psyqo::Vec2 size) {
    auto ix = GetFreeIndex();
    if (ix == -1)
        return nullptr;

    m_billboards[ix] = Billboard(name, pos, size, ix);
    return &m_billboards[ix];
}

int8_t BillboardManager::GetFreeIndex(void) {
    for (auto i = 0; i < MAX_BILLBOARDS; i++) {
        if (m_billboards.at(i).name().empty())
            return i;
    }

    return -1;
}

void BillboardManager::DestroyBillboard(Billboard *billboard) {
    if (billboard)
        billboard->Destroy();
}

const eastl::fixed_vector<Billboard*, MAX_BILLBOARDS> &BillboardManager::GetActiveBillboards(void) {
    m_activeBillboards.clear();

    for (auto &billboard : m_billboards) {
        if (billboard.id() != INVALID_BILLBOARD_ID)
            m_activeBillboards.push_back(&billboard);
    }

    return m_activeBillboards;
}

Billboard* BillboardManager::GetBillboardByName(const eastl::fixed_string<char, MAX_BILLBOARD_NAME_LENGTH> name) {
    for (auto i = 0; i < MAX_BILLBOARDS; i++) {
        if (m_billboards.at(i).name() == name)
            return &m_billboards.at(i);
    }

    return nullptr;
}

#include "billboard_manager.hh"
#include "billboard.hh"
#include "defs.hh"

#include <EASTL/fixed_vector.h>

Pool<Billboard, MAX_BILLBOARDS> BillboardManager::m_pool;
eastl::fixed_vector<Billboard*, MAX_BILLBOARDS> BillboardManager::m_activeBillboards;

Billboard *BillboardManager::CreateBillboard(const eastl::fixed_string<char, MAX_BILLBOARD_NAME_LENGTH> &name, psyqo::Vec3 pos, psyqo::Vec2 size) {
    auto id = m_pool.Acquire();
    if (id == INVALID_POOL_ID)
        return nullptr;

    auto* billboard = m_pool.Get(id);
    billboard->Init(name, pos, size, id);
    return billboard;
}

void BillboardManager::DestroyBillboard(Billboard *billboard) {
    if (!billboard)
        return;

    m_pool.Free(billboard->id());
    billboard->Destroy();
}

const eastl::fixed_vector<Billboard*, MAX_BILLBOARDS>& BillboardManager::GetActiveBillboards(void) {
    m_activeBillboards.clear();

    auto count = m_pool.size();
    for (auto i = 0; i < count; i++) {
        auto* billboard = m_pool.Get(i);
        if (billboard && billboard->id() != INVALID_POOL_ID)
            m_activeBillboards.push_back(billboard);
    }

    return m_activeBillboards;
}

Billboard* BillboardManager::GetBillboardByName(const eastl::fixed_string<char, MAX_BILLBOARD_NAME_LENGTH>& name) {
    return GetBillboardByName(HashName(name));
}

Billboard* BillboardManager::GetBillboardByName(uint64_t nameHash) {
    auto count = m_pool.size();
    for (auto i = 0; i < count; i++) {
        auto* billboard = m_pool.Get(i);
        if (billboard && billboard->id() != INVALID_POOL_ID && billboard->nameHash() == nameHash)
            return billboard;
    }

    return nullptr;
}

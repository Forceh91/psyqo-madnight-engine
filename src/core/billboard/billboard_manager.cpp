/*
 * Copyright (C) 2025-2026 Matt Hadden / Madnight Games
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "billboard_manager.hh"
#include "billboard.hh"
#include "defs.hh"

#include <EASTL/array.h>
#include <EASTL/fixed_vector.h>
#include <EASTL/string_view.h>

Billboard* BillboardManager::CreateBillboard(const eastl::string_view& name, psyqo::Vec3 pos, psyqo::Vec2 size) {
	auto ix = GetFreeIndex();
	if (ix == -1) {
		return nullptr;
	}

	m_billboards[ix] = Billboard(HashName(name), pos, size, ix);
	return &m_billboards[ix];
}

int16_t BillboardManager::GetFreeIndex(void) {
	for (auto i = 0; i < MAX_BILLBOARDS; i++) {
		if (m_billboards.at(i).id() == INVALID_BILLBOARD_ID) {
			return i;
		}
	}

	return -1;
}

void BillboardManager::DestroyBillboard(Billboard* billboard) {
	if (billboard) {
		billboard->Destroy();
	}
}

const eastl::fixed_vector<Billboard*, MAX_BILLBOARDS>& BillboardManager::GetActiveBillboards(void) {
	m_activeBillboards.clear();

	for (auto& billboard : m_billboards) {
		if (billboard.id() != INVALID_BILLBOARD_ID) {
			m_activeBillboards.push_back(&billboard);
		}
	}

	return m_activeBillboards;
}

Billboard* BillboardManager::GetBillboardByName(const eastl::string_view& name) {
	return GetBillboardByName(HashName(name));
}

constexpr Billboard* BillboardManager::GetBillboardByName(uint64_t nameHash) {
	for (auto i = 0; i < MAX_BILLBOARDS; i++) {
		if (m_billboards.at(i).id() != INVALID_BILLBOARD_ID && m_billboards.at(i).nameHash() == nameHash) {
			return &m_billboards.at(i);
		}
	}

	return nullptr;
}

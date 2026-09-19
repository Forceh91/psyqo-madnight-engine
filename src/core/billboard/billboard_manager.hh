/*
 * Copyright (C) 2025-2026 Matt Hadden / Madnight Games
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include "billboard.hh"
#include "defs.hh"
#include <EASTL/string_view.h>

#include <EASTL/array.h>
#include <EASTL/fixed_vector.h>
#include <psyqo/vector.hh>

class BillboardManager final {
  public:
	Billboard* CreateBillboard(const eastl::string_view& name, psyqo::Vec3 pos, psyqo::Vec2 size);
	void DestroyBillboard(Billboard* billboard);

	const eastl::fixed_vector<Billboard*, MAX_BILLBOARDS>& GetActiveBillboards(void);
	const constexpr Billboard* GetBillboards(void) const { return m_pool.Entries(); }
	Billboard* GetBillboardByName(const eastl::string_view& name);
	constexpr Billboard* GetBillboardByName(uint64_t nameHash);

  private:
	static Pool<Billboard, MAX_BILLBOARDS> m_pool;
	eastl::fixed_vector<Billboard*, MAX_BILLBOARDS> m_activeBillboards;
};

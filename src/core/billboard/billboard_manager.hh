#pragma once

#include "billboard.hh"
#include "defs.hh"
#include <EASTL/string_view.h>

#include <EASTL/array.h>
#include <EASTL/fixed_vector.h>
#include <psyqo/vector.hh>

class BillboardManager final {
  public:
	static Billboard* CreateBillboard(const eastl::string_view& name, psyqo::Vec3 pos, psyqo::Vec2 size);
	static void DestroyBillboard(Billboard* billboard);

	static const eastl::fixed_vector<Billboard*, MAX_BILLBOARDS>& GetActiveBillboards(void);
	static const eastl::array<Billboard, MAX_BILLBOARDS>& GetBillboards(void) { return m_billboards; }
	static Billboard* GetBillboardByName(const eastl::string_view& name);
	static Billboard* GetBillboardByName(uint64_t nameHash);

  private:
	static eastl::array<Billboard, MAX_BILLBOARDS> m_billboards;
	static eastl::fixed_vector<Billboard*, MAX_BILLBOARDS> m_activeBillboards;

	static int16_t GetFreeIndex(void);
};

/*
 * Copyright (C) 2025-2026 Matt Hadden / Madnight Games
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once
#include "psyqo/advancedpad.hh"
#include <EASTL/array.h>

struct MenuControllerBinds {
	psyqo::AdvancedPad::Event onEventType;
	psyqo::AdvancedPad::Button menuItemNext;
	psyqo::AdvancedPad::Button menuItemPrev;
	psyqo::AdvancedPad::Button menuItemConfirm;
	psyqo::AdvancedPad::Button menuItemBackCancel;
	eastl::array<psyqo::AdvancedPad::Button, 16> menuItemCustom;
};

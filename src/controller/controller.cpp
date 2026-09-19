/*
 * Copyright (C) 2025-2026 Matt Hadden / Madnight Games
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "controller.hh"
#include "../madnight.hh"
#include "psyqo/advancedpad.hh"

void ControllerHelper::init(void) {
	// try and force our controller into analog mode
}

int ControllerHelper::GetNormalizedAnalogStickInput(psyqo::AdvancedPad::Pad pad, uint8_t analog_index) {
	if (!g_madnightEngine.m_input.isPadConnected(pad) || !g_madnightEngine.m_input.hasAnalog(pad)) {
		return 0;
	}

	auto val = static_cast<int>(g_madnightEngine.m_input.getAdc(pad, analog_index) - 0x80);
	return analog_index == LeftStickY || analog_index == RightStickY
			   ? -val
			   : val; // normalize y axis so that a positive value is up on the stick
}

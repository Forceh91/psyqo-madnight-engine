/*
 * Copyright (C) 2025-2026 Matt Hadden / Madnight Games
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <psyqo/fixed-point.hh>
#include <psyqo/trigonometry.hh>

using namespace psyqo::trig_literals;

[[deprecated("Use `psyqo::atan2` instead. This is not reliable and drifts from expected values (by ~3.5 deg) or takes "
			 "the long path instead of short.")]]
psyqo::Angle atan2_fixed(int16_t y, int16_t x);
psyqo::Angle LerpAngle(const psyqo::Angle& a, const psyqo::Angle& b, const psyqo::FixedPoint<10>& t);
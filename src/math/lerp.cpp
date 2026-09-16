/*
 * Copyright (C) 2025-2026 Matt Hadden / Madnight Games
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "lerp.hh"
#include "psyqo/fixed-point.hh"

using namespace psyqo::fixed_point_literals;

psyqo::FixedPoint<> Lerp(const psyqo::FixedPoint<>& a, const psyqo::FixedPoint<>& b, const psyqo::FixedPoint<>& t) {
	return a * (1.0_fp - t) + b * t;
}

psyqo::FixedPoint<> inverseLerp(uint32_t a, uint32_t b, uint32_t value) {
	return (1.0_fp * value - 1.0_fp * a) / (1.0_fp * b - 1.0_fp * a);
}

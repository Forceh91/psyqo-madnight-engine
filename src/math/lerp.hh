#pragma once

#include "psyqo/fixed-point.hh"

using namespace psyqo::fixed_point_literals;

psyqo::FixedPoint<> Lerp(const psyqo::FixedPoint<>& a, const psyqo::FixedPoint<>& b, const psyqo::FixedPoint<>& t);
psyqo::FixedPoint<> inverseLerp(uint32_t a, uint32_t b, uint32_t value);

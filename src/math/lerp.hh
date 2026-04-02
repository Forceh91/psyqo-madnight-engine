#pragma once

#include "psyqo/fixed-point.hh"

using namespace psyqo::fixed_point_literals;

psyqo::FixedPoint<> Lerp(const psyqo::FixedPoint<>& a, const psyqo::FixedPoint<>& b, const psyqo::FixedPoint<>& t);

/*
 * Copyright (C) 2025-2026 Matt Hadden / Madnight Games
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once
#include "psyqo/fixed-point.hh"
#include "psyqo/vector.hh"

psyqo::FixedPoint<> DotProduct(const psyqo::Vec3& a, const psyqo::Vec3& b);
psyqo::Vec2 Lerp(const psyqo::Vec2& a, const psyqo::Vec2& b, const psyqo::FixedPoint<>& t);
psyqo::Vec3 Lerp(const psyqo::Vec3& a, const psyqo::Vec3& b, const psyqo::FixedPoint<>& t);
bool IsVector3Zero(const psyqo::Vec3& v);

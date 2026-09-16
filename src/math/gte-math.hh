/*
 * Copyright (C) 2025-2026 Matt Hadden / Madnight Games
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once
#include "psyqo/matrix.hh"
#include "psyqo/vector.hh"

class GTEMath final {
  public:
	static psyqo::Vec3 ProjectVectorOntoAxes(const psyqo::Matrix33& axisMatrix, const psyqo::Vec3& normalizedVec);
};

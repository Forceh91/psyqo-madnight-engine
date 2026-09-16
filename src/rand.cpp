/*
 * Copyright (C) 2025-2026 Matt Hadden / Madnight Games
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "rand.hh"

uint32_t Rand::rand() {
	m_seed = m_seed * 1664525 + 1013904223;
	return m_seed;
}

void Rand::seed(uint32_t seed) { m_seed = seed; }

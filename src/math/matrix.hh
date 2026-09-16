/*
 * Copyright (C) 2025-2026 Matt Hadden / Madnight Games
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include "psyqo/matrix.hh"

psyqo::Matrix33 TransposeMatrix33(const psyqo::Matrix33& rotationMatrix);
psyqo::Matrix33 InverseMatrix33(const psyqo::Matrix33& rotationMatrix);

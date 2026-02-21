#ifndef _MATH_MATRIX33_H
#define _MATH_MATRIX33_H

#include "psyqo/matrix.hh"

psyqo::Matrix33 TransposeMatrix33(const psyqo::Matrix33 &rotationMatrix);
psyqo::Matrix33 InverseMatrix33(const psyqo::Matrix33 &rotationMatrix);

#endif

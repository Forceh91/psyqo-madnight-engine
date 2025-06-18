#ifndef _GTE_MATH_VECTOR_H
#define _GTE_MATH_VECTOR_H

#include "psyqo/vector.hh"
#include "psyqo/matrix.hh"

class GTEMathVector final
{
public:
    static psyqo::Vec3 ProjectOntoAxes(const psyqo::Matrix33 &axisMatrix, const psyqo::Vec3 &normalizedVec);
};

#endif

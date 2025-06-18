#ifndef _GTE_MATH_H
#define _GTE_MATH_H

#include "psyqo/vector.hh"
#include "psyqo/matrix.hh"

class GTEMath final
{
public:
    static psyqo::Vec3 ProjectVectorOntoAxes(const psyqo::Matrix33 &axisMatrix, const psyqo::Vec3 &normalizedVec);
};

#endif

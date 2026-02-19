#ifndef _GTE_MATH_H
#define _GTE_MATH_H

#include "psyqo/matrix.hh"
#include "psyqo/vector.hh"

class GTEMath final {
public:
  static psyqo::Vec3 ProjectVectorOntoAxes(const psyqo::Matrix33 &axisMatrix, const psyqo::Vec3 &normalizedVec);
  static void MultiplyMatrix33(const psyqo::Matrix33 &rotationMatrixA, const psyqo::Matrix33 rotationMatrixB,
                               psyqo::Matrix33 *out);
  static void MultiplyMatrixVec3(const psyqo::Matrix33 &rotationMatrix, const psyqo::Vec3 posVector, psyqo::Vec3 *out);
};

#endif

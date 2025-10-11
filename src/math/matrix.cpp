#include "matrix.hh"

/*
 * The first row of A becomes the first column of \(A^{T}\).
 * The second row of A becomes the second column of \(A^{T}\).
 * The third row of A becomes the third column of \(A^{T}\).
 */

psyqo::Matrix33 TransposeMatrix33(const psyqo::Matrix33 &rotationMatrix) {
  return {{{rotationMatrix.vs[0].x, rotationMatrix.vs[1].x, rotationMatrix.vs[2].x},
           {rotationMatrix.vs[0].y, rotationMatrix.vs[1].y, rotationMatrix.vs[2].y},
           {rotationMatrix.vs[0].z, rotationMatrix.vs[1].z, rotationMatrix.vs[2].z}}};
}

#include "matrix.hh"
#include "psyqo/matrix.hh"
#include "psyqo/soft-math.hh"

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

psyqo::Matrix33 InverseMatrix33(const psyqo::Matrix33 &rotationMatrix) {
    auto abs = [](int32_t x) { return x < 0 ? -x : x; };

	auto det = psyqo::SoftMath::matrixDeterminant3(rotationMatrix);
	if (abs(det.value) < 50)
		return rotationMatrix;

	auto invDet = 1 / det;
	return {
		{
			{
				(rotationMatrix.vs[1].y * rotationMatrix.vs[2].z - rotationMatrix.vs[2].y * rotationMatrix.vs[1].z) * invDet,
				(rotationMatrix.vs[0].z * rotationMatrix.vs[2].y - rotationMatrix.vs[0].y * rotationMatrix.vs[2].z) * invDet,
				(rotationMatrix.vs[0].y * rotationMatrix.vs[1].z - rotationMatrix.vs[0].z * rotationMatrix.vs[1].y) * invDet,
			},
			{
				(rotationMatrix.vs[1].z * rotationMatrix.vs[2].x - rotationMatrix.vs[1].x * rotationMatrix.vs[2].z) * invDet,
				(rotationMatrix.vs[0].x * rotationMatrix.vs[2].z - rotationMatrix.vs[0].z * rotationMatrix.vs[2].x) * invDet,
				(rotationMatrix.vs[1].x * rotationMatrix.vs[0].z - rotationMatrix.vs[0].x * rotationMatrix.vs[1].z) * invDet,
			},
			{
				(rotationMatrix.vs[1].x * rotationMatrix.vs[2].y - rotationMatrix.vs[2].x * rotationMatrix.vs[1].y) * invDet,
				(rotationMatrix.vs[2].x * rotationMatrix.vs[0].y - rotationMatrix.vs[0].x * rotationMatrix.vs[2].y) * invDet,
				(rotationMatrix.vs[0].x * rotationMatrix.vs[1].y - rotationMatrix.vs[1].x * rotationMatrix.vs[0].y) * invDet,
			},			
	}};
}

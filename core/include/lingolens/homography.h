#pragma once

#include "types.h"
#include <array>

namespace lingolens {

/// 3x3 matrix for homography / perspective transforms.
/// Stored in row-major order: data[row * 3 + col].
struct Matrix3x3 {
    std::array<double, 9> data = {1, 0, 0, 0, 1, 0, 0, 0, 1}; // Identity

    double& operator()(int row, int col) { return data[row * 3 + col]; }
    double  operator()(int row, int col) const { return data[row * 3 + col]; }
};

class Homography {
public:
    /// Compute the 3x3 homography H such that H maps each corner of
    /// @p src to the corresponding corner of @p dst.
    static Matrix3x3 compute(const Quad& src, const Quad& dst);

    /// Apply homography H to a 2D point (projective divide included).
    static Point transformPoint(const Matrix3x3& H, const Point& p);

    /// Warp a rectangular source buffer into quad-shaped region on @p dst.
    /// Iterates over every pixel inside dstQuad's bounding box, inverse-
    /// maps into src coordinates, and copies the nearest pixel.
    static void warpInto(
        const FrameBuffer& src,
        FrameBuffer& dst,
        const Quad& dstQuad);

    /// Matrix multiplication: C = A * B.
    static Matrix3x3 multiply(const Matrix3x3& A, const Matrix3x3& B);

    /// Compute the inverse of a 3x3 matrix using the adjugate method.
    static Matrix3x3 inverse(const Matrix3x3& M);

private:
    /// Solve an 8x8 augmented linear system [A|b] via Gaussian elimination
    /// with partial pivoting.  @p A is 8 rows × 9 cols (augmented).
    static void solveLinearSystem(double A[8][9], double result[8]);

    /// Test whether a point lies inside a convex quadrilateral
    /// using the cross-product sign test.
    static bool pointInQuad(const Point& p, const Quad& q);
};

} // namespace lingolens

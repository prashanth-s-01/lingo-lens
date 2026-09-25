#include "../include/lingolens/homography.h"
#include <cmath>
#include <algorithm>
#include <stdexcept>

namespace lingolens {

// ─── Gaussian elimination with partial pivoting ──────────────────
// Solves the 8×9 augmented system [A|b] and writes the 8 unknowns
// into @p result.
void Homography::solveLinearSystem(double A[8][9], double result[8]) {
    constexpr int N = 8;

    for (int col = 0; col < N; ++col) {
        // Partial pivoting: find row with largest absolute value in column
        int maxRow = col;
        double maxVal = std::abs(A[col][col]);
        for (int row = col + 1; row < N; ++row) {
            double v = std::abs(A[row][col]);
            if (v > maxVal) { maxVal = v; maxRow = row; }
        }
        if (maxVal < 1e-12)
            throw std::runtime_error("Homography: singular matrix");

        // Swap rows
        if (maxRow != col) {
            for (int j = 0; j <= N; ++j)
                std::swap(A[col][j], A[maxRow][j]);
        }

        // Eliminate below
        for (int row = col + 1; row < N; ++row) {
            double factor = A[row][col] / A[col][col];
            for (int j = col; j <= N; ++j)
                A[row][j] -= factor * A[col][j];
        }
    }

    // Back-substitution
    for (int i = N - 1; i >= 0; --i) {
        result[i] = A[i][N];
        for (int j = i + 1; j < N; ++j)
            result[i] -= A[i][j] * result[j];
        result[i] /= A[i][i];
    }
}

// ─── Compute homography via Direct Linear Transform (DLT) ───────
// Sets up 8 equations from 4 point correspondences and solves for
// the 8 unknowns h0..h7 (h8 is normalised to 1).
//
// For each correspondence (x,y) → (x',y'):
//   x'(h6·x + h7·y + 1) = h0·x + h1·y + h2
//   y'(h6·x + h7·y + 1) = h3·x + h4·y + h5
//
// Which gives two linear equations per point.
Matrix3x3 Homography::compute(const Quad& src, const Quad& dst) {
    const Point sp[4] = {src.topLeft, src.topRight, src.bottomRight, src.bottomLeft};
    const Point dp[4] = {dst.topLeft, dst.topRight, dst.bottomRight, dst.bottomLeft};

    double A[8][9] = {};

    for (int i = 0; i < 4; ++i) {
        double x = sp[i].x, y = sp[i].y;
        double u = dp[i].x, v = dp[i].y;
        int r = i * 2;

        // Row for x':  h0·x + h1·y + h2 - h6·x·u - h7·y·u = u
        A[r][0] = x;  A[r][1] = y;  A[r][2] = 1;
        A[r][3] = 0;  A[r][4] = 0;  A[r][5] = 0;
        A[r][6] = -u * x;  A[r][7] = -u * y;  A[r][8] = u;

        // Row for y':  h3·x + h4·y + h5 - h6·x·v - h7·y·v = v
        A[r+1][0] = 0;  A[r+1][1] = 0;  A[r+1][2] = 0;
        A[r+1][3] = x;  A[r+1][4] = y;  A[r+1][5] = 1;
        A[r+1][6] = -v * x;  A[r+1][7] = -v * y;  A[r+1][8] = v;
    }

    double h[8];
    solveLinearSystem(A, h);

    Matrix3x3 H;
    H(0,0) = h[0];  H(0,1) = h[1];  H(0,2) = h[2];
    H(1,0) = h[3];  H(1,1) = h[4];  H(1,2) = h[5];
    H(2,0) = h[6];  H(2,1) = h[7];  H(2,2) = 1.0;
    return H;
}

// ─── Transform a 2D point through a homography ──────────────────
Point Homography::transformPoint(const Matrix3x3& H, const Point& p) {
    double w = H(2,0) * p.x + H(2,1) * p.y + H(2,2);
    if (std::abs(w) < 1e-12) w = 1e-12; // avoid division by zero
    return {
        (H(0,0) * p.x + H(0,1) * p.y + H(0,2)) / w,
        (H(1,0) * p.x + H(1,1) * p.y + H(1,2)) / w
    };
}

// ─── Matrix multiply ────────────────────────────────────────────
Matrix3x3 Homography::multiply(const Matrix3x3& A, const Matrix3x3& B) {
    Matrix3x3 C;
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) {
            double sum = 0;
            for (int k = 0; k < 3; ++k)
                sum += A(i,k) * B(k,j);
            C(i,j) = sum;
        }
    return C;
}

// ─── 3×3 matrix inverse via adjugate / determinant ──────────────
Matrix3x3 Homography::inverse(const Matrix3x3& M) {
    // Cofactors
    double c00 = M(1,1)*M(2,2) - M(1,2)*M(2,1);
    double c01 = M(1,2)*M(2,0) - M(1,0)*M(2,2);
    double c02 = M(1,0)*M(2,1) - M(1,1)*M(2,0);

    double det = M(0,0)*c00 + M(0,1)*c01 + M(0,2)*c02;
    if (std::abs(det) < 1e-12)
        throw std::runtime_error("Homography::inverse: singular matrix");

    double invDet = 1.0 / det;

    Matrix3x3 inv;
    inv(0,0) =  c00 * invDet;
    inv(0,1) = (M(0,2)*M(2,1) - M(0,1)*M(2,2)) * invDet;
    inv(0,2) = (M(0,1)*M(1,2) - M(0,2)*M(1,1)) * invDet;
    inv(1,0) =  c01 * invDet;
    inv(1,1) = (M(0,0)*M(2,2) - M(0,2)*M(2,0)) * invDet;
    inv(1,2) = (M(0,2)*M(1,0) - M(0,0)*M(1,2)) * invDet;
    inv(2,0) =  c02 * invDet;
    inv(2,1) = (M(0,1)*M(2,0) - M(0,0)*M(2,1)) * invDet;
    inv(2,2) = (M(0,0)*M(1,1) - M(0,1)*M(1,0)) * invDet;
    return inv;
}

// ─── Point-in-quad test (convex quad assumed) ────────────────────
// Uses cross-product signs: a point is inside a convex polygon iff
// the cross products at all edges have the same sign.
bool Homography::pointInQuad(const Point& p, const Quad& q) {
    const Point corners[4] = {q.topLeft, q.topRight, q.bottomRight, q.bottomLeft};

    auto cross = [](const Point& a, const Point& b, const Point& c) -> double {
        return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
    };

    bool allPos = true, allNeg = true;
    for (int i = 0; i < 4; ++i) {
        double cp = cross(corners[i], corners[(i+1) % 4], p);
        if (cp < 0) allPos = false;
        if (cp > 0) allNeg = false;
    }
    return allPos || allNeg;
}

// ─── Warp a flat source buffer into a perspective quad on dst ────
// For every pixel inside dstQuad's bounding box, we inverse-map
// back into src coordinates and copy the nearest pixel.  This is
// the standard "inverse warp" approach that avoids holes.
void Homography::warpInto(
    const FrameBuffer& src,
    FrameBuffer& dst,
    const Quad& dstQuad)
{
    // Build a homography from unit-square → dstQuad, invert it so we
    // can map dst pixels back to normalised src coordinates.
    Quad unitQuad;
    unitQuad.topLeft     = {0, 0};
    unitQuad.topRight    = {static_cast<double>(src.width), 0};
    unitQuad.bottomLeft  = {0, static_cast<double>(src.height)};
    unitQuad.bottomRight = {static_cast<double>(src.width), static_cast<double>(src.height)};

    Matrix3x3 H    = compute(unitQuad, dstQuad);
    Matrix3x3 Hinv = inverse(H);

    // Bounding box of dstQuad (clamped to frame)
    BoundingBox bbox(dstQuad);
    int x0 = std::max(0, static_cast<int>(bbox.topLeft.x));
    int y0 = std::max(0, static_cast<int>(bbox.topLeft.y));
    int x1 = std::min(dst.width  - 1, static_cast<int>(bbox.topLeft.x + bbox.width));
    int y1 = std::min(dst.height - 1, static_cast<int>(bbox.topLeft.y + bbox.height));

    for (int dy = y0; dy <= y1; ++dy) {
        for (int dx = x0; dx <= x1; ++dx) {
            // Quick reject pixels outside the quad
            Point dstPt{static_cast<double>(dx), static_cast<double>(dy)};
            if (!pointInQuad(dstPt, dstQuad)) continue;

            // Inverse-map to source coordinates
            Point srcPt = transformPoint(Hinv, dstPt);
            int sx = static_cast<int>(std::round(srcPt.x));
            int sy = static_cast<int>(std::round(srcPt.y));

            if (sx < 0 || sx >= src.width || sy < 0 || sy >= src.height)
                continue;

            int srcIdx = sy * src.stride + sx * 4;
            int dstIdx = dy * dst.stride + dx * 4;

            // Alpha-blend: only overwrite if the source pixel is opaque
            uint8_t srcAlpha = src.data[srcIdx + 3];
            if (srcAlpha == 0) continue;

            if (srcAlpha == 255) {
                dst.data[dstIdx]     = src.data[srcIdx];
                dst.data[dstIdx + 1] = src.data[srcIdx + 1];
                dst.data[dstIdx + 2] = src.data[srcIdx + 2];
                dst.data[dstIdx + 3] = 255;
            } else {
                // Proper alpha compositing
                double a = srcAlpha / 255.0;
                for (int c = 0; c < 3; ++c) {
                    double blended = src.data[srcIdx + c] * a +
                                     dst.data[dstIdx + c] * (1.0 - a);
                    dst.data[dstIdx + c] = static_cast<uint8_t>(
                        std::clamp(blended, 0.0, 255.0));
                }
                dst.data[dstIdx + 3] = 255;
            }
        }
    }
}

} // namespace lingolens

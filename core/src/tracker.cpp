#include "../include/lingolens/tracker.h"
#include <cmath>
#include <algorithm>

namespace lingolens {

Tracker::Tracker() = default;

// ─── RGBA → grayscale (BT.601 luminance weights) ────────────────
void Tracker::toGrayscale(const FrameBuffer& frame,
                          std::vector<uint8_t>& gray) {
    gray.resize(frame.width * frame.height);
    for (int y = 0; y < frame.height; ++y) {
        for (int x = 0; x < frame.width; ++x) {
            int idx = y * frame.stride + x * 4;
            uint8_t r = frame.data[idx];
            uint8_t g = frame.data[idx + 1];
            uint8_t b = frame.data[idx + 2];
            // BT.601:  Y = 0.299R + 0.587G + 0.114B
            gray[y * frame.width + x] = static_cast<uint8_t>(
                0.299 * r + 0.587 * g + 0.114 * b);
        }
    }
}

// ─── Lucas-Kanade single-point tracker ──────────────────────────
// Iterative inverse-compositional LK on a (2·wh+1)×(2·wh+1) window.
//
// For each pixel (px, py) in the window around prevPt:
//   Ix  = ∂I/∂x   (central difference in previous frame)
//   Iy  = ∂I/∂y
//   It  = I_curr(px+u, py+v) − I_prev(px, py)  (temporal gradient)
//
// We accumulate the 2×2 structure tensor G and the mismatch vector b:
//   G = Σ [ Ix² IxIy ]    b = Σ [ -Ix·It ]
//       [ IxIy Iy² ]          [ -Iy·It ]
//
// Then solve G · [u,v]ᵀ = b.

bool Tracker::trackPoint(
    const std::vector<uint8_t>& prevGray,
    const std::vector<uint8_t>& currGray,
    int w, int h,
    const Point& prevPt, Point& currPt,
    int windowHalf)
{
    double u = 0.0, v = 0.0;      // accumulated displacement
    constexpr int ITERATIONS = 5;

    for (int iter = 0; iter < ITERATIONS; ++iter) {
        double Gxx = 0, Gxy = 0, Gyy = 0;
        double bx  = 0, by  = 0;

        for (int dy = -windowHalf; dy <= windowHalf; ++dy) {
            for (int dx = -windowHalf; dx <= windowHalf; ++dx) {
                int px = static_cast<int>(prevPt.x) + dx;
                int py = static_cast<int>(prevPt.y) + dy;

                // Bounds check for gradient computation (need ±1 pixel)
                if (px < 1 || px >= w - 1 || py < 1 || py >= h - 1)
                    continue;

                // Spatial gradients in the previous frame (central difference)
                double Ix = 0.5 * (prevGray[py * w + px + 1] -
                                   prevGray[py * w + px - 1]);
                double Iy = 0.5 * (prevGray[(py + 1) * w + px] -
                                   prevGray[(py - 1) * w + px]);

                // Current position with accumulated displacement
                int cx = static_cast<int>(std::round(px + u));
                int cy = static_cast<int>(std::round(py + v));
                if (cx < 0 || cx >= w || cy < 0 || cy >= h)
                    continue;

                // Temporal gradient
                double It = static_cast<double>(currGray[cy * w + cx]) -
                            static_cast<double>(prevGray[py * w + px]);

                Gxx += Ix * Ix;
                Gxy += Ix * Iy;
                Gyy += Iy * Iy;
                bx  += -Ix * It;
                by  += -Iy * It;
            }
        }

        // Solve 2×2 system:  [Gxx Gxy] [du]   [bx]
        //                     [Gxy Gyy] [dv] = [by]
        double det = Gxx * Gyy - Gxy * Gxy;
        if (std::abs(det) < 1e-6) {
            // Ill-conditioned — cannot track this point
            return false;
        }

        double du = ( Gyy * bx - Gxy * by) / det;
        double dv = (-Gxy * bx + Gxx * by) / det;

        u += du;
        v += dv;

        // Converged?
        if (du * du + dv * dv < 0.01) break;
    }

    currPt.x = prevPt.x + u;
    currPt.y = prevPt.y + v;

    // Reject if the displacement is unreasonably large
    double disp = std::sqrt(u * u + v * v);
    return disp < 100.0;
}

// ─── setDetections ──────────────────────────────────────────────
void Tracker::setDetections(const std::vector<Quad>& detections) {
    m_tracked.clear();
    for (const auto& q : detections) {
        TrackedQuad tq;
        tq.id             = m_nextId++;
        tq.quad           = q;
        tq.confidence     = 1.0f;
        tq.framesTracked  = 0;
        m_tracked.push_back(tq);
    }
    m_framesSinceKeyframe = 0;
}

// ─── update ─────────────────────────────────────────────────────
// For each tracked quad, track all 4 corners via LK and update.
std::vector<TrackedQuad> Tracker::update(const FrameBuffer& currentFrame) {
    std::vector<uint8_t> currGray;
    toGrayscale(currentFrame, currGray);

    if (!m_prevGray.empty() &&
        m_prevWidth == currentFrame.width &&
        m_prevHeight == currentFrame.height)
    {
        std::vector<TrackedQuad> updated;

        for (auto& tq : m_tracked) {
            Point corners[4] = {
                tq.quad.topLeft,    tq.quad.topRight,
                tq.quad.bottomLeft, tq.quad.bottomRight
            };
            Point newCorners[4];
            bool allGood = true;

            for (int c = 0; c < 4; ++c) {
                if (!trackPoint(m_prevGray, currGray,
                                m_prevWidth, m_prevHeight,
                                corners[c], newCorners[c],
                                WINDOW_SIZE)) {
                    allGood = false;
                    break;
                }
            }

            if (allGood) {
                tq.quad.topLeft     = newCorners[0];
                tq.quad.topRight    = newCorners[1];
                tq.quad.bottomLeft  = newCorners[2];
                tq.quad.bottomRight = newCorners[3];
                tq.framesTracked++;
                // Confidence decays each frame (encourages keyframe refresh)
                tq.confidence *= 0.95f;
                if (tq.confidence >= MIN_CONFIDENCE)
                    updated.push_back(tq);
            }
            // else: drop this quad (tracking lost)
        }

        m_tracked = std::move(updated);
    }

    // Cache current frame for next update()
    m_prevGray   = std::move(currGray);
    m_prevWidth  = currentFrame.width;
    m_prevHeight = currentFrame.height;
    m_framesSinceKeyframe++;

    return m_tracked;
}

const std::vector<TrackedQuad>& Tracker::tracked() const {
    return m_tracked;
}

bool Tracker::needsKeyframe() const {
    return m_tracked.empty() ||
           m_framesSinceKeyframe >= KEYFRAME_INTERVAL;
}

} // namespace lingolens

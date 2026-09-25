#pragma once

#include "types.h"
#include <vector>
#include <cstdint>

namespace lingolens {

/// A quad that is being tracked across frames, with an ID and confidence.
struct TrackedQuad {
    int   id;              ///< Unique identifier for this tracked region
    Quad  quad;            ///< Current estimated position
    float confidence;      ///< Tracking quality (0..1, drops each frame)
    int   framesTracked;   ///< How many consecutive frames this has been tracked
};

/// Simplified Lucas-Kanade optical-flow tracker.
///
/// Tracks the four corners of each detected quad from frame to frame so
/// the heavy OCR path only needs to run on keyframes.
class Tracker {
public:
    Tracker();

    /// Feed fresh OCR detections from a keyframe.  Replaces all tracked quads.
    void setDetections(const std::vector<Quad>& detections);

    /// Track existing quads from the previous frame into @p currentFrame.
    /// Returns the updated list; quads whose confidence drops below the
    /// threshold are removed.
    std::vector<TrackedQuad> update(const FrameBuffer& currentFrame);

    /// Current set of tracked quads (read-only).
    const std::vector<TrackedQuad>& tracked() const;

    /// True when it is time to run OCR again (every KEYFRAME_INTERVAL frames
    /// or when no quads are being tracked).
    bool needsKeyframe() const;

private:
    std::vector<TrackedQuad> m_tracked;
    std::vector<uint8_t>     m_prevGray;
    int m_prevWidth  = 0;
    int m_prevHeight = 0;
    int m_framesSinceKeyframe = 0;
    int m_nextId = 0;

    static constexpr int   KEYFRAME_INTERVAL = 15;
    static constexpr int   WINDOW_SIZE       = 15;   ///< LK window half-size
    static constexpr float MIN_CONFIDENCE    = 0.3f;

    /// RGBA → single-channel grayscale (luminance).
    static void toGrayscale(const FrameBuffer& frame,
                            std::vector<uint8_t>& gray);

    /// Track a single point between two grayscale images using the
    /// iterative Lucas-Kanade method.  Returns false if the point
    /// could not be reliably tracked.
    static bool trackPoint(
        const std::vector<uint8_t>& prevGray,
        const std::vector<uint8_t>& currGray,
        int w, int h,
        const Point& prevPt, Point& currPt,
        int windowHalf);
};

} // namespace lingolens

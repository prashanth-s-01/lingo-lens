#pragma once

#include "types.h"

namespace lingolens {

class StyleAnalyzer {
public:
    // Extracts foreground (text) and background colors from a region
    // using K-means clustering with K=2.
    //
    // Parameters:
    //   frame  - the full camera frame
    //   region - the quad where text was detected
    //
    // Returns:
    //   StyleInfo with foreground and background colors filled in
    StyleInfo analyze(const FrameBuffer& frame, const Quad& region) const;

private:
    // Computes squared Euclidean distance between two colors.
    // We skip sqrt as an optimization — only relative comparison matters.
    static double colorDistanceSq(const Color& a, const Color& b);

    // Runs K-means clustering (K=2) on a set of pixel colors.
    // Separates pixels into two groups and returns their average colors.
    //
    // Parameters:
    //   pixels     - vector of pixel colors from the region
    //   out_colorA - average color of cluster A (output)
    //   out_colorB - average color of cluster B (output)
    //   out_countA - number of pixels in cluster A (output)
    //   out_countB - number of pixels in cluster B (output)
    //   maxIters   - maximum iterations before stopping (default: 10)
    static void kMeansTwo(
        const std::vector<Color>& pixels,
        Color& out_colorA,
        Color& out_colorB,
        int& out_countA,
        int& out_countB,
        int maxIters = 10
    );
};

} // namespace lingolens

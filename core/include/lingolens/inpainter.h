#pragma once

#include "types.h"

namespace lingolens {

class Inpainter {
public:
    // Erases text from a detected sign region by replacing text pixels
    // with the background color.
    //
    // Parameters:
    //   frame      - the camera frame (will be modified in place)
    //   region     - the quad where text was detected
    //   foreground - the text color (from StyleAnalyzer)
    //   background - the sign background color (from StyleAnalyzer)
    void inpaint(
        FrameBuffer& frame,
        const Quad& region,
        const Color& foreground,
        const Color& background
    ) const;

private:
    // Determines if a pixel is closer to the foreground (text) color
    // than to the background color.
    static bool isTextPixel(
        const Color& pixel,
        const Color& foreground,
        const Color& background
    );
};

} // namespace lingolens

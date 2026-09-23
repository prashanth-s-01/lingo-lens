#include "../include/lingolens/inpainter.h"
#include "../include/lingolens/style_analyzer.h"
#include <algorithm>

namespace lingolens {

// ---------------------------------------------------------------
// YOUR TASK: Implement isTextPixel
// ---------------------------------------------------------------
// Given a pixel color, determine if it belongs to the text
// (foreground) or the sign surface (background).
//
// Use the colorDistanceSq idea from StyleAnalyzer:
//   - Compute distance from pixel to foreground
//   - Compute distance from pixel to background
//   - If pixel is closer to foreground, it's a text pixel
//
// Hint: You already wrote colorDistanceSq in StyleAnalyzer.
// You can rewrite the same math here (it's just 3 lines), or
// call StyleAnalyzer::colorDistanceSq if you prefer.
// Since colorDistanceSq is private in StyleAnalyzer, let's
// just rewrite the formula inline here.

bool Inpainter::isTextPixel(const Color &pixel, const Color &foreground,
                            const Color &background) {
  // TODO: Return true if pixel is closer to foreground than background
  int r1 = static_cast<int>(pixel.red);
  int r2 = static_cast<int>(foreground.red);
  int g1 = static_cast<int>(pixel.green);
  int g2 = static_cast<int>(foreground.green);
  int b1 = static_cast<int>(pixel.blue);
  int b2 = static_cast<int>(foreground.blue);
  int distForeground =
      (r1 - r2) * (r1 - r2) + (g1 - g2) * (g1 - g2) + (b1 - b2) * (b1 - b2);
  r1 = static_cast<int>(pixel.red);
  r2 = static_cast<int>(background.red);
  g1 = static_cast<int>(pixel.green);
  g2 = static_cast<int>(background.green);
  b1 = static_cast<int>(pixel.blue);
  b2 = static_cast<int>(background.blue);
  int distBackground =
      (r1 - r2) * (r1 - r2) + (g1 - g2) * (g1 - g2) + (b1 - b2) * (b1 - b2);
  return distForeground <= distBackground;
}

// ---------------------------------------------------------------
// YOUR TASK: Implement inpaint
// ---------------------------------------------------------------
// Scan every pixel inside the region's bounding box.
// If a pixel is a text pixel (isTextPixel returns true),
// overwrite it with the background color.
//
// This is very similar to the pixel-scanning loop in
// StyleAnalyzer::analyze() — same bounding box, same index math.
//
// Steps:
//   1. Compute BoundingBox from region (same as analyze)
//   2. Clamp to frame boundaries (same as analyze)
//   3. For each pixel in the bounding box:
//      a. Read the pixel color from frame.data
//      b. If isTextPixel(pixel, foreground, background):
//         - Write background.red, .green, .blue, .alpha back
//           to frame.data at the same index
//      c. Else: leave it alone

void Inpainter::inpaint(FrameBuffer &frame, const Quad &region,
                        const Color &foreground,
                        const Color &background) const {
  // TODO: Implement this function
  BoundingBox bbox(region);
  int startX = std::max(0, static_cast<int>(bbox.topLeft.x));
  int startY = std::max(0, static_cast<int>(bbox.topLeft.y));
  int endX =
      std::min(frame.width - 1, static_cast<int>(bbox.topLeft.x + bbox.width));
  int endY = std::min(frame.height - 1,
                      static_cast<int>(bbox.topLeft.y + bbox.height));
  for (int y = startY; y <= endY; y++) {
    for (int x = startX; x <= endX; x++) {
      int idx = (y * frame.stride) + (x * 4);
      Color c;
      c.red = frame.data[idx];
      c.green = frame.data[idx + 1];
      c.blue = frame.data[idx + 2];
      c.alpha = frame.data[idx + 3];
      if (isTextPixel(c, foreground, background)) {
        frame.data[idx] = background.red;
        frame.data[idx + 1] = background.green;
        frame.data[idx + 2] = background.blue;
        frame.data[idx + 3] = background.alpha;
      }
    }
  }
}

} // namespace lingolens

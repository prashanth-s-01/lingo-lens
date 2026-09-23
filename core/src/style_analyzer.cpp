#include "../include/lingolens/style_analyzer.h"
#include <cmath>

namespace lingolens {

// ---------------------------------------------------------------
// YOUR TASK: Implement colorDistanceSq
// ---------------------------------------------------------------
// Compute the squared Euclidean distance between two Color values.
//
// Remember:
//   distance² = (r1-r2)² + (g1-g2)² + (b1-b2)²
//
// Hint: Color members are uint8_t (0-255). When you subtract two
// uint8_t values, C++ may give unexpected results because uint8_t
// is unsigned. Cast to int first!
//
// Example: int dr = static_cast<int>(a.red) - static_cast<int>(b.red);

double StyleAnalyzer::colorDistanceSq(const Color &a, const Color &b) {
  // TODO: Implement this function
  // Replace the line below with your implementation
  int r1 = static_cast<int>(a.red);
  int r2 = static_cast<int>(b.red);
  int g1 = static_cast<int>(a.green);
  int g2 = static_cast<int>(b.green);
  int b1 = static_cast<int>(a.blue);
  int b2 = static_cast<int>(b.blue);
  return (r1 - r2) * (r1 - r2) + (g1 - g2) * (g1 - g2) + (b1 - b2) * (b1 - b2);
}

// ---------------------------------------------------------------
// YOUR TASK: Implement kMeansTwo
// ---------------------------------------------------------------
// Separate a vector of pixel colors into 2 clusters.
//
// Algorithm:
//   1. Initialize center_A = pixels[0], center_B = pixels[pixels.size()/2]
//      (pick two spread-apart starting points)
//
//   2. Loop for maxIters iterations:
//      a. Reset accumulators: sumA_r, sumA_g, sumA_b, countA = 0
//         (same for B)
//
//      b. For each pixel in pixels:
//         - Compute distA = colorDistanceSq(pixel, center_A)
//         - Compute distB = colorDistanceSq(pixel, center_B)
//         - If distA <= distB: add pixel's r,g,b to sumA, increment countA
//         - Else:             add pixel's r,g,b to sumB, increment countB
//
//      c. Update centers:
//         - center_A = { sumA_r/countA, sumA_g/countA, sumA_b/countA, 255 }
//         - center_B = { sumB_r/countB, sumB_g/countB, sumB_b/countB, 255 }
//         (be careful: don't divide by zero if a cluster is empty!)
//
//   3. Write results to out_colorA, out_colorB, out_countA, out_countB

void StyleAnalyzer::kMeansTwo(const std::vector<Color> &pixels,
                              Color &out_colorA, Color &out_colorB,
                              int &out_countA, int &out_countB, int maxIters) {
  // TODO: Implement this function
  if (pixels.empty()) {
    out_colorA = Color{0, 0, 0, 255};
    out_colorB = Color{0, 0, 0, 255};
    out_countA = 0;
    out_countB = 0;
    return;
  }
  int sumA_r = 0, sumA_g = 0, sumA_b = 0, sumB_r = 0, sumB_g = 0, sumB_b = 0;
  int countA = 0, countB = 0;
  out_colorA = pixels[0];
  out_colorB = pixels[pixels.size() / 2];
  for (int i = 0; i < maxIters; i++) {
    sumA_r = 0;
    sumA_g = 0;
    sumA_b = 0;
    sumB_r = 0;
    sumB_g = 0;
    sumB_b = 0;
    countA = 0;
    countB = 0;
    for (const auto &pixel : pixels) {
      if (colorDistanceSq(pixel, out_colorA) <=
          colorDistanceSq(pixel, out_colorB)) {
        sumA_r += pixel.red;
        sumA_g += pixel.green;
        sumA_b += pixel.blue;
        countA++;
      } else {
        sumB_r += pixel.red;
        sumB_g += pixel.green;
        sumB_b += pixel.blue;
        countB++;
      }
    }
    if (countA > 0) {
      out_colorA.red = sumA_r / countA;
      out_colorA.green = sumA_g / countA;
      out_colorA.blue = sumA_b / countA;
    }
    out_colorA.alpha = 255;
    if (countB > 0) {
      out_colorB.red = sumB_r / countB;
      out_colorB.green = sumB_g / countB;
      out_colorB.blue = sumB_b / countB;
    }
    out_colorB.alpha = 255;
  }
  out_countA = countA;
  out_countB = countB;
}

// ---------------------------------------------------------------
// analyze() — Extracts pixels from the quad region, runs K-means,
// and assigns foreground (text) vs background colors.
// ---------------------------------------------------------------
StyleInfo StyleAnalyzer::analyze(const FrameBuffer &frame,
                                 const Quad &region) const {
  // Step 1: Compute the axis-aligned bounding box of the quad
  // so we know which rows/columns of pixels to scan.
  BoundingBox bbox(region);
  int startX = std::max(0, static_cast<int>(bbox.topLeft.x));
  int startY = std::max(0, static_cast<int>(bbox.topLeft.y));
  int endX = std::min(frame.width - 1, static_cast<int>(bbox.topLeft.x + bbox.width));
  int endY = std::min(frame.height - 1, static_cast<int>(bbox.topLeft.y + bbox.height));

  // Step 2: Collect pixel colors from the bounding box region.
  // (A full implementation would test if each pixel is inside the
  // quad using point-in-polygon, but the bounding box is a good
  // approximation for roughly rectangular signs.)
  std::vector<Color> pixels;
  pixels.reserve((endX - startX) * (endY - startY));

  for (int y = startY; y <= endY; y++) {
    for (int x = startX; x <= endX; x++) {
      // Each pixel is 4 bytes: R, G, B, A (RGBA format)
      int idx = (y * frame.stride) + (x * 4);
      Color c;
      c.red   = frame.data[idx];
      c.green = frame.data[idx + 1];
      c.blue  = frame.data[idx + 2];
      c.alpha = frame.data[idx + 3];
      pixels.push_back(c);
    }
  }

  // Step 3: Run K-means to find the two dominant colors
  Color colorA, colorB;
  int countA, countB;
  kMeansTwo(pixels, colorA, colorB, countA, countB);

  // Step 4: The smaller cluster is the text (foreground),
  // the larger cluster is the sign surface (background).
  StyleInfo info{};
  if (countA <= countB) {
    info.foreground = colorA;
    info.background = colorB;
  } else {
    info.foreground = colorB;
    info.background = colorA;
  }
  info.fontSize = 0.0;    // Will be estimated in a later phase
  info.slantAngle = 0.0;  // Will be estimated in a later phase

  return info;
}

} // namespace lingolens

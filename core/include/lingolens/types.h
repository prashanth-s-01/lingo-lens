#pragma once

#include <cstdint>
#include <algorithm>
#include <string>
#include <vector>

namespace lingolens {

struct Point {
    double x;
    double y;
};

struct Quad {
    Point topLeft;
    Point topRight;
    Point bottomLeft;
    Point bottomRight;
};

struct Color {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    uint8_t alpha;
};

struct BoundingBox {
    Point topLeft;
    double width;
    double height;

    BoundingBox(Quad quad) {
        double min_x = std::min({quad.topLeft.x, quad.topRight.x, quad.bottomLeft.x, quad.bottomRight.x});
        double min_y = std::min({quad.topLeft.y, quad.topRight.y, quad.bottomLeft.y, quad.bottomRight.y});
        double max_x = std::max({quad.topLeft.x, quad.topRight.x, quad.bottomLeft.x, quad.bottomRight.x});
        double max_y = std::max({quad.topLeft.y, quad.topRight.y, quad.bottomLeft.y, quad.bottomRight.y});

        topLeft = Point{min_x, min_y};
        width = max_x - min_x;
        height = max_y - min_y;
    }
};

struct FrameBuffer {
    int width;
    int height;
    int stride;
    std::vector<uint8_t> data;
};

struct StyleInfo {
    Color foreground;
    Color background;
    double fontSize;
    double slantAngle;
};

struct RecognitionResult {
    std::string text;
    std::string detectedLanguage;
    float confidence;
};

struct TranslationResult {
    std::string originalText;
    std::string translatedText;
    std::string sourceLanguage;
    std::string targetLanguage;
};

} // namespace lingolens

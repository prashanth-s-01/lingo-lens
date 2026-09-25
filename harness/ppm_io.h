#pragma once
// ═══════════════════════════════════════════════════════════════════
// Minimal PPM image I/O  (no external dependencies)
// ═══════════════════════════════════════════════════════════════════
// PPM (Portable Pixmap) is a dead-simple image format:
//   P6\n<width> <height>\n255\n<raw RGB bytes>
//
// We convert to/from LingoLens FrameBuffer (RGBA) on the fly.
// ═══════════════════════════════════════════════════════════════════

#include "lingolens/types.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace lingolens {

inline FrameBuffer loadPPM(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open())
        throw std::runtime_error("Cannot open PPM file: " + path);

    std::string magic;
    file >> magic;
    if (magic != "P6")
        throw std::runtime_error("Not a P6 PPM file: " + path);

    // Skip comments
    char c;
    file.get(c);
    while (file.peek() == '#') {
        std::string comment;
        std::getline(file, comment);
    }

    int width, height, maxVal;
    file >> width >> height >> maxVal;
    file.get(c); // consume the single whitespace after maxVal

    if (width <= 0 || height <= 0 || maxVal != 255)
        throw std::runtime_error("Invalid PPM header in: " + path);

    // Read raw RGB data
    std::vector<uint8_t> rgb(width * height * 3);
    file.read(reinterpret_cast<char*>(rgb.data()),
              static_cast<std::streamsize>(rgb.size()));

    // Convert to RGBA FrameBuffer
    FrameBuffer fb;
    fb.width  = width;
    fb.height = height;
    fb.stride = width * 4;
    fb.data.resize(width * height * 4);

    for (int i = 0; i < width * height; ++i) {
        fb.data[i * 4]     = rgb[i * 3];
        fb.data[i * 4 + 1] = rgb[i * 3 + 1];
        fb.data[i * 4 + 2] = rgb[i * 3 + 2];
        fb.data[i * 4 + 3] = 255;
    }

    return fb;
}

inline void savePPM(const std::string& path, const FrameBuffer& fb) {
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open())
        throw std::runtime_error("Cannot create PPM file: " + path);

    file << "P6\n" << fb.width << " " << fb.height << "\n255\n";

    // Convert RGBA → RGB
    std::vector<uint8_t> rgb(fb.width * fb.height * 3);
    for (int y = 0; y < fb.height; ++y)
        for (int x = 0; x < fb.width; ++x) {
            int srcIdx = y * fb.stride + x * 4;
            int dstIdx = (y * fb.width + x) * 3;
            rgb[dstIdx]     = fb.data[srcIdx];
            rgb[dstIdx + 1] = fb.data[srcIdx + 1];
            rgb[dstIdx + 2] = fb.data[srcIdx + 2];
        }

    file.write(reinterpret_cast<const char*>(rgb.data()),
               static_cast<std::streamsize>(rgb.size()));
}

/// Generate a synthetic signboard test image for pipeline demos.
/// Creates a coloured background with white text-like regions.
inline FrameBuffer generateTestSignboard(int width = 640, int height = 200) {
    FrameBuffer fb;
    fb.width  = width;
    fb.height = height;
    fb.stride = width * 4;
    fb.data.resize(width * height * 4);

    // Fill with a dark blue "sign" background
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x) {
            int idx = y * fb.stride + x * 4;
            fb.data[idx]     = 20;   // R
            fb.data[idx + 1] = 40;   // G
            fb.data[idx + 2] = 100;  // B
            fb.data[idx + 3] = 255;  // A
        }

    // Draw a light border
    auto drawRect = [&](int x0, int y0, int x1, int y1,
                        uint8_t r, uint8_t g, uint8_t b) {
        x0 = std::max(0, x0);  y0 = std::max(0, y0);
        x1 = std::min(width-1, x1);  y1 = std::min(height-1, y1);
        for (int y = y0; y <= y1; ++y)
            for (int x = x0; x <= x1; ++x) {
                int idx = y * fb.stride + x * 4;
                fb.data[idx]     = r;
                fb.data[idx + 1] = g;
                fb.data[idx + 2] = b;
            }
    };

    // Border
    int bw = 4;
    drawRect(0, 0, width-1, bw, 200, 200, 200);           // top
    drawRect(0, height-bw-1, width-1, height-1, 200, 200, 200); // bottom
    drawRect(0, 0, bw, height-1, 200, 200, 200);           // left
    drawRect(width-bw-1, 0, width-1, height-1, 200, 200, 200);  // right

    // Draw some "text" — white rectangles simulating letter strokes
    int textY = height / 3;
    int textH = height / 3;
    int letterW = 30;
    int gap = 10;
    int startX = width / 6;

    // Simulate "HELLO" with rough rectangular letter shapes
    // H
    drawRect(startX, textY, startX+6, textY+textH, 240, 240, 240);
    drawRect(startX+letterW-6, textY, startX+letterW, textY+textH, 240, 240, 240);
    drawRect(startX, textY+textH/2-3, startX+letterW, textY+textH/2+3, 240, 240, 240);
    startX += letterW + gap;

    // E
    drawRect(startX, textY, startX+6, textY+textH, 240, 240, 240);
    drawRect(startX, textY, startX+letterW, textY+6, 240, 240, 240);
    drawRect(startX, textY+textH/2-3, startX+letterW-5, textY+textH/2+3, 240, 240, 240);
    drawRect(startX, textY+textH-6, startX+letterW, textY+textH, 240, 240, 240);
    startX += letterW + gap;

    // L
    drawRect(startX, textY, startX+6, textY+textH, 240, 240, 240);
    drawRect(startX, textY+textH-6, startX+letterW, textY+textH, 240, 240, 240);
    startX += letterW + gap;

    // L
    drawRect(startX, textY, startX+6, textY+textH, 240, 240, 240);
    drawRect(startX, textY+textH-6, startX+letterW, textY+textH, 240, 240, 240);
    startX += letterW + gap;

    // O
    drawRect(startX, textY, startX+letterW, textY+6, 240, 240, 240);
    drawRect(startX, textY+textH-6, startX+letterW, textY+textH, 240, 240, 240);
    drawRect(startX, textY, startX+6, textY+textH, 240, 240, 240);
    drawRect(startX+letterW-6, textY, startX+letterW, textY+textH, 240, 240, 240);

    return fb;
}

} // namespace lingolens

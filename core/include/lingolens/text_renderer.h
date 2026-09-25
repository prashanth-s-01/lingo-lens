#pragma once

#include "types.h"
#include "homography.h"
#include <string>
#include <cstdint>

namespace lingolens {

/// Software bitmap-font text renderer.
///
/// Contains a built-in 8×8 pixel font covering printable ASCII (32-126).
/// Text is first rendered into a flat off-screen buffer, then perspective-
/// warped into the target quad on the live frame using Homography::warpInto.
class TextRenderer {
public:
    TextRenderer() = default;

    /// Render @p text into @p frame, warped to fill @p region, using
    /// the foreground color and font size from @p style.
    void renderText(
        FrameBuffer& frame,
        const std::string& text,
        const Quad& region,
        const StyleInfo& style) const;

    /// Estimate the largest font-scale multiplier so that @p text fits
    /// inside a box of @p boxWidth × @p boxHeight.
    static double fitFontSize(
        const std::string& text,
        double boxWidth,
        double boxHeight,
        double minScale = 1.0,
        double maxScale = 16.0);

private:
    // ── Built-in 8×8 bitmap font ──────────────────────────────────
    static const uint8_t FONT_DATA[95][8]; ///< ASCII 32..126, 8 rows each
    static constexpr int GLYPH_W = 8;
    static constexpr int GLYPH_H = 8;

    /// Render @p text at @p scale into a newly-allocated RGBA @p buffer.
    /// Output buffer dimensions are written to @p outW, @p outH.
    static void renderFlat(
        const std::string& text,
        const Color& color,
        double scale,
        FrameBuffer& buffer);

    /// Return pointer to 8-byte glyph bitmap for @p c, or a fallback box.
    static const uint8_t* getGlyph(char c);

    /// Measure pixel dimensions of @p text at @p scale.
    static void measureText(const std::string& text, double scale,
                            int& outWidth, int& outHeight);
};

} // namespace lingolens

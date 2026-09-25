#pragma once

#include "backend_interface.h"
#include "style_analyzer.h"
#include "inpainter.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace lingolens {

// ─── Mock Text Detector ──────────────────────────────────────────
// Simple contrast-based heuristic: scans horizontal bands for high
// intensity variance, then returns bounding quads.
class MockTextDetector : public ITextDetector {
public:
    std::vector<Quad> detect(const FrameBuffer& frame) override;
private:
    static void toGrayscale(const FrameBuffer& frame,
                            std::vector<uint8_t>& gray);
};

// ─── Mock Text Recogniser ────────────────────────────────────────
// Returns a deterministic string derived from the pixel content of
// the region (useful for reproducible testing).
class MockTextRecognizer : public ITextRecognizer {
public:
    RecognitionResult recognize(const FrameBuffer& frame,
                                const Quad& region) override;
};

// ─── Mock Translator ─────────────────────────────────────────────
// Built-in phrase dictionary covering common signboard words in
// English ↔ Spanish, German, French, Japanese, Hindi, Tamil.
class MockTranslator : public ITranslator {
public:
    TranslationResult translate(
        const std::string& text,
        const std::string& sourceLanguage,
        const std::string& targetLanguage) override;

private:
    /// key = "srcLang:targetLang:TEXT", value = translated text
    static const std::unordered_map<std::string, std::string>& dictionary();
};

// ─── Mock Inpainter (backend adapter) ────────────────────────────
// Wraps the core StyleAnalyzer + Inpainter so that it conforms to
// the IInpainter interface (which takes only frame + region).
class MockInpainter : public IInpainter {
public:
    void inpaint(FrameBuffer& frame, const Quad& region) override;
private:
    StyleAnalyzer m_styleAnalyzer;
    Inpainter     m_inpainter;
};

} // namespace lingolens

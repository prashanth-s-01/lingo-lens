#pragma once
// ═══════════════════════════════════════════════════════════════════
// ONNX Runtime backend scaffolding for LingoLens
// ═══════════════════════════════════════════════════════════════════
// These classes wrap ONNX Runtime sessions for text detection (DBNet),
// text recognition (CRNN), and translation (NLLB / MarianMT).
//
// To build:
//   cmake -DLINGOLENS_ENABLE_ONNX=ON -DONNXRUNTIME_ROOT=/path/to/onnxruntime ..
//
// When ONNX Runtime is not available, these files are simply not
// compiled and the Router falls back to Mock backends.
// ═══════════════════════════════════════════════════════════════════

#include "lingolens/backend_interface.h"
#include <memory>
#include <string>

namespace lingolens {

// ─── ONNX Text Detector (DBNet) ─────────────────────────────────
class OnnxTextDetector : public ITextDetector {
public:
    /// @param modelPath  Path to the DBNet .onnx model file.
    explicit OnnxTextDetector(const std::string& modelPath);
    ~OnnxTextDetector();

    std::vector<Quad> detect(const FrameBuffer& frame) override;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

// ─── ONNX Text Recogniser (CRNN / IndicOCR) ─────────────────────
class OnnxTextRecognizer : public ITextRecognizer {
public:
    /// @param modelPath  Path to the CRNN / IndicOCR .onnx model file.
    explicit OnnxTextRecognizer(const std::string& modelPath);
    ~OnnxTextRecognizer();

    RecognitionResult recognize(const FrameBuffer& frame,
                                const Quad& region) override;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

// ─── ONNX Translator (NLLB / MarianMT) ──────────────────────────
class OnnxTranslator : public ITranslator {
public:
    /// @param modelDir  Directory containing encoder.onnx + decoder.onnx.
    explicit OnnxTranslator(const std::string& modelDir);
    ~OnnxTranslator();

    TranslationResult translate(const std::string& text,
                                const std::string& sourceLanguage,
                                const std::string& targetLanguage) override;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace lingolens

#pragma once

#include "types.h"
#include "backend_interface.h"
#include "router.h"
#include "tracker.h"
#include "style_analyzer.h"
#include "inpainter.h"
#include "text_renderer.h"
#include <memory>
#include <string>
#include <vector>

namespace lingolens {

/// Configuration for the real-time translation pipeline.
struct PipelineConfig {
    BackendType backend        = BackendType::Auto;
    std::string targetLanguage = "es";      ///< ISO 639-1 code
    bool        enableTracking = true;
    int         detectionInterval = 15;     ///< frames between full OCR
};

/// Result of processing one detected text region.
struct ProcessedRegion {
    Quad              quad;
    std::string       originalText;
    std::string       translatedText;
    std::string       sourceLanguage;
    StyleInfo         style;
};

/// Main real-time frame pipeline coordinator.
///
/// Orchestrates the full Detect → Recognise → Translate → Inpaint → Render
/// loop.  Supports dual-path operation:
///   • **Fast path** (every frame): optical-flow tracking + cached translations
///   • **Heavy path** (keyframes):  OCR detection + recognition + translation
class Pipeline {
public:
    explicit Pipeline(const PipelineConfig& config);

    /// Process a single camera frame in-place.
    void processFrame(FrameBuffer& frame);

    /// Results from the most recent processFrame() call.
    const std::vector<ProcessedRegion>& lastResults() const;

    /// Change the target language at runtime.
    void setTargetLanguage(const std::string& lang);

private:
    PipelineConfig  m_config;
    Router          m_router;
    Tracker         m_tracker;
    StyleAnalyzer   m_styleAnalyzer;
    Inpainter       m_inpainter;
    TextRenderer    m_textRenderer;

    std::unique_ptr<ITextDetector>   m_detector;
    std::unique_ptr<ITextRecognizer> m_recognizer;
    std::unique_ptr<ITranslator>     m_translator;

    std::vector<ProcessedRegion> m_lastResults;
    std::vector<ProcessedRegion> m_cachedResults;   ///< from last keyframe
    int m_frameCount = 0;
};

} // namespace lingolens

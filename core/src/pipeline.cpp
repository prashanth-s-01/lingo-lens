#include "../include/lingolens/pipeline.h"
#include <algorithm>

namespace lingolens {

Pipeline::Pipeline(const PipelineConfig& config)
    : m_config(config)
    , m_router(config.backend)
{
    m_detector   = m_router.createDetector();
    m_recognizer = m_router.createRecognizer();
    m_translator = m_router.createTranslator();
}

void Pipeline::setTargetLanguage(const std::string& lang) {
    m_config.targetLanguage = lang;
    // Clear cached results so everything is re-translated
    m_cachedResults.clear();
}

const std::vector<ProcessedRegion>& Pipeline::lastResults() const {
    return m_lastResults;
}

// ─── processFrame ───────────────────────────────────────────────
// Dual-path pipeline:
//   • Fast path (every frame): track quads, use cached translations
//   • Heavy path (keyframes):  full detect → recognise → translate
void Pipeline::processFrame(FrameBuffer& frame) {
    m_lastResults.clear();
    ++m_frameCount;

    bool isKeyframe = !m_config.enableTracking ||
                      m_tracker.needsKeyframe() ||
                      m_cachedResults.empty();

    if (isKeyframe) {
        // ── Heavy path ──────────────────────────────────────────
        std::vector<Quad> detections = m_detector->detect(frame);

        m_cachedResults.clear();
        for (auto& quad : detections) {
            ProcessedRegion pr;
            pr.quad = quad;

            // OCR
            RecognitionResult rr = m_recognizer->recognize(frame, quad);
            pr.originalText   = rr.text;
            pr.sourceLanguage = rr.detectedLanguage;

            // Translation
            TranslationResult tr = m_translator->translate(
                rr.text, rr.detectedLanguage, m_config.targetLanguage);
            pr.translatedText = tr.translatedText;

            // Style analysis (always uses core analyser)
            pr.style = m_styleAnalyzer.analyze(frame, quad);

            m_cachedResults.push_back(pr);
        }

        if (m_config.enableTracking) {
            m_tracker.setDetections(detections);
        }
    } else {
        // ── Fast path: optical-flow tracking ────────────────────
        auto tracked = m_tracker.update(frame);

        // Update cached quad positions from tracker
        for (auto& cached : m_cachedResults) {
            for (const auto& tq : tracked) {
                // Match by proximity of top-left corner
                double dx = cached.quad.topLeft.x - tq.quad.topLeft.x;
                double dy = cached.quad.topLeft.y - tq.quad.topLeft.y;
                if (dx * dx + dy * dy < 2500.0) { // within 50px
                    cached.quad = tq.quad;
                    break;
                }
            }
        }
    }

    // ── Render phase (both paths) ───────────────────────────────
    for (const auto& pr : m_cachedResults) {
        // 1. Inpaint (erase original text)
        m_inpainter.inpaint(frame, pr.quad,
                            pr.style.foreground, pr.style.background);

        // 2. Render translated text
        m_textRenderer.renderText(frame, pr.translatedText,
                                  pr.quad, pr.style);
    }

    m_lastResults = m_cachedResults;
}

} // namespace lingolens

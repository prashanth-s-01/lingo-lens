#pragma once

#include "backend_interface.h"
#include <memory>

namespace lingolens {

enum class BackendType {
    Auto,      // Probe device and choose best automatically
    Apple,     // Vision / CoreML / Metal (iOS/macOS)
    MLKit,     // Google ML Kit (Android)
    ONNX,      // Cross-platform ONNX Runtime (also the fallback)
    Mock       // Built-in mock backends for testing / development
};

class Router {
public:
    // Construct a Router. If Auto, probes the device once and caches the result.
    explicit Router(BackendType preferred = BackendType::Auto);

    // Factory methods — create backend implementations for each pipeline stage
    std::unique_ptr<ITextDetector>   createDetector()   const;
    std::unique_ptr<ITextRecognizer> createRecognizer() const;
    std::unique_ptr<ITranslator>     createTranslator() const;
    std::unique_ptr<IInpainter>      createInpainter()  const;

    // Returns the resolved backend after probing
    BackendType resolvedBackend() const;

private:
    BackendType m_resolved;

    // Probes the device and returns the best available backend
    static BackendType probeDevice();
};

} // namespace lingolens

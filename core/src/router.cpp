#include "../include/lingolens/router.h"
#include "../include/lingolens/mock_backends.h"
#include <stdexcept>

namespace lingolens {

// --- Device Probing (compile-time + runtime) ---

BackendType Router::probeDevice() {
  // The preprocessor (#if defined) runs at COMPILE TIME — the compiler
  // strips out code paths that don't apply to the target platform.
  // This is how cross-platform C++ decides which platform-specific
  // APIs are even available in the binary.

#if defined(__APPLE__)
  // On Apple platforms, we have access to Vision framework, CoreML,
  // Metal shaders, and the Neural Engine via the Apple backend.
  return BackendType::Apple;

#elif defined(__ANDROID__)
  // On Android, Google ML Kit provides on-device OCR and translation,
  // with NNAPI / Hexagon DSP acceleration available through ONNX.
  return BackendType::MLKit;

#else
  // Desktop Linux/Windows or unknown platform — fall back to the
  // vendor-agnostic ONNX Runtime backend (CPU or GPU via Vulkan).
  return BackendType::ONNX;

#endif
}

// --- Constructor ---

Router::Router(BackendType preferred)
    : m_resolved(preferred == BackendType::Auto ? probeDevice() : preferred) {
  // If the user passed Auto, we probe and cache.
  // If they passed a specific backend, we trust their choice.
}

// --- Accessor ---

BackendType Router::resolvedBackend() const { return m_resolved; }

// --- Factory Methods ---

std::unique_ptr<ITextDetector> Router::createDetector() const {
  switch (m_resolved) {
  case BackendType::Apple:
    // TODO Phase 4: return std::make_unique<AppleTextDetector>();
    // Fall through to Mock for now
  case BackendType::MLKit:
    // TODO Phase 4: return std::make_unique<MLKitTextDetector>();
    // Fall through to Mock for now
  case BackendType::ONNX:
    // TODO Phase 4: return std::make_unique<OnnxTextDetector>();
    // Fall through to Mock for now
  case BackendType::Mock:
    return std::make_unique<MockTextDetector>();

  default:
    throw std::runtime_error("Unknown backend type");
  }
}

std::unique_ptr<ITextRecognizer> Router::createRecognizer() const {
  switch (m_resolved) {
  case BackendType::Apple:
  case BackendType::MLKit:
  case BackendType::ONNX:
  case BackendType::Mock:
    return std::make_unique<MockTextRecognizer>();

  default:
    throw std::runtime_error("Unknown backend type");
  }
}

std::unique_ptr<ITranslator> Router::createTranslator() const {
  switch (m_resolved) {
  case BackendType::Apple:
  case BackendType::MLKit:
  case BackendType::ONNX:
  case BackendType::Mock:
    return std::make_unique<MockTranslator>();

  default:
    throw std::runtime_error("Unknown backend type");
  }
}

std::unique_ptr<IInpainter> Router::createInpainter() const {
  switch (m_resolved) {
  case BackendType::Apple:
  case BackendType::MLKit:
  case BackendType::ONNX:
  case BackendType::Mock:
    return std::make_unique<MockInpainter>();

  default:
    throw std::runtime_error("Unknown backend type");
  }
}

} // namespace lingolens

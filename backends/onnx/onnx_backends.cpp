// ═══════════════════════════════════════════════════════════════════
// ONNX Runtime backend implementation for LingoLens
// ═══════════════════════════════════════════════════════════════════
// Requires ONNX Runtime headers and library.  Build with:
//   cmake -DLINGOLENS_ENABLE_ONNX=ON -DONNXRUNTIME_ROOT=/path ..
// ═══════════════════════════════════════════════════════════════════

#include "onnx_backends.h"

#include <onnxruntime_cxx_api.h>

#include <algorithm>
#include <cmath>
#include <numeric>
#include <stdexcept>
#include <vector>

namespace lingolens {

// ─── Shared ONNX Runtime environment ────────────────────────────
static Ort::Env& getOrtEnv() {
    static Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "LingoLens");
    return env;
}

// ═══════════════════════════════════════════════════════════════════
// OnnxTextDetector  (DBNet)
// ═══════════════════════════════════════════════════════════════════

struct OnnxTextDetector::Impl {
    Ort::Session             session;
    Ort::AllocatorWithDefaultOptions allocator;

    explicit Impl(const std::string& modelPath)
        : session(getOrtEnv(), modelPath.c_str(), Ort::SessionOptions{}) {}
};

OnnxTextDetector::OnnxTextDetector(const std::string& modelPath)
    : m_impl(std::make_unique<Impl>(modelPath)) {}

OnnxTextDetector::~OnnxTextDetector() = default;

std::vector<Quad> OnnxTextDetector::detect(const FrameBuffer& frame) {
    // 1. Pre-process: resize + normalise to NCHW float32 tensor
    const int targetH = 640, targetW = 640;
    std::vector<float> inputTensor(3 * targetH * targetW);

    double scaleX = static_cast<double>(frame.width)  / targetW;
    double scaleY = static_cast<double>(frame.height) / targetH;

    for (int y = 0; y < targetH; ++y)
        for (int x = 0; x < targetW; ++x) {
            int srcX = std::min(static_cast<int>(x * scaleX), frame.width  - 1);
            int srcY = std::min(static_cast<int>(y * scaleY), frame.height - 1);
            int srcIdx = srcY * frame.stride + srcX * 4;
            // ImageNet normalisation (mean=[0.485,0.456,0.406], std=[0.229,0.224,0.225])
            inputTensor[0 * targetH * targetW + y * targetW + x] =
                (frame.data[srcIdx]     / 255.0f - 0.485f) / 0.229f;
            inputTensor[1 * targetH * targetW + y * targetW + x] =
                (frame.data[srcIdx + 1] / 255.0f - 0.456f) / 0.224f;
            inputTensor[2 * targetH * targetW + y * targetW + x] =
                (frame.data[srcIdx + 2] / 255.0f - 0.406f) / 0.225f;
        }

    // 2. Run inference
    std::array<int64_t, 4> inputShape = {1, 3, targetH, targetW};
    auto memInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    Ort::Value inputVal = Ort::Value::CreateTensor<float>(
        memInfo, inputTensor.data(), inputTensor.size(),
        inputShape.data(), inputShape.size());

    auto inputName  = m_impl->session.GetInputNameAllocated(0, m_impl->allocator);
    auto outputName = m_impl->session.GetOutputNameAllocated(0, m_impl->allocator);
    const char* inNames[]  = {inputName.get()};
    const char* outNames[] = {outputName.get()};

    auto outputTensors = m_impl->session.Run(
        Ort::RunOptions{nullptr}, inNames, &inputVal, 1, outNames, 1);

    // 3. Post-process: threshold probability map → bounding quads
    const float* probMap = outputTensors[0].GetTensorData<float>();
    auto outShape = outputTensors[0].GetTensorTypeAndShapeInfo().GetShape();
    int mapH = static_cast<int>(outShape[2]);
    int mapW = static_cast<int>(outShape[3]);

    const float THRESHOLD = 0.3f;
    std::vector<Quad> results;

    // Simple connected-component bounding box extraction
    std::vector<bool> visited(mapH * mapW, false);
    for (int y = 0; y < mapH; ++y)
        for (int x = 0; x < mapW; ++x) {
            if (visited[y * mapW + x] || probMap[y * mapW + x] < THRESHOLD)
                continue;

            // BFS flood fill
            int minX = x, maxX = x, minY = y, maxY = y;
            std::vector<std::pair<int,int>> stack = {{x, y}};
            visited[y * mapW + x] = true;

            while (!stack.empty()) {
                auto [cx, cy] = stack.back(); stack.pop_back();
                minX = std::min(minX, cx); maxX = std::max(maxX, cx);
                minY = std::min(minY, cy); maxY = std::max(maxY, cy);

                for (auto [dx, dy] : std::initializer_list<std::pair<int,int>>{
                        {1,0},{-1,0},{0,1},{0,-1}}) {
                    int nx = cx + dx, ny = cy + dy;
                    if (nx >= 0 && nx < mapW && ny >= 0 && ny < mapH &&
                        !visited[ny * mapW + nx] &&
                        probMap[ny * mapW + nx] >= THRESHOLD) {
                        visited[ny * mapW + nx] = true;
                        stack.push_back({nx, ny});
                    }
                }
            }

            // Scale back to original image coordinates
            Quad q;
            q.topLeft     = {minX * scaleX, minY * scaleY};
            q.topRight    = {(maxX + 1) * scaleX, minY * scaleY};
            q.bottomLeft  = {minX * scaleX, (maxY + 1) * scaleY};
            q.bottomRight = {(maxX + 1) * scaleX, (maxY + 1) * scaleY};
            results.push_back(q);
        }

    return results;
}

// ═══════════════════════════════════════════════════════════════════
// OnnxTextRecognizer  (CRNN)
// ═══════════════════════════════════════════════════════════════════

struct OnnxTextRecognizer::Impl {
    Ort::Session             session;
    Ort::AllocatorWithDefaultOptions allocator;

    explicit Impl(const std::string& modelPath)
        : session(getOrtEnv(), modelPath.c_str(), Ort::SessionOptions{}) {}
};

OnnxTextRecognizer::OnnxTextRecognizer(const std::string& modelPath)
    : m_impl(std::make_unique<Impl>(modelPath)) {}

OnnxTextRecognizer::~OnnxTextRecognizer() = default;

RecognitionResult OnnxTextRecognizer::recognize(
    const FrameBuffer& frame, const Quad& region)
{
    // 1. Crop and resize region to model input (e.g. 32×100 grayscale)
    BoundingBox bbox(region);
    const int cropH = 32, cropW = 100;
    std::vector<float> inputTensor(1 * cropH * cropW);

    double sx = bbox.width  / cropW;
    double sy = bbox.height / cropH;

    for (int y = 0; y < cropH; ++y)
        for (int x = 0; x < cropW; ++x) {
            int srcX = std::clamp(
                static_cast<int>(bbox.topLeft.x + x * sx), 0, frame.width - 1);
            int srcY = std::clamp(
                static_cast<int>(bbox.topLeft.y + y * sy), 0, frame.height - 1);
            int idx = srcY * frame.stride + srcX * 4;
            float gray = 0.299f * frame.data[idx] +
                         0.587f * frame.data[idx+1] +
                         0.114f * frame.data[idx+2];
            inputTensor[y * cropW + x] = gray / 255.0f;
        }

    // 2. Run CRNN inference
    std::array<int64_t, 4> shape = {1, 1, cropH, cropW};
    auto memInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    Ort::Value inputVal = Ort::Value::CreateTensor<float>(
        memInfo, inputTensor.data(), inputTensor.size(),
        shape.data(), shape.size());

    auto inName  = m_impl->session.GetInputNameAllocated(0, m_impl->allocator);
    auto outName = m_impl->session.GetOutputNameAllocated(0, m_impl->allocator);
    const char* inNames[]  = {inName.get()};
    const char* outNames[] = {outName.get()};

    auto outputs = m_impl->session.Run(
        Ort::RunOptions{nullptr}, inNames, &inputVal, 1, outNames, 1);

    // 3. CTC-decode output logits to text
    const float* logits = outputs[0].GetTensorData<float>();
    auto outShape = outputs[0].GetTensorTypeAndShapeInfo().GetShape();
    int seqLen   = static_cast<int>(outShape[0]);
    int numClass = static_cast<int>(outShape[2]);

    // Standard CTC alphabet (index 0 = blank)
    static const char ALPHABET[] =
        "0123456789abcdefghijklmnopqrstuvwxyz"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ !\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~";

    std::string decoded;
    float totalConf = 0.0f;
    int prevIdx = 0; // blank
    for (int t = 0; t < seqLen; ++t) {
        // Argmax over classes
        int bestIdx = 0;
        float bestVal = logits[t * numClass];
        for (int c = 1; c < numClass; ++c) {
            if (logits[t * numClass + c] > bestVal) {
                bestVal = logits[t * numClass + c];
                bestIdx = c;
            }
        }
        totalConf += bestVal;

        // CTC: skip blanks and repeated characters
        if (bestIdx != 0 && bestIdx != prevIdx) {
            int charIdx = bestIdx - 1;
            if (charIdx >= 0 && charIdx < static_cast<int>(sizeof(ALPHABET) - 1))
                decoded += ALPHABET[charIdx];
        }
        prevIdx = bestIdx;
    }

    RecognitionResult result;
    result.text             = decoded;
    result.detectedLanguage = "en"; // Language detection would be a separate model
    result.confidence       = seqLen > 0 ? totalConf / seqLen : 0.0f;
    return result;
}

// ═══════════════════════════════════════════════════════════════════
// OnnxTranslator  (NLLB / MarianMT encoder-decoder)
// ═══════════════════════════════════════════════════════════════════

struct OnnxTranslator::Impl {
    Ort::Session             encoderSession;
    Ort::Session             decoderSession;
    Ort::AllocatorWithDefaultOptions allocator;

    Impl(const std::string& modelDir)
        : encoderSession(getOrtEnv(),
            (modelDir + "/encoder.onnx").c_str(), Ort::SessionOptions{})
        , decoderSession(getOrtEnv(),
            (modelDir + "/decoder.onnx").c_str(), Ort::SessionOptions{}) {}
};

OnnxTranslator::OnnxTranslator(const std::string& modelDir)
    : m_impl(std::make_unique<Impl>(modelDir)) {}

OnnxTranslator::~OnnxTranslator() = default;

TranslationResult OnnxTranslator::translate(
    const std::string& text,
    const std::string& sourceLanguage,
    const std::string& targetLanguage)
{
    // Note: A full implementation would include:
    // 1. SentencePiece tokenization of the input text
    // 2. Running the encoder to produce hidden states
    // 3. Auto-regressive decoding with the decoder
    // 4. SentencePiece de-tokenization of the output
    //
    // For now, this is a structural placeholder that shows the correct
    // ONNX Runtime API usage.  Plug in a real NLLB-200 or MarianMT
    // model to enable actual translation.

    // Simple character-level "tokenisation" placeholder
    std::vector<int64_t> inputIds;
    for (char c : text)
        inputIds.push_back(static_cast<int64_t>(c));

    std::array<int64_t, 2> shape = {1, static_cast<int64_t>(inputIds.size())};
    auto memInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);

    Ort::Value encoderInput = Ort::Value::CreateTensor<int64_t>(
        memInfo, inputIds.data(), inputIds.size(),
        shape.data(), shape.size());

    auto inName  = m_impl->encoderSession.GetInputNameAllocated(0, m_impl->allocator);
    auto outName = m_impl->encoderSession.GetOutputNameAllocated(0, m_impl->allocator);
    const char* inNames[]  = {inName.get()};
    const char* outNames[] = {outName.get()};

    auto encoderOutputs = m_impl->encoderSession.Run(
        Ort::RunOptions{nullptr}, inNames, &encoderInput, 1, outNames, 1);

    // TODO: Feed encoder output into decoder for auto-regressive generation.
    // This requires a beam-search or greedy loop calling the decoder
    // session repeatedly with attention mask updates.

    TranslationResult result;
    result.originalText   = text;
    result.translatedText = "[ONNX:" + targetLanguage + "] " + text;
    result.sourceLanguage = sourceLanguage;
    result.targetLanguage = targetLanguage;
    return result;
}

} // namespace lingolens

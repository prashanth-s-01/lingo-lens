// ═══════════════════════════════════════════════════════════════════
// LingoLens Test Harness
// ═══════════════════════════════════════════════════════════════════
// Desktop CLI tool that runs the full translation pipeline on a
// test image (PPM format) or a built-in synthetic signboard.
//
// Usage:
//   lingolens_harness [options]
//
// Options:
//   --input  <file.ppm>   Input PPM image (default: built-in test image)
//   --output <file.ppm>   Output PPM image (default: output.ppm)
//   --lang   <code>       Target language ISO 639-1 code (default: es)
//   --generate            Generate test signboard and exit
//   --help                Show usage
// ═══════════════════════════════════════════════════════════════════

#include "ppm_io.h"
#include "lingolens/pipeline.h"

#include <chrono>
#include <cstring>
#include <iostream>
#include <string>

static void printUsage(const char* argv0) {
    std::cout
        << "LingoLens Test Harness\n"
        << "━━━━━━━━━━━━━━━━━━━━━\n"
        << "Usage: " << argv0 << " [options]\n\n"
        << "Options:\n"
        << "  --input  <file.ppm>   Input PPM image (default: built-in)\n"
        << "  --output <file.ppm>   Output PPM image (default: output.ppm)\n"
        << "  --lang   <code>       Target language: es, de, fr, ja, hi, ta\n"
        << "                        (default: es)\n"
        << "  --generate            Generate test signboard → test_sign.ppm\n"
        << "  --help                Show this help\n\n"
        << "Supported target language codes:\n"
        << "  es  Spanish       de  German        fr  French\n"
        << "  ja  Japanese      hi  Hindi         ta  Tamil\n\n"
        << "Example:\n"
        << "  " << argv0 << " --generate\n"
        << "  " << argv0 << " --input test_sign.ppm --lang de --output out.ppm\n";
}

int main(int argc, char* argv[]) {
    std::string inputPath;
    std::string outputPath = "output.ppm";
    std::string targetLang = "es";
    bool generateOnly = false;

    // ── Parse command-line arguments ────────────────────────────
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--help") == 0 ||
            std::strcmp(argv[i], "-h") == 0) {
            printUsage(argv[0]);
            return 0;
        }
        if (std::strcmp(argv[i], "--input") == 0 && i + 1 < argc) {
            inputPath = argv[++i];
        } else if (std::strcmp(argv[i], "--output") == 0 && i + 1 < argc) {
            outputPath = argv[++i];
        } else if (std::strcmp(argv[i], "--lang") == 0 && i + 1 < argc) {
            targetLang = argv[++i];
        } else if (std::strcmp(argv[i], "--generate") == 0) {
            generateOnly = true;
        } else {
            std::cerr << "Unknown option: " << argv[i] << "\n\n";
            printUsage(argv[0]);
            return 1;
        }
    }

    // ── Generate mode ───────────────────────────────────────────
    if (generateOnly) {
        auto fb = lingolens::generateTestSignboard(640, 200);
        lingolens::savePPM("test_sign.ppm", fb);
        std::cout << "✓ Generated test_sign.ppm (640×200)\n";
        return 0;
    }

    // ── Load or generate input image ────────────────────────────
    lingolens::FrameBuffer frame;
    if (inputPath.empty()) {
        std::cout << "No --input specified, using built-in test signboard.\n";
        frame = lingolens::generateTestSignboard(640, 200);
    } else {
        std::cout << "Loading: " << inputPath << "\n";
        frame = lingolens::loadPPM(inputPath);
    }
    std::cout << "Image: " << frame.width << "×" << frame.height << " pixels\n";

    // ── Configure pipeline ──────────────────────────────────────
    lingolens::PipelineConfig config;
    config.backend        = lingolens::BackendType::Mock;
    config.targetLanguage = targetLang;
    config.enableTracking = false;  // single-frame mode

    std::cout << "Backend: Mock\n";
    std::cout << "Target language: " << targetLang << "\n\n";

    // ── Run pipeline ────────────────────────────────────────────
    lingolens::Pipeline pipeline(config);

    auto t0 = std::chrono::high_resolution_clock::now();
    pipeline.processFrame(frame);
    auto t1 = std::chrono::high_resolution_clock::now();

    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    // ── Report results ──────────────────────────────────────────
    const auto& results = pipeline.lastResults();
    std::cout << "━━━ Pipeline Results ━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
    std::cout << "Regions detected: " << results.size() << "\n";

    for (size_t i = 0; i < results.size(); ++i) {
        const auto& r = results[i];
        std::cout << "\n  Region " << i + 1 << ":\n";
        std::cout << "    Original:   \"" << r.originalText << "\" ("
                  << r.sourceLanguage << ")\n";
        std::cout << "    Translated: \"" << r.translatedText << "\" ("
                  << targetLang << ")\n";
        std::cout << "    Quad:       ("
                  << r.quad.topLeft.x << "," << r.quad.topLeft.y << ") → ("
                  << r.quad.bottomRight.x << "," << r.quad.bottomRight.y
                  << ")\n";
        std::cout << "    Text color: RGB("
                  << static_cast<int>(r.style.foreground.red) << ","
                  << static_cast<int>(r.style.foreground.green) << ","
                  << static_cast<int>(r.style.foreground.blue) << ")\n";
        std::cout << "    Bg color:   RGB("
                  << static_cast<int>(r.style.background.red) << ","
                  << static_cast<int>(r.style.background.green) << ","
                  << static_cast<int>(r.style.background.blue) << ")\n";
    }

    std::cout << "\nLatency: " << ms << " ms\n";

    // ── Save output ─────────────────────────────────────────────
    lingolens::savePPM(outputPath, frame);
    std::cout << "✓ Saved: " << outputPath << "\n";

    return 0;
}

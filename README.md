# LingoLens

**See the world in your language.**

LingoLens is an on-device, real-time augmented reality translation engine for camera feeds. It detects signboard text through the camera, erases the original text, translates it, and re-renders the translation in-place — matching the original font size, color, perspective, and style — all in real time across iOS, Android, and desktop platforms.

---

## Supported Languages

| Language | Script | Backend Route |
|----------|--------|---------------|
| English | Latin | Native (Apple Vision / ML Kit) |
| Spanish | Latin | Native (Apple Vision / ML Kit) |
| German | Latin | Native (Apple Vision / ML Kit) |
| French | Latin | Native (Apple Vision / ML Kit) |
| Japanese | CJK | Native / ONNX fallback |
| Hindi | Devanagari (Indic) | ONNX (IndicTrans2 / NLLB) |
| Tamil | Tamil (Indic) | ONNX (IndicTrans2 / NLLB) |

> The Router automatically selects the best backend per language. Indic scripts that lack native OS support are dynamically routed to the ONNX pipeline.

---

## Architecture

```
                       ┌────────────────────────────────┐
                       │     LingoLens Application      │
                       │   (iOS SwiftUI / Android UI)   │
                       └───────────────┬────────────────┘
                                       │
                       ┌───────────────▼────────────────┐
                       │      Router & Probe Layer      │
                       │ (Device, Chipset, Script Check) │
                       └───────┬───────┬────────┬───────┘
                               │       │        │
            ┌──────────────────┘       │        └──────────────────┐
            ▼                          ▼                           ▼
   ┌─────────────────┐       ┌─────────────────┐         ┌─────────────────┐
   │  Apple Backend  │       │ ML Kit Backend  │         │  ONNX Backend   │
   │  (iOS / macOS)  │       │   (Android)     │         │ (Cross-platform)│
   ├─────────────────┤       ├─────────────────┤         ├─────────────────┤
   │• Vision OCR     │       │• ML Kit v2 OCR  │         │• DBNet Text Det │
   │• Translation API│       │• ML Kit Trans.  │         │• CRNN / IndicOCR│
   │• Metal Shaders  │       │• Surface Shaders│         │• NLLB / MarianMT│
   └────────┬────────┘       └────────┬────────┘         └────────┬────────┘
            │                         │                           │
            └─────────────────────────┼───────────────────────────┘
                                      ▼
                       ┌────────────────────────────────┐
                       │    LingoLens Core Pipeline     │
                       ├────────────────────────────────┤
                       │ 1. Text Detection              │
                       │ 2. Optical Flow Tracking (LK)  │
                       │ 3. Style & Color Analysis      │
                       │ 4. Background Inpainting       │
                       │ 5. Translation                 │
                       │ 6. Perspective Text Rendering   │
                       └────────────────────────────────┘
```

---

## Core Pipeline

The real-time pipeline processes each camera frame through a **dual-path** architecture:

- **Heavy path** (keyframes, ~5-12 FPS): Full detect → recognise → translate → style analysis
- **Fast path** (every frame, 30-60 FPS): Lucas-Kanade optical flow tracking with cached translations

### 1. Text Detection
Detects text regions as 4-point quadrilaterals (`Quad`) on signboards. Handles angled and tilted signs via perspective-aware bounding.

### 2. Optical Flow Tracking
Lucas-Kanade tracker keeps detected quads locked to the camera's movement frame-to-frame, so the expensive OCR path only runs on keyframes.

### 3. Text Recognition (OCR)
Recognizes detected text and identifies the source language. Returns a `RecognitionResult` with the text, detected language, and confidence score.

### 4. Style & Color Analysis
Extracts the visual style of the original sign using **K-means clustering (K=2)** to separate:
- **Foreground** (text color) — the smaller cluster
- **Background** (sign surface color) — the larger cluster

### 5. Background Inpainting
Erases the original text by classifying each pixel as text or background (using squared Euclidean color distance) and replacing text pixels with the background color.

### 6. Translation
Translates recognized text to the target language. The Router selects the optimal translation backend based on the language pair and device capabilities.

### 7. Text Rendering & Overlay
Renders translated text with:
- Built-in 8×8 bitmap font (ASCII) with optional **HarfBuzz + FreeType** for complex scripts
- 4-point homography-based perspective warp matching the sign's 3D orientation
- Dynamic font fitting to keep text within the original bounding quad

---

## Backend Routing

The `Router` automatically selects the optimal backend at startup via compile-time platform detection and runtime capability probing:

| Backend | Platform | Acceleration | Use Case |
|---------|----------|-------------|----------|
| **Apple** | iOS / macOS | Vision, CoreML, Metal, Neural Engine | Primary path on Apple devices |
| **MLKit** | Android | ML Kit v2, NNAPI, Hexagon DSP | Primary path on Android devices |
| **ONNX** | All platforms | ONNX Runtime (CPU / Vulkan GPU) | Cross-platform, Indic scripts |
| **Mock** | All platforms | CPU (no ML models) | Development & testing |

```cpp
// The Router probes the device and caches the result:
Router router(BackendType::Auto);  // Auto-detect best backend

// Factory methods create the right implementation per stage:
auto detector   = router.createDetector();
auto recognizer = router.createRecognizer();
auto translator = router.createTranslator();
auto inpainter  = router.createInpainter();
```

---

## Project Structure

```
lingo-lens/
├── CMakeLists.txt                  # Root build — C++20
├── core/
│   ├── CMakeLists.txt              # Core static library (lingolens_core)
│   ├── include/lingolens/
│   │   ├── types.h                 # Point, Quad, Color, BoundingBox, FrameBuffer,
│   │   │                           # StyleInfo, RecognitionResult, TranslationResult
│   │   ├── backend_interface.h     # ITextDetector, ITextRecognizer, ITranslator, IInpainter
│   │   ├── router.h                # BackendType enum, Router (probe + factory)
│   │   ├── homography.h            # 3x3 matrix, DLT solve, perspective warp
│   │   ├── tracker.h               # Lucas-Kanade optical flow tracker
│   │   ├── style_analyzer.h        # K-means color extraction
│   │   ├── inpainter.h             # Text erasure via pixel classification
│   │   ├── text_renderer.h         # 8x8 bitmap font + perspective rendering
│   │   ├── mock_backends.h         # Mock detector, recognizer, translator
│   │   └── pipeline.h              # Dual-path pipeline coordinator
│   └── src/
│       ├── router.cpp              # Compile-time platform detection + factories
│       ├── homography.cpp          # DLT homography, Gaussian elimination, warp
│       ├── tracker.cpp             # Iterative LK with structure tensor
│       ├── style_analyzer.cpp      # K-means clustering, pixel scanning
│       ├── inpainter.cpp           # Text pixel replacement
│       ├── text_renderer.cpp       # CP437 bitmap font, flat render + warp
│       ├── mock_backends.cpp       # Contrast-band detector, dictionary translator
│       └── pipeline.cpp            # Keyframe + tracking orchestration
├── backends/
│   ├── CMakeLists.txt              # Optional ONNX build gate
│   └── onnx/
│       ├── onnx_backends.h         # OnnxTextDetector, OnnxTextRecognizer, OnnxTranslator
│       └── onnx_backends.cpp       # DBNet + CRNN + NLLB via ONNX Runtime
├── harness/
│   ├── CMakeLists.txt              # CLI test harness executable
│   ├── main.cpp                    # End-to-end pipeline runner with latency profiling
│   └── ppm_io.h                    # Zero-dependency PPM image I/O + test image generator
├── platform/                       # App shells
│   ├── ios/                        #   iOS SwiftUI
│   └── android/                    #   Android Jetpack Compose
└── models/
    └── manifest.json               # On-demand language pack registry
```

---

## Installation & Building

### Prerequisites

| Dependency | Version | Required |
|-----------|---------|----------|
| **CMake** | ≥ 3.20 | ✅ Yes |
| **C++20 compiler** | Clang 14+, GCC 12+, MSVC 19.29+ | ✅ Yes |
| **ONNX Runtime** | ≥ 1.16 | ❌ Optional (for ONNX backend) |

#### macOS

```bash
# Install build tools via Homebrew
brew install cmake

# Xcode Command Line Tools (provides Apple Clang)
xcode-select --install
```

#### Ubuntu / Debian

```bash
sudo apt update
sudo apt install cmake g++ build-essential
```

#### Windows

```powershell
# Via winget
winget install Kitware.CMake

# Or use Visual Studio 2022+ (includes MSVC and CMake integration)
```

### Basic Build (Mock Backend)

The project builds and runs out of the box with no ML dependencies — the mock backend provides a built-in dictionary translator and contrast-based text detector:

```bash
# Clone
git clone https://github.com/your-org/lingo-lens.git
cd lingo-lens

# Configure
cmake -B build -S .

# Build
cmake --build build

# Run the test harness
./build/harness/lingolens_harness --help
```

### Build with ONNX Runtime

To enable the ONNX backend (required for production Indic/CJK translation):

```bash
# 1. Download ONNX Runtime
#    https://github.com/microsoft/onnxruntime/releases
#    Extract to e.g. /opt/onnxruntime

# 2. Configure with ONNX enabled
cmake -B build -S . \
  -DLINGOLENS_ENABLE_ONNX=ON \
  -DONNXRUNTIME_ROOT=/opt/onnxruntime

# 3. Build
cmake --build build
```

### Build Targets

| Target | Type | Description |
|--------|------|-------------|
| `lingolens_core` | Static library | Core engine + mock backends |
| `lingolens_harness` | Executable | CLI test harness |
| `lingolens_onnx` | Static library | ONNX backend (optional) |

---

## Running the Test Harness

The harness runs the complete pipeline on a single image and reports results:

```bash
# Run with built-in test signboard → Spanish
./build/harness/lingolens_harness --lang es

# Run with built-in test signboard → German
./build/harness/lingolens_harness --lang de --output out_de.ppm

# Generate a test signboard image
./build/harness/lingolens_harness --generate

# Process your own PPM image
./build/harness/lingolens_harness --input sign.ppm --lang fr --output translated.ppm
```

### Example Output

```
Image: 640×200 pixels
Backend: Mock
Target language: es

━━━ Pipeline Results ━━━━━━━━━━━━━━━━━━━━━━━━━━━
Regions detected: 3

  Region 1:
    Original:   "HELLO WORLD" (en)
    Translated: "HOLA MUNDO" (es)
    Quad:       (0,56) → (639,144)
    Text color: RGB(233,233,233)
    Bg color:   RGB(20,40,100)

Latency: 21.16 ms
✓ Saved: output.ppm
```

### Supported Language Codes

| Code | Language | Code | Language |
|------|----------|------|----------|
| `es` | Spanish | `ja` | Japanese |
| `de` | German | `hi` | Hindi |
| `fr` | French | `ta` | Tamil |

---

## Deployment

### Desktop (CLI / Test)

The harness binary is self-contained — copy `lingolens_harness` to any machine with the same OS/arch. No runtime dependencies beyond the C++ standard library.

### iOS

1. Add the `core/` directory to your Xcode project as a C++ library target
2. Bridge via Objective-C++ (`.mm` files) into Swift/SwiftUI
3. Use `BackendType::Apple` for hardware-accelerated Vision + CoreML + Metal
4. Link against `Vision.framework`, `CoreML.framework`, and `Metal.framework`

### Android

1. Add `core/` as a native library via CMake in `build.gradle`
2. Bridge via JNI into Kotlin/Jetpack Compose
3. Use `BackendType::MLKit` for on-device ML Kit OCR + Translation
4. Add `com.google.mlkit:text-recognition` and `com.google.mlkit:translate` to Gradle

### ONNX Runtime (Cross-platform)

1. Download pre-built ONNX Runtime for your target platform
2. Download model files:
   - **Text Detection**: DBNet (`.onnx`)
   - **Text Recognition**: CRNN or IndicOCR (`.onnx`)
   - **Translation**: NLLB-200 or MarianMT encoder/decoder (`.onnx`)
3. Build with `-DLINGOLENS_ENABLE_ONNX=ON -DONNXRUNTIME_ROOT=/path`
4. Place model files in the `models/` directory

---

## API Quick Reference

```cpp
#include "lingolens/pipeline.h"

// Configure
lingolens::PipelineConfig config;
config.backend        = lingolens::BackendType::Mock;
config.targetLanguage = "es";
config.enableTracking = true;

// Create pipeline
lingolens::Pipeline pipeline(config);

// Process frames (call in your render loop)
pipeline.processFrame(frame);  // modifies frame in-place

// Access results
for (const auto& r : pipeline.lastResults()) {
    std::cout << r.originalText << " → " << r.translatedText << "\n";
}

// Change language at runtime
pipeline.setTargetLanguage("de");
```
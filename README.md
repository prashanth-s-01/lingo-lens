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
                       │ 1. Frame Preprocessing         │
                       │ 2. Optical Flow Tracking (LK)  │
                       │ 3. Style & Color Analysis      │
                       │ 4. Background Inpainting       │
                       │ 5. Perspective Text Rendering   │
                       │ 6. Dynamic Font Fitting        │
                       └────────────────────────────────┘
```

---

## Core Pipeline

The real-time pipeline processes each camera frame through these stages:

### 1. Text Detection
Detects text regions as 4-point quadrilaterals (`Quad`) on signboards. Handles angled and tilted signs via perspective-aware bounding.

### 2. Text Recognition (OCR)
Recognizes detected text and identifies the source language. Returns a `RecognitionResult` with the text, detected language, and confidence score.

### 3. Style & Color Analysis
Extracts the visual style of the original sign using **K-means clustering (K=2)** to separate:
- **Foreground** (text color) — the smaller cluster
- **Background** (sign surface color) — the larger cluster

This ensures the translated text blends naturally with the sign.

### 4. Background Inpainting
Erases the original text by classifying each pixel as text or background (using squared Euclidean color distance) and replacing text pixels with the background color.

### 5. Translation
Translates recognized text to the target language. The Router selects the optimal translation backend based on the language pair and device capabilities.

### 6. Text Rendering & Overlay
Renders translated text with:
- **HarfBuzz + FreeType** for complex script shaping (Brahmic ligatures, CJK glyphs, diacritics)
- Perspective transform matching the sign's 3D orientation
- Dynamic font fitting to keep text within the original bounding quad

---

## Backend Routing

The `Router` automatically selects the optimal backend at startup via compile-time platform detection and runtime capability probing:

| Backend | Platform | Acceleration | Use Case |
|---------|----------|-------------|----------|
| **Apple** | iOS / macOS | Vision, CoreML, Metal, Neural Engine | Primary path on Apple devices |
| **MLKit** | Android | ML Kit v2, NNAPI, Hexagon DSP | Primary path on Android devices |
| **ONNX** | All platforms | ONNX Runtime (CPU / Vulkan GPU) | Cross-platform fallback, Indic scripts |

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
├── CMakeLists.txt                  # Root build — C++20, includes core/
├── core/
│   ├── CMakeLists.txt              # Core library target
│   ├── include/lingolens/
│   │   ├── types.h                 # Point, Quad, Color, BoundingBox, FrameBuffer,
│   │   │                           # StyleInfo, RecognitionResult, TranslationResult
│   │   ├── backend_interface.h     # ITextDetector, ITextRecognizer, ITranslator, IInpainter
│   │   ├── router.h                # BackendType enum, Router (probe + factory)
│   │   ├── style_analyzer.h        # K-means color extraction
│   │   └── inpainter.h             # Text erasure via pixel classification
│   └── src/
│       ├── router.cpp              # Compile-time platform detection + factory stubs
│       ├── style_analyzer.cpp      # K-means clustering, bounding box pixel scanning
│       └── inpainter.cpp           # Text pixel replacement with background color
├── backends/                       #Platform-specific backend implementations
│   ├── apple/                      #   Apple Vision + TranslationSession + Metal
│   ├── mlkit/                      #   Android Google ML Kit
│   └── onnx/                       #   ONNX Runtime vendor-agnostic backend
├── harness/                        # Desktop CLI test harness
├── platform/                       # App shells
│   ├── ios/                        #   iOS SwiftUI
│   └── android/                    #   Android Jetpack Compose
└── models/
    └── manifest.json               # On-demand language pack registry
```

---

## Building

### Prerequisites
- **CMake** ≥ 3.20
- A C++20-capable compiler (Clang 14+, GCC 12+, MSVC 19.29+)

### Build Steps

```bash
# Configure
cmake -B build -S .

# Build
cmake --build build
```
#include "../include/lingolens/mock_backends.h"
#include <algorithm>
#include <cmath>
#include <cctype>
#include <numeric>

namespace lingolens {

// ═══════════════════════════════════════════════════════════════════
// MockTextDetector — contrast-band heuristic
// ═══════════════════════════════════════════════════════════════════
// Scans horizontal bands for regions with high intensity variance,
// which is a rough proxy for "there is text here."

void MockTextDetector::toGrayscale(const FrameBuffer& frame,
                                   std::vector<uint8_t>& gray) {
    gray.resize(frame.width * frame.height);
    for (int y = 0; y < frame.height; ++y)
        for (int x = 0; x < frame.width; ++x) {
            int idx = y * frame.stride + x * 4;
            gray[y * frame.width + x] = static_cast<uint8_t>(
                0.299 * frame.data[idx] +
                0.587 * frame.data[idx + 1] +
                0.114 * frame.data[idx + 2]);
        }
}

std::vector<Quad> MockTextDetector::detect(const FrameBuffer& frame) {
    std::vector<uint8_t> gray;
    toGrayscale(frame, gray);

    const int W = frame.width;
    const int H = frame.height;
    const int BAND_H = std::max(8, H / 20);  // scan in horizontal bands
    const double VAR_THRESHOLD = 400.0;       // variance needed to be "text"

    // For each band, compute variance of pixel intensities.
    // Contiguous high-variance bands are merged into text regions.
    struct Band { int y0; int y1; double variance; };
    std::vector<Band> bands;

    for (int by = 0; by + BAND_H <= H; by += BAND_H / 2) {
        // Compute mean
        double sum = 0;
        int count = 0;
        for (int y = by; y < by + BAND_H && y < H; ++y)
            for (int x = 0; x < W; ++x) {
                sum += gray[y * W + x];
                ++count;
            }
        double mean = sum / count;

        // Compute variance
        double varSum = 0;
        for (int y = by; y < by + BAND_H && y < H; ++y)
            for (int x = 0; x < W; ++x) {
                double d = gray[y * W + x] - mean;
                varSum += d * d;
            }
        double var = varSum / count;

        if (var > VAR_THRESHOLD) {
            bands.push_back({by, std::min(by + BAND_H, H), var});
        }
    }

    if (bands.empty()) return {};

    // Merge overlapping/adjacent bands into text regions
    std::vector<Quad> quads;
    int regionStart = bands[0].y0;
    int regionEnd   = bands[0].y1;

    auto emitQuad = [&](int y0, int y1) {
        // Find horizontal extent by scanning for columns with variance
        int xMin = W, xMax = 0;
        for (int y = y0; y < y1; ++y)
            for (int x = 0; x < W; ++x) {
                // Look for non-uniform columns
                if (x > 0 && std::abs(static_cast<int>(gray[y*W + x]) -
                                      static_cast<int>(gray[y*W + x-1])) > 30) {
                    xMin = std::min(xMin, x - 1);
                    xMax = std::max(xMax, x);
                }
            }
        if (xMin >= xMax) { xMin = W / 6; xMax = W * 5 / 6; }

        // Add some padding
        int pad = 4;
        xMin = std::max(0, xMin - pad);
        xMax = std::min(W - 1, xMax + pad);
        y0   = std::max(0, y0 - pad);
        y1   = std::min(H - 1, y1 + pad);

        Quad q;
        q.topLeft     = {static_cast<double>(xMin), static_cast<double>(y0)};
        q.topRight    = {static_cast<double>(xMax), static_cast<double>(y0)};
        q.bottomLeft  = {static_cast<double>(xMin), static_cast<double>(y1)};
        q.bottomRight = {static_cast<double>(xMax), static_cast<double>(y1)};
        quads.push_back(q);
    };

    for (size_t i = 1; i < bands.size(); ++i) {
        if (bands[i].y0 <= regionEnd + BAND_H) {
            regionEnd = std::max(regionEnd, bands[i].y1);
        } else {
            emitQuad(regionStart, regionEnd);
            regionStart = bands[i].y0;
            regionEnd   = bands[i].y1;
        }
    }
    emitQuad(regionStart, regionEnd);

    return quads;
}

// ═══════════════════════════════════════════════════════════════════
// MockTextRecognizer — deterministic placeholder
// ═══════════════════════════════════════════════════════════════════
// Returns "SAMPLE TEXT" with high confidence.  In a real backend this
// would run an OCR model.

RecognitionResult MockTextRecognizer::recognize(
    const FrameBuffer& /*frame*/, const Quad& /*region*/)
{
    return {"HELLO WORLD", "en", 0.95f};
}

// ═══════════════════════════════════════════════════════════════════
// MockTranslator — built-in phrase dictionary
// ═══════════════════════════════════════════════════════════════════

static std::string toUpper(const std::string& s) {
    std::string out = s;
    for (auto& c : out) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return out;
}

const std::unordered_map<std::string, std::string>& MockTranslator::dictionary() {
    static const std::unordered_map<std::string, std::string> dict = {
        // English → Spanish
        {"en:es:EXIT",           "SALIDA"},
        {"en:es:ENTER",          "ENTRAR"},
        {"en:es:ENTRANCE",       "ENTRADA"},
        {"en:es:STOP",           "ALTO"},
        {"en:es:WELCOME",        "BIENVENIDOS"},
        {"en:es:OPEN",           "ABIERTO"},
        {"en:es:CLOSED",         "CERRADO"},
        {"en:es:DANGER",         "PELIGRO"},
        {"en:es:NO ENTRY",       "PROHIBIDO EL PASO"},
        {"en:es:PUSH",           "EMPUJAR"},
        {"en:es:PULL",           "JALAR"},
        {"en:es:HELLO WORLD",    "HOLA MUNDO"},
        {"en:es:PARKING",        "ESTACIONAMIENTO"},
        {"en:es:RESTAURANT",     "RESTAURANTE"},
        {"en:es:HOTEL",          "HOTEL"},
        {"en:es:AIRPORT",        "AEROPUERTO"},
        {"en:es:HOSPITAL",       "HOSPITAL"},
        {"en:es:PHARMACY",       "FARMACIA"},
        {"en:es:TOILET",         "BANO"},
        {"en:es:RESTROOM",       "BANO"},
        {"en:es:CAUTION",        "PRECAUCION"},
        {"en:es:WARNING",        "ADVERTENCIA"},

        // English → German
        {"en:de:EXIT",           "AUSGANG"},
        {"en:de:ENTER",          "EINGANG"},
        {"en:de:ENTRANCE",       "EINGANG"},
        {"en:de:STOP",           "HALT"},
        {"en:de:WELCOME",        "WILLKOMMEN"},
        {"en:de:OPEN",           "OFFEN"},
        {"en:de:CLOSED",         "GESCHLOSSEN"},
        {"en:de:DANGER",         "GEFAHR"},
        {"en:de:NO ENTRY",       "KEIN EINTRITT"},
        {"en:de:PUSH",           "DRUECKEN"},
        {"en:de:PULL",           "ZIEHEN"},
        {"en:de:HELLO WORLD",    "HALLO WELT"},
        {"en:de:PARKING",        "PARKPLATZ"},
        {"en:de:RESTAURANT",     "RESTAURANT"},
        {"en:de:HOTEL",          "HOTEL"},
        {"en:de:AIRPORT",        "FLUGHAFEN"},
        {"en:de:HOSPITAL",       "KRANKENHAUS"},
        {"en:de:PHARMACY",       "APOTHEKE"},
        {"en:de:CAUTION",        "VORSICHT"},

        // English → French
        {"en:fr:EXIT",           "SORTIE"},
        {"en:fr:ENTER",          "ENTREE"},
        {"en:fr:ENTRANCE",       "ENTREE"},
        {"en:fr:STOP",           "ARRET"},
        {"en:fr:WELCOME",        "BIENVENUE"},
        {"en:fr:OPEN",           "OUVERT"},
        {"en:fr:CLOSED",         "FERME"},
        {"en:fr:DANGER",         "DANGER"},
        {"en:fr:NO ENTRY",       "ACCES INTERDIT"},
        {"en:fr:PUSH",           "POUSSER"},
        {"en:fr:PULL",           "TIRER"},
        {"en:fr:HELLO WORLD",    "BONJOUR LE MONDE"},
        {"en:fr:PARKING",        "STATIONNEMENT"},
        {"en:fr:RESTAURANT",     "RESTAURANT"},
        {"en:fr:HOTEL",          "HOTEL"},
        {"en:fr:AIRPORT",        "AEROPORT"},
        {"en:fr:HOSPITAL",       "HOPITAL"},
        {"en:fr:PHARMACY",       "PHARMACIE"},

        // English → Japanese (Romanised — the bitmap font can't render kanji)
        {"en:ja:EXIT",           "DEGUCHI"},
        {"en:ja:ENTER",          "IRIGUCHI"},
        {"en:ja:STOP",           "TOMARE"},
        {"en:ja:WELCOME",        "YOUKOSO"},
        {"en:ja:HELLO WORLD",    "KONNICHIWA"},
        {"en:ja:DANGER",         "KIKEN"},

        // English → Hindi (Romanised)
        {"en:hi:EXIT",           "NIKASH"},
        {"en:hi:ENTER",          "PRAVESH"},
        {"en:hi:STOP",           "RUKO"},
        {"en:hi:WELCOME",        "SWAGAT"},
        {"en:hi:HELLO WORLD",    "NAMASTE DUNIYA"},
        {"en:hi:DANGER",         "KHATRAA"},

        // English → Tamil (Romanised)
        {"en:ta:EXIT",           "VELIYERU"},
        {"en:ta:ENTER",          "NUZHAIVAASAL"},
        {"en:ta:STOP",           "NIRUTTHU"},
        {"en:ta:WELCOME",        "VARAVETRPU"},
        {"en:ta:HELLO WORLD",    "VANAKKAM ULAGAM"},
        {"en:ta:DANGER",         "AABATHU"},
    };
    return dict;
}

TranslationResult MockTranslator::translate(
    const std::string& text,
    const std::string& sourceLanguage,
    const std::string& targetLanguage)
{
    TranslationResult result;
    result.originalText  = text;
    result.sourceLanguage = sourceLanguage;
    result.targetLanguage = targetLanguage;

    if (sourceLanguage == targetLanguage) {
        result.translatedText = text;
        return result;
    }

    std::string key = sourceLanguage + ":" + targetLanguage + ":" + toUpper(text);
    auto it = dictionary().find(key);
    if (it != dictionary().end()) {
        result.translatedText = it->second;
    } else {
        // Fallback: return original text with a [lang] tag
        result.translatedText = "[" + targetLanguage + "] " + text;
    }

    return result;
}

// ═══════════════════════════════════════════════════════════════════
// MockInpainter — wraps core StyleAnalyzer + Inpainter
// ═══════════════════════════════════════════════════════════════════

void MockInpainter::inpaint(FrameBuffer& frame, const Quad& region) {
    StyleInfo style = m_styleAnalyzer.analyze(frame, region);
    m_inpainter.inpaint(frame, region, style.foreground, style.background);
}

} // namespace lingolens

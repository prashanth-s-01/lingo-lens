#pragma once

#include "types.h"
#include <string>
#include <vector>

namespace lingolens {

class ITextDetector {
public:
  virtual ~ITextDetector() = default;
  virtual std::vector<Quad> detect(const FrameBuffer &frame) = 0;
};

class ITextRecognizer {
public:
  virtual ~ITextRecognizer() = default;
  virtual RecognitionResult recognize(const FrameBuffer &frame,
                                      const Quad &region) = 0;
};

class ITranslator {
public:
  virtual ~ITranslator() = default;
  virtual TranslationResult translate(const std::string &text,
                                      const std::string &sourceLanguage,
                                      const std::string &targetLanguage) = 0;
};

class IInpainter {
public:
  virtual ~IInpainter() = default;
  virtual void inpaint(FrameBuffer &frame, const Quad &region) = 0;
};

} // namespace lingolens

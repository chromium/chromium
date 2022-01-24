// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/lottie/resource.h"

#include <memory>
#include <vector>

#include "base/memory/ref_counted_memory.h"
#include "cc/paint/skottie_wrapper.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image_skia_rep_default.h"
#include "ui/lottie/animation.h"

namespace {

// A descendant of |gfx::ImageSkiaSource| that uses a |lottie::Animation| for a
// still image. Used as a utility class, not for |gfx::ImageSkia|'s backend.
class LottieImageSource : public gfx::CanvasImageSource {
 public:
  LottieImageSource(std::unique_ptr<lottie::Animation> content,
                    const gfx::Size& size)
      : gfx::CanvasImageSource(size), content_(std::move(content)) {
    DCHECK(content_);
    DCHECK(content_->skottie());
    DCHECK(content_->skottie()->is_valid());
  }
  LottieImageSource(const LottieImageSource&) = delete;
  LottieImageSource& operator=(const LottieImageSource&) = delete;
  ~LottieImageSource() override = default;

  // gfx::CanvasImageSource:
  bool HasRepresentationAtAllScales() const override { return true; }
  void Draw(gfx::Canvas* canvas) override {
    content_->PaintFrame(canvas, 0.f, size_);
  }

 private:
  std::unique_ptr<lottie::Animation> content_;
};

}  // namespace

namespace lottie {

gfx::ImageSkiaRep ParseLottieAsStillImage(
    const base::RefCountedString& bytes_string,
    float scale) {
  const uint8_t* bytes_pointer = bytes_string.front_as<uint8_t>();
  std::unique_ptr<lottie::Animation> content =
      std::make_unique<lottie::Animation>(
          cc::SkottieWrapper::CreateSerializable(std::vector<uint8_t>(
              bytes_pointer, bytes_pointer + bytes_string.size())));
  const gfx::Size size = content->GetOriginalSize();
  return LottieImageSource(std::move(content), size).GetImageForScale(scale);
}

}  // namespace lottie

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/button/single_animated_image_container.h"

#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "cc/paint/paint_flags.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/lottie/animation.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/image_view.h"

namespace {

class LottieIconSource : public gfx::CanvasImageSource {
 public:
  LottieIconSource(lottie::Animation* animation,
                   float progress,
                   int size,
                   SkColor color)
      : gfx::CanvasImageSource(gfx::Size(size, size)),
        animation_(animation),
        progress_(progress),
        color_(color) {}
  LottieIconSource(const LottieIconSource&) = delete;
  LottieIconSource& operator=(const LottieIconSource&) = delete;
  ~LottieIconSource() override = default;

  // gfx::CanvasImageSource:
  void Draw(gfx::Canvas* canvas) override {
    if (!animation_) {
      return;
    }

    // TODO(b/517231960): Use SkottieColorMap to set the colors in the
    // lottie animation itself, instead of using a color filter.
    cc::PaintFlags flags;
    flags.setColorFilter(cc::ColorFilter::MakeBlend(
        SkColor4f::FromColor(color_), SkBlendMode::kSrcIn));
    canvas->SaveLayerWithFlags(flags);
    animation_->PaintFrame(canvas, progress_, size());
    canvas->Restore();
  }

 private:
  raw_ptr<lottie::Animation> animation_;
  const float progress_;
  const SkColor color_;
};

}  // namespace

namespace views {

SingleAnimatedImageContainer::SingleAnimatedImageContainer(
    LabelButton* button,
    base::TimeDelta animation_duration)
    : button_(button), slide_animation_(this) {
  if (animation_duration.is_positive()) {
    slide_animation_.SetSlideDuration(animation_duration);
  }
}

SingleAnimatedImageContainer::~SingleAnimatedImageContainer() = default;

void SingleAnimatedImageContainer::SetAnimatedImage(int lottie_resource_id,
                                                    SkColor color) {
  std::optional<std::vector<uint8_t>> lottie_bytes =
      ui::ResourceBundle::GetSharedInstance().GetLottieData(lottie_resource_id);
  CHECK(lottie_bytes.has_value());
  scoped_refptr<cc::SkottieWrapper> skottie =
      cc::SkottieWrapper::UnsafeCreateSerializable(std::move(*lottie_bytes));
  animated_image_ = std::make_unique<lottie::Animation>(skottie);
  color_ = color;
}

void SingleAnimatedImageContainer::ClearAnimatedImage() {
  animated_image_.reset();
}

void SingleAnimatedImageContainer::UpdateImage(const LabelButton* button) {
  if (slide_animation_.GetCurrentValue() == 0.0f) {
    SingleImageContainer::UpdateImage(button);
  }
}

void SingleAnimatedImageContainer::ShowAnimation() {
  slide_animation_.Reset(0.0f);
  slide_animation_.Show();
}

void SingleAnimatedImageContainer::HideAnimation() {
  slide_animation_.Hide();
}

void SingleAnimatedImageContainer::ResetAnimation() {
  slide_animation_.Reset(0.0f);
}

void SingleAnimatedImageContainer::AnimationProgressed(
    const gfx::Animation* animation) {
  ImageView* image_view = static_cast<ImageView*>(GetView());
  if (!image_view) {
    return;
  }
  const gfx::Size& size = image_view->size();

  ui::ImageModel model = ui::ImageModel::FromImageSkia(
      gfx::CanvasImageSource::MakeImageSkia<LottieIconSource>(
          animated_image_.get(), slide_animation_.GetCurrentValue(),
          size.width(), color_));

  image_view->SetImage(model);
}

void SingleAnimatedImageContainer::AnimationEnded(
    const gfx::Animation* animation) {
  if (slide_animation_.GetCurrentValue() == 0.0f) {
    UpdateImage(button_);
  }
}

}  // namespace views

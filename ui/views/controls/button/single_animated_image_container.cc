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
  // Lottie animations have easing function embedded into them,
  // so we use a linear tween for the slide animation.
  slide_animation_.SetTweenType(gfx::Tween::LINEAR);
  if (animation_duration.is_positive()) {
    slide_animation_.SetSlideDuration(animation_duration);
  }
}

SingleAnimatedImageContainer::~SingleAnimatedImageContainer() {
  // The image in the image_view may still contain a reference to the
  // lottie animation owned by this class. We need to clear the ImageModel
  // before the lottie animation is deleted.
  if (ImageView* image_view = static_cast<ImageView*>(GetView()); image_view) {
    image_view->SetImage(ui::ImageModel());
  }
}

void SingleAnimatedImageContainer::AddAnimatedImage(int resource_id) {
  if (HasAnimatedImage(resource_id)) {
    return;
  }

  animated_images_.emplace(resource_id, LoadAnimatedImage(resource_id));
}

std::unique_ptr<lottie::Animation>
SingleAnimatedImageContainer::LoadAnimatedImage(int resource_id) {
  std::optional<std::vector<uint8_t>> lottie_bytes =
      ui::ResourceBundle::GetSharedInstance().GetLottieData(resource_id);
  CHECK(lottie_bytes.has_value());
  scoped_refptr<cc::SkottieWrapper> skottie =
      cc::SkottieWrapper::UnsafeCreateSerializable(std::move(*lottie_bytes));
  return std::make_unique<lottie::Animation>(skottie);
}

void SingleAnimatedImageContainer::ClearAnimatedImages() {
  ResetAnimation();
  animated_images_.clear();
}

void SingleAnimatedImageContainer::UpdateImage(const LabelButton* button) {
  // In order to update back to static image, we shouldn't be showing animation.
  if (!IsShowingAnimation()) {
    SingleImageContainer::UpdateImage(button);
  }
}

bool SingleAnimatedImageContainer::HasAnimatedImage(int resource_id) const {
  return animated_images_.contains(resource_id);
}

bool SingleAnimatedImageContainer::IsShowingAnimation() const {
  // Showing animation state includes paused state at the end of a forward
  // animation.
  return slide_animation_.is_animating() ||
         slide_animation_.GetCurrentValue() != 0.0f;
}

void SingleAnimatedImageContainer::PlayAnimation(AnimationDefinition definition,
                                                 AnimationConfig config) {
  if (config.direction == AnimationDirection::kForward) {
    slide_animation_.Reset(0.0f);
    AddAnimatedImage(definition.resource_id);
    playing_animation_ =
        std::make_optional<AnimationState>({definition, config.end_behavior});
    slide_animation_.Show();
  } else {
    CHECK(config.direction == AnimationDirection::kBackward);
    CHECK(config.end_behavior == AnimationEndBehavior::kReset);
    slide_animation_.Reset(1.0f);
    AddAnimatedImage(definition.resource_id);
    playing_animation_ =
        std::make_optional<AnimationState>({definition, config.end_behavior});
    slide_animation_.Hide();
  }
}

void SingleAnimatedImageContainer::ResetAnimation() {
  if (slide_animation_.GetCurrentValue() != 0.0f) {
    slide_animation_.Reset(0.0f);
  }
}

void SingleAnimatedImageContainer::AnimationProgressed(
    const gfx::Animation* animation) {
  CHECK(playing_animation_);

  ImageView* image_view = static_cast<ImageView*>(GetView());
  if (!image_view) {
    return;
  }
  const gfx::Size& size = image_view->size();

  ui::ImageModel model = ui::ImageModel::FromImageSkia(
      gfx::CanvasImageSource::MakeImageSkia<LottieIconSource>(
          animated_images_[playing_animation_->definition.resource_id].get(),
          slide_animation_.GetCurrentValue(), size.width(),
          playing_animation_->definition.color));

  image_view->SetImage(model);
}

void SingleAnimatedImageContainer::AnimationEnded(
    const gfx::Animation* animation) {
  CHECK(playing_animation_);

  if (playing_animation_->end_behavior == AnimationEndBehavior::kReset) {
    ResetAnimation();
    UpdateImage(button_);
  }

  playing_animation_.reset();
}

}  // namespace views

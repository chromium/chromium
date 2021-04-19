// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/animation_example.h"

#include <algorithm>
#include <memory>
#include <numeric>

#include "base/check.h"
#include "base/containers/circular_deque.h"
#include "base/numerics/ranges.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/layer_animator.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/views/background.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

namespace views {
namespace examples {
namespace {

class Animation : public gfx::Animation {
 public:
  Animation(gfx::AnimationDelegate* delegate,
            gfx::Point current_position,
            gfx::Point target_position);
  Animation(const Animation&) = delete;
  Animation& operator=(const Animation&) = delete;
  ~Animation() override = default;

  // gfx::Animation:
  double GetCurrentValue() const override;

  gfx::Vector2dF GetOffsetFromCurrentAnimationStart() const;
  void NewAnimationStarted();

 protected:
  bool ShouldSendCanceledFromStop() override;
  void Step(base::TimeTicks time_now) override;

 private:
  gfx::Vector2dF delta_;
  double t_ = 0;
  base::TimeDelta total_time_ = base::TimeDelta::FromSeconds(1);
  bool decelerating_ = false;
};

Animation::Animation(gfx::AnimationDelegate* delegate,
                     gfx::Point current_position,
                     gfx::Point target_position)
    : gfx::Animation(base::TimeDelta::FromHz(60)),
      delta_(target_position - current_position) {
  set_delegate(delegate);
  Start();
}

double Animation::GetCurrentValue() const {
  return decelerating_ ? (t_ * (1 - t_)) : t_;
}

gfx::Vector2dF Animation::GetOffsetFromCurrentAnimationStart() const {
  return ScaleVector2d(delta_, GetCurrentValue());
}

void Animation::NewAnimationStarted() {
  Stop();
  delta_ -= GetOffsetFromCurrentAnimationStart();
  t_ = 0;
  total_time_ -= base::TimeTicks::Now() - start_time();
  decelerating_ = true;
  Start();
}

bool Animation::ShouldSendCanceledFromStop() {
  return t_ != 1;
}

void Animation::Step(base::TimeTicks time_now) {
  const base::TimeDelta elapsed_time =
      std::min(time_now - start_time(), total_time_);
  t_ = gfx::Tween::CalculateValue(gfx::Tween::EASE_OUT,
                                  elapsed_time / total_time_);
  delegate()->AnimationProgressed(this);
  if (t_ == 1)
    Stop();
}

}  // namespace

AnimationExample::AnimationExample() : ExampleBase("Animation") {}

AnimationExample::~AnimationExample() = default;

class AnimatingSquare : public View,
                        public gfx::AnimationDelegate,
                        public ViewObserver {
 public:
  AnimatingSquare(size_t index, View* parent);
  AnimatingSquare(const AnimatingSquare&) = delete;
  AnimatingSquare& operator=(const AnimatingSquare&) = delete;
  ~AnimatingSquare() override = default;

  // gfx::AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;

  // ViewObserver:
  void OnViewBoundsChanged(View* observed_view) override;

 private:
  gfx::Point ComputeTargetPosition(const View* parent) const;

  static constexpr int kPadding = 25;
  static constexpr gfx::Size kSize = gfx::Size(100, 100);

  size_t index_;
  base::ScopedObservation<View, ViewObserver> view_observation_{this};
  gfx::Point start_position_, target_position_;
  base::circular_deque<std::unique_ptr<Animation>> animations_;
};

// static
constexpr gfx::Size AnimatingSquare::kSize;

AnimatingSquare::AnimatingSquare(size_t index, View* parent)
    : index_(index), target_position_(ComputeTargetPosition(parent)) {
  SetBackground(
      CreateSolidBackground(SkColorSetRGB((5 - index_) * 51, 0, index_ * 51)));
  view_observation_.Observe(parent);
  SetBoundsRect({target_position_, kSize});

  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  auto opacity_sequence = std::make_unique<ui::LayerAnimationSequence>();
  opacity_sequence->set_is_repeating(true);
  opacity_sequence->AddElement(ui::LayerAnimationElement::CreateOpacityElement(
      0.4f, base::TimeDelta::FromSeconds(2)));
  opacity_sequence->AddElement(ui::LayerAnimationElement::CreateOpacityElement(
      0.9f, base::TimeDelta::FromSeconds(2)));
  layer()->GetAnimator()->StartAnimation(opacity_sequence.release());
}

void AnimatingSquare::OnViewBoundsChanged(View* observed_view) {
  const gfx::Point target_position = ComputeTargetPosition(observed_view);
  if (target_position_ == target_position)
    return;
  start_position_ = origin();
  target_position_ = target_position;
  for (const auto& animation : animations_)
    animation->NewAnimationStarted();
  animations_.push_back(
      std::make_unique<Animation>(this, start_position_, target_position_));
}

void AnimatingSquare::AnimationProgressed(const gfx::Animation* animation) {
  gfx::PointF position(start_position_);
  position = std::accumulate(
      animations_.cbegin(), animations_.cend(), position,
      [](gfx::PointF p, const std::unique_ptr<Animation>& animation) {
        return p + animation->GetOffsetFromCurrentAnimationStart();
      });
  SetPosition(gfx::ToRoundedPoint(position));
}

void AnimatingSquare::AnimationEnded(const gfx::Animation* animation) {
  DCHECK_EQ(animation, animations_.front().get());
  animations_.pop_front();
}

gfx::Point AnimatingSquare::ComputeTargetPosition(const View* parent) const {
  const int views_per_row = base::ClampToRange(
      (parent->width() - kPadding) / (kPadding + kSize.width()), 1, 5);
  const size_t row = index_ / views_per_row;
  const size_t column = index_ % views_per_row;
  return {kPadding + column * (kPadding + kSize.width()),
          kPadding + row * (kPadding + kSize.height())};
}

void AnimationExample::CreateExampleView(View* container) {
  for (size_t i = 0; i < 5; ++i)
    container->AddChildView(std::make_unique<AnimatingSquare>(i, container));
}

}  // namespace examples
}  // namespace views

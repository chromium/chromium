// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/layer_animation_element.h"

#include <utility>

#include "base/compiler_specific.h"
#include "base/strings/stringprintf.h"
#include "cc/animation/animation_id_provider.h"
#include "cc/animation/keyframe_model.h"
#include "ui/compositor/float_animation_curve_adapter.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_delegate.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/transform_animation_curve_adapter.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/interpolated_transform.h"

namespace ui {

namespace {

// Pause -----------------------------------------------------------------------
class Pause : public LayerAnimationElement {
 public:
  Pause(AnimatableProperties properties, base::TimeDelta duration)
      : LayerAnimationElement(properties, duration) {
  }

  Pause(const Pause&) = delete;
  Pause& operator=(const Pause&) = delete;

  ~Pause() override {}

 private:
  std::string DebugName() const override { return "Pause"; }
  void OnStart(LayerAnimationDelegate* delegate) override {}
  bool OnProgress(double t, LayerAnimationDelegate* delegate) override {
    return false;
  }
  void OnGetTarget(TargetValue* target) const override {}
  void OnAbort(LayerAnimationDelegate* delegate) override {}
};

// InterpolatedTransformTransition ---------------------------------------------

class InterpolatedTransformTransition : public LayerAnimationElement {
 public:
  InterpolatedTransformTransition(
      std::unique_ptr<InterpolatedTransform> interpolated_transform,
      base::TimeDelta duration)
      : LayerAnimationElement(TRANSFORM, duration),
        interpolated_transform_(std::move(interpolated_transform)) {}

  InterpolatedTransformTransition(const InterpolatedTransformTransition&) =
      delete;
  InterpolatedTransformTransition& operator=(
      const InterpolatedTransformTransition&) = delete;

  ~InterpolatedTransformTransition() override {}

 protected:
  std::string DebugName() const override {
    return "InterpolatedTransformTransition";
  }
  void OnStart(LayerAnimationDelegate* delegate) override {}

  bool OnProgress(double t, LayerAnimationDelegate* delegate) override {
    delegate->SetTransformFromAnimation(
        interpolated_transform_->Interpolate(static_cast<float>(t)),
        PropertyChangeReason::FROM_ANIMATION);
    return true;
  }

  void OnGetTarget(TargetValue* target) const override {
    target->transform = interpolated_transform_->Interpolate(1.0f);
  }

  void OnAbort(LayerAnimationDelegate* delegate) override {}

 private:
  std::unique_ptr<InterpolatedTransform> interpolated_transform_;
};

// BoundsTransition ------------------------------------------------------------

class BoundsTransition : public LayerAnimationElement {
 public:
  BoundsTransition(const gfx::Rect& target, base::TimeDelta duration)
      : LayerAnimationElement(BOUNDS, duration),
        target_(target) {
  }

  BoundsTransition(const BoundsTransition&) = delete;
  BoundsTransition& operator=(const BoundsTransition&) = delete;

  ~BoundsTransition() override {}

 protected:
  std::string DebugName() const override { return "BoundsTransition"; }
  void OnStart(LayerAnimationDelegate* delegate) override {
    start_ = delegate->GetBoundsForAnimation();
  }

  bool OnProgress(double t, LayerAnimationDelegate* delegate) override {
    delegate->SetBoundsFromAnimation(
        gfx::Tween::RectValueBetween(t, start_, target_),
        PropertyChangeReason::FROM_ANIMATION);
    return true;
  }

  void OnGetTarget(TargetValue* target) const override {
    target->bounds = target_;
  }

  void OnAbort(LayerAnimationDelegate* delegate) override {}

 private:
  gfx::Rect start_;
  const gfx::Rect target_;
};

// VisibilityTransition --------------------------------------------------------

class VisibilityTransition : public LayerAnimationElement {
 public:
  VisibilityTransition(bool target, base::TimeDelta duration)
      : LayerAnimationElement(VISIBILITY, duration),
        start_(false),
        target_(target) {
  }

  VisibilityTransition(const VisibilityTransition&) = delete;
  VisibilityTransition& operator=(const VisibilityTransition&) = delete;

  ~VisibilityTransition() override {}

 protected:
  std::string DebugName() const override { return "VisibilityTransition"; }
  void OnStart(LayerAnimationDelegate* delegate) override {
    start_ = delegate->GetVisibilityForAnimation();
  }

  bool OnProgress(double t, LayerAnimationDelegate* delegate) override {
    delegate->SetVisibilityFromAnimation(t == 1.0 ? target_ : start_,
                                         PropertyChangeReason::FROM_ANIMATION);
    return t == 1.0;
  }

  void OnGetTarget(TargetValue* target) const override {
    target->visibility = target_;
  }

  void OnAbort(LayerAnimationDelegate* delegate) override {}

 private:
  bool start_;
  const bool target_;
};

// BrightnessTransition --------------------------------------------------------

class BrightnessTransition : public LayerAnimationElement {
 public:
  BrightnessTransition(float target, base::TimeDelta duration)
      : LayerAnimationElement(BRIGHTNESS, duration),
        start_(0.0f),
        target_(target) {
  }

  BrightnessTransition(const BrightnessTransition&) = delete;
  BrightnessTransition& operator=(const BrightnessTransition&) = delete;

  ~BrightnessTransition() override {}

 protected:
  std::string DebugName() const override { return "BrightnessTransition"; }
  void OnStart(LayerAnimationDelegate* delegate) override {
    start_ = delegate->GetBrightnessForAnimation();
  }

  bool OnProgress(double t, LayerAnimationDelegate* delegate) override {
    delegate->SetBrightnessFromAnimation(
        gfx::Tween::FloatValueBetween(t, start_, target_),
        PropertyChangeReason::FROM_ANIMATION);
    return true;
  }

  void OnGetTarget(TargetValue* target) const override {
    target->brightness = target_;
  }

  void OnAbort(LayerAnimationDelegate* delegate) override {}

 private:
  float start_;
  const float target_;
};

// GrayscaleTransition ---------------------------------------------------------

class GrayscaleTransition : public LayerAnimationElement {
 public:
  GrayscaleTransition(float target, base::TimeDelta duration)
      : LayerAnimationElement(GRAYSCALE, duration),
        start_(0.0f),
        target_(target) {
  }

  GrayscaleTransition(const GrayscaleTransition&) = delete;
  GrayscaleTransition& operator=(const GrayscaleTransition&) = delete;

  ~GrayscaleTransition() override {}

 protected:
  std::string DebugName() const override { return "GrayscaleTransition"; }
  void OnStart(LayerAnimationDelegate* delegate) override {
    start_ = delegate->GetGrayscaleForAnimation();
  }

  bool OnProgress(double t, LayerAnimationDelegate* delegate) override {
    delegate->SetGrayscaleFromAnimation(
        gfx::Tween::FloatValueBetween(t, start_, target_),
        PropertyChangeReason::FROM_ANIMATION);
    return true;
  }

  void OnGetTarget(TargetValue* target) const override {
    target->grayscale = target_;
  }

  void OnAbort(LayerAnimationDelegate* delegate) override {}

 private:
  float start_;
  const float target_;
};

// ColorTransition -------------------------------------------------------------

class ColorTransition : public LayerAnimationElement {
 public:
  ColorTransition(SkColor target, base::TimeDelta duration)
      : LayerAnimationElement(COLOR, duration),
        start_(SK_ColorBLACK),
        target_(target) {
  }

  ColorTransition(const ColorTransition&) = delete;
  ColorTransition& operator=(const ColorTransition&) = delete;

  ~ColorTransition() override {}

 protected:
  std::string DebugName() const override { return "ColorTransition"; }
  void OnStart(LayerAnimationDelegate* delegate) override {
    start_ = delegate->GetColorForAnimation();
  }

  bool OnProgress(double t, LayerAnimationDelegate* delegate) override {
    delegate->SetColorFromAnimation(
        gfx::Tween::ColorValueBetween(t, start_, target_),
        PropertyChangeReason::FROM_ANIMATION);
    return true;
  }

  void OnGetTarget(TargetValue* target) const override {
    target->color = target_;
  }

  void OnAbort(LayerAnimationDelegate* delegate) override {}

 private:
  SkColor start_;
  const SkColor target_;
};

// ClipRectTransition ----------------------------------------------------------

class ClipRectTransition : public LayerAnimationElement {
 public:
  ClipRectTransition(const gfx::Rect& target, base::TimeDelta duration)
      : LayerAnimationElement(CLIP, duration), target_(target) {}

  ClipRectTransition(const ClipRectTransition&) = delete;
  ClipRectTransition& operator=(const ClipRectTransition&) = delete;

  ~ClipRectTransition() override {}

 protected:
  std::string DebugName() const override { return "ClipRectTransition"; }
  void OnStart(LayerAnimationDelegate* delegate) override {
    start_ = delegate->GetClipRectForAnimation();
  }

  bool OnProgress(double t, LayerAnimationDelegate* delegate) override {
    delegate->SetClipRectFromAnimation(
        gfx::Tween::RectValueBetween(t, start_, target_),
        PropertyChangeReason::FROM_ANIMATION);
    return true;
  }

  void OnGetTarget(TargetValue* target) const override {
    target->clip_rect = target_;
  }

  void OnAbort(LayerAnimationDelegate* delegate) override {}

 private:
  gfx::Rect start_;
  const gfx::Rect target_;
};

// RoundedCornersTransition ----------------------------------------------------

class RoundedCornersTransition : public LayerAnimationElement {
 public:
  RoundedCornersTransition(const gfx::RoundedCornersF& target,
                           base::TimeDelta duration)
      : LayerAnimationElement(ROUNDED_CORNERS, duration), target_(target) {}

  RoundedCornersTransition(const RoundedCornersTransition&) = delete;
  RoundedCornersTransition& operator=(const RoundedCornersTransition&) = delete;

  ~RoundedCornersTransition() override = default;

 protected:
  std::string DebugName() const override { return "RoundedCornersTransition"; }
  void OnStart(LayerAnimationDelegate* delegate) override {
    start_ = delegate->GetRoundedCornersForAnimation();
  }

  bool OnProgress(double t, LayerAnimationDelegate* delegate) override {
    delegate->SetRoundedCornersFromAnimation(
        gfx::RoundedCornersF(
            gfx::Tween::FloatValueBetween(t, start_.upper_left(),
                                          target_.upper_left()),
            gfx::Tween::FloatValueBetween(t, start_.upper_right(),
                                          target_.upper_right()),
            gfx::Tween::FloatValueBetween(t, start_.lower_right(),
                                          target_.lower_right()),
            gfx::Tween::FloatValueBetween(t, start_.lower_left(),
                                          target_.lower_left())),
        PropertyChangeReason::FROM_ANIMATION);
    return true;
  }

  void OnGetTarget(TargetValue* target) const override {
    target->rounded_corners = target_;
  }

  void OnAbort(LayerAnimationDelegate* delegate) override {}

 private:
  gfx::RoundedCornersF start_;
  gfx::RoundedCornersF target_;
};

// GradientMaskTransition ----------------------------------------------------

class GradientMaskTransition : public LayerAnimationElement {
 public:
  GradientMaskTransition(const gfx::LinearGradient& target,
                         base::TimeDelta duration)
      : LayerAnimationElement(GRADIENT_MASK, duration), target_(target) {}

  GradientMaskTransition(const GradientMaskTransition&) = delete;
  GradientMaskTransition& operator=(const GradientMaskTransition&) = delete;

  ~GradientMaskTransition() override = default;

 protected:
  std::string DebugName() const override { return "GradientMaskTransition"; }
  void OnStart(LayerAnimationDelegate* delegate) override {
    start_ = delegate->GetGradientMaskForAnimation();
  }

  bool OnProgress(double t, LayerAnimationDelegate* delegate) override {
    auto gradient_mask = gfx::LinearGradient(
        gfx::Tween::FloatValueBetween(t, start_.angle(), target_.angle()));

    DCHECK_EQ(start_.step_count(), target_.step_count());
    for (auto i = 0; i < static_cast<int>(start_.step_count()); ++i) {
      gradient_mask.AddStep(
          gfx::Tween::FloatValueBetween(t, start_.steps()[i].fraction,
                                        target_.steps()[i].fraction),
          gfx::Tween::IntValueBetween(t, start_.steps()[i].alpha,
                                      target_.steps()[i].alpha));
    }

    delegate->SetGradientMaskFromAnimation(
        gradient_mask, PropertyChangeReason::FROM_ANIMATION);
    return true;
  }

  void OnGetTarget(TargetValue* target) const override {
    target->gradient_mask = target_;
  }

  void OnAbort(LayerAnimationDelegate* delegate) override {}

 private:
  gfx::LinearGradient start_;
  const gfx::LinearGradient target_;
};

// ThreadedLayerAnimationElement -----------------------------------------------

class ThreadedLayerAnimationElement : public LayerAnimationElement {
 public:
  ThreadedLayerAnimationElement(AnimatableProperties properties,
                                base::TimeDelta duration)
      : LayerAnimationElement(properties, duration) {
  }

  ThreadedLayerAnimationElement(const ThreadedLayerAnimationElement&) = delete;
  ThreadedLayerAnimationElement& operator=(
      const ThreadedLayerAnimationElement&) = delete;

  ~ThreadedLayerAnimationElement() override {}

  bool IsThreaded(LayerAnimationDelegate* delegate) const override {
    return !duration().is_zero();
  }

 protected:
  explicit ThreadedLayerAnimationElement(const LayerAnimationElement& element)
    : LayerAnimationElement(element) {
  }
  std::string DebugName() const override {
    return "ThreadedLayerAnimationElement";
  }

  bool OnProgress(double t, LayerAnimationDelegate* delegate) override {
    if (t < 1.0)
      return false;

    if (Started() && IsThreaded(delegate)) {
      LayerThreadedAnimationDelegate* threaded =
          delegate->GetThreadedAnimationDelegate();
      DCHECK(threaded);
      threaded->RemoveThreadedAnimation(keyframe_model_id());
    }

    OnEnd(delegate);
    return true;
  }

  void OnAbort(LayerAnimationDelegate* delegate) override {
    if (delegate && Started() && IsThreaded(delegate)) {
      LayerThreadedAnimationDelegate* threaded =
          delegate->GetThreadedAnimationDelegate();
      DCHECK(threaded);
      threaded->RemoveThreadedAnimation(keyframe_model_id());
    }
  }

  void RequestEffectiveStart(LayerAnimationDelegate* delegate) override {
    DCHECK(animation_group_id());
    if (!IsThreaded(delegate)) {
      set_effective_start_time(requested_start_time());
      return;
    }
    set_effective_start_time(base::TimeTicks());
    std::unique_ptr<cc::KeyframeModel> keyframe_model = CreateCCKeyframeModel();
    keyframe_model->set_needs_synchronized_start_time(true);

    LayerThreadedAnimationDelegate* threaded =
        delegate->GetThreadedAnimationDelegate();
    DCHECK(threaded);
    threaded->AddThreadedAnimation(std::move(keyframe_model));
  }

  virtual void OnEnd(LayerAnimationDelegate* delegate) = 0;

  virtual std::unique_ptr<cc::KeyframeModel> CreateCCKeyframeModel() = 0;
};

// ThreadedOpacityTransition ---------------------------------------------------

class ThreadedOpacityTransition : public ThreadedLayerAnimationElement {
 public:
  ThreadedOpacityTransition(float target, base::TimeDelta duration)
      : ThreadedLayerAnimationElement(OPACITY, duration),
        start_(0.0f),
        target_(target) {
  }

  ThreadedOpacityTransition(const ThreadedOpacityTransition&) = delete;
  ThreadedOpacityTransition& operator=(const ThreadedOpacityTransition&) =
      delete;

  ~ThreadedOpacityTransition() override {}

 protected:
  std::string DebugName() const override { return "ThreadedOpacityTransition"; }
  void OnStart(LayerAnimationDelegate* delegate) override {
    start_ = delegate->GetOpacityForAnimation();
    delegate->SetOpacityFromAnimation(delegate->GetOpacityForAnimation(),
                                      PropertyChangeReason::FROM_ANIMATION);
  }

  void OnAbort(LayerAnimationDelegate* delegate) override {
    if (delegate && Started()) {
      ThreadedLayerAnimationElement::OnAbort(delegate);
      delegate->SetOpacityFromAnimation(
          gfx::Tween::FloatValueBetween(
              gfx::Tween::CalculateValue(tween_type(),
                                         last_progressed_fraction()),
              start_, target_),
          PropertyChangeReason::FROM_ANIMATION);
    }
  }

  void OnEnd(LayerAnimationDelegate* delegate) override {
    delegate->SetOpacityFromAnimation(target_,
                                      PropertyChangeReason::FROM_ANIMATION);
  }

  std::unique_ptr<cc::KeyframeModel> CreateCCKeyframeModel() override {
    // Ensures that we don't remove and add a model with the same id in a single
    // frame.
    UpdateKeyframeModelId();
    std::unique_ptr<gfx::AnimationCurve> animation_curve(
        new FloatAnimationCurveAdapter(tween_type(), start_, target_,
                                       duration()));
    std::unique_ptr<cc::KeyframeModel> keyframe_model(cc::KeyframeModel::Create(
        std::move(animation_curve), keyframe_model_id(), animation_group_id(),
        cc::KeyframeModel::TargetPropertyId(cc::TargetProperty::OPACITY)));
    return keyframe_model;
  }

  void OnGetTarget(TargetValue* target) const override {
    target->opacity = target_;
  }

  bool IsThreaded(LayerAnimationDelegate* delegate) const override {
    // If the start and target values are the same, we do not create cc
    // animation so that we will not create render pass in this case.
    // http://crbug.com/764575.
    if (duration().is_zero())
      return false;
    if (Started())
      return start_ != target_;
    return delegate->GetOpacityForAnimation() != target_;
  }

 private:
  float start_;
  const float target_;
};

// ThreadedTransformTransition -------------------------------------------------

class ThreadedTransformTransition : public ThreadedLayerAnimationElement {
 public:
  ThreadedTransformTransition(const gfx::Transform& target,
                              base::TimeDelta duration)
      : ThreadedLayerAnimationElement(TRANSFORM, duration),
        target_(target) {
  }

  ThreadedTransformTransition(const ThreadedTransformTransition&) = delete;
  ThreadedTransformTransition& operator=(const ThreadedTransformTransition&) =
      delete;

  ~ThreadedTransformTransition() override {}

 protected:
  std::string DebugName() const override {
    return "ThreadedTransformTransition";
  }
  void OnStart(LayerAnimationDelegate* delegate) override {
    start_ = delegate->GetTransformForAnimation();
    delegate->SetTransformFromAnimation(delegate->GetTransformForAnimation(),
                                        PropertyChangeReason::FROM_ANIMATION);
  }

  void OnAbort(LayerAnimationDelegate* delegate) override {
    if (delegate && Started()) {
      ThreadedLayerAnimationElement::OnAbort(delegate);
      delegate->SetTransformFromAnimation(
          gfx::Tween::TransformValueBetween(
              gfx::Tween::CalculateValue(tween_type(),
                                         last_progressed_fraction()),
              start_, target_),
          PropertyChangeReason::FROM_ANIMATION);
    }
  }

  void OnEnd(LayerAnimationDelegate* delegate) override {
    delegate->SetTransformFromAnimation(target_,
                                        PropertyChangeReason::FROM_ANIMATION);
  }

  std::unique_ptr<cc::KeyframeModel> CreateCCKeyframeModel() override {
    // Ensures that we don't remove and add a model with the same id in a single
    // frame.
    UpdateKeyframeModelId();
    std::unique_ptr<gfx::AnimationCurve> animation_curve(
        new TransformAnimationCurveAdapter(tween_type(), start_, target_,
                                           duration()));
    std::unique_ptr<cc::KeyframeModel> keyframe_model(cc::KeyframeModel::Create(
        std::move(animation_curve), keyframe_model_id(), animation_group_id(),
        cc::KeyframeModel::TargetPropertyId(cc::TargetProperty::TRANSFORM)));
    return keyframe_model;
  }

  void OnGetTarget(TargetValue* target) const override {
    target->transform = target_;
  }

 private:
  gfx::Transform start_;
  const gfx::Transform target_;
};

}  // namespace

// LayerAnimationElement::TargetValue ------------------------------------------

LayerAnimationElement::TargetValue::TargetValue()
    : opacity(0.0f),
      visibility(false),
      brightness(0.0f),
      grayscale(0.0f),
      color(SK_ColorBLACK) {
}

LayerAnimationElement::TargetValue::TargetValue(
    const LayerAnimationDelegate* delegate)
    : bounds(delegate ? delegate->GetBoundsForAnimation() : gfx::Rect()),
      transform(delegate ? delegate->GetTransformForAnimation()
                         : gfx::Transform()),
      opacity(delegate ? delegate->GetOpacityForAnimation() : 0.0f),
      visibility(delegate ? delegate->GetVisibilityForAnimation() : false),
      brightness(delegate ? delegate->GetBrightnessForAnimation() : 0.0f),
      grayscale(delegate ? delegate->GetGrayscaleForAnimation() : 0.0f),
      color(delegate ? delegate->GetColorForAnimation() : SK_ColorTRANSPARENT),
      clip_rect(delegate ? delegate->GetClipRectForAnimation() : gfx::Rect()),
      rounded_corners(delegate ? delegate->GetRoundedCornersForAnimation()
                               : gfx::RoundedCornersF()),
      gradient_mask(delegate ? delegate->GetGradientMaskForAnimation()
                             : gfx::LinearGradient::GetEmpty()) {}

// LayerAnimationElement -------------------------------------------------------

LayerAnimationElement::LayerAnimationElement(AnimatableProperties properties,
                                             base::TimeDelta duration)
    : first_frame_(true),
      properties_(properties),
      duration_(GetEffectiveDuration(duration)),
      tween_type_(gfx::Tween::LINEAR),
      keyframe_model_id_(cc::AnimationIdProvider::NextKeyframeModelId()),
      animation_group_id_(0),
      last_progressed_fraction_(0.0) {}

LayerAnimationElement::LayerAnimationElement(
    const LayerAnimationElement& element)
    : first_frame_(element.first_frame_),
      properties_(element.properties_),
      duration_(element.duration_),
      tween_type_(element.tween_type_),
      keyframe_model_id_(cc::AnimationIdProvider::NextKeyframeModelId()),
      animation_group_id_(element.animation_group_id_),
      last_progressed_fraction_(element.last_progressed_fraction_) {}

LayerAnimationElement::~LayerAnimationElement() = default;

void LayerAnimationElement::Start(LayerAnimationDelegate* delegate,
                                  int animation_group_id) {
  DCHECK(requested_start_time_ != base::TimeTicks());
  DCHECK(first_frame_);
  animation_group_id_ = animation_group_id;
  last_progressed_fraction_ = 0.0;
  OnStart(delegate);
  RequestEffectiveStart(delegate);
  first_frame_ = false;
}

bool LayerAnimationElement::Progress(base::TimeTicks now,
                                     LayerAnimationDelegate* delegate) {
  DCHECK(requested_start_time_ != base::TimeTicks());
  DCHECK(!first_frame_);

  bool need_draw;
  double t = 1.0;

  if ((effective_start_time_ == base::TimeTicks()) ||
      (now < effective_start_time_))  {
    // This hasn't actually started yet.
    need_draw = false;
    last_progressed_fraction_ = 0.0;
    return need_draw;
  }

  base::TimeDelta elapsed = now - effective_start_time_;
  if ((duration_.is_positive()) && (elapsed < duration_))
    t = elapsed / duration_;
  base::WeakPtr<LayerAnimationElement> alive(weak_ptr_factory_.GetWeakPtr());
  need_draw = OnProgress(gfx::Tween::CalculateValue(tween_type_, t), delegate);
  if (!alive)
    return need_draw;
  first_frame_ = t == 1.0;
  last_progressed_fraction_ = t;
  return need_draw;
}

bool LayerAnimationElement::IsFinished(base::TimeTicks time,
                                       base::TimeDelta* total_duration) {
  // If an effective start has been requested but the effective start time
  // hasn't yet been set, the keyframe_model is not finished, regardless of the
  // value of |time|.
  if (!first_frame_ && (effective_start_time_ == base::TimeTicks()))
    return false;

  base::TimeDelta queueing_delay;
  if (!first_frame_)
    queueing_delay = effective_start_time_ - requested_start_time_;

  base::TimeDelta elapsed = time - requested_start_time_;
  if (elapsed >= duration_ + queueing_delay) {
    *total_duration = duration_ + queueing_delay;
    return true;
  }
  return false;
}

bool LayerAnimationElement::ProgressToEnd(LayerAnimationDelegate* delegate) {
  if (first_frame_)
    OnStart(delegate);

  base::WeakPtr<LayerAnimationElement> alive(weak_ptr_factory_.GetWeakPtr());
  bool need_draw = OnProgress(1.0, delegate);

  if (!alive)
    return need_draw;
  last_progressed_fraction_ = 1.0;
  first_frame_ = true;
  return need_draw;
}

void LayerAnimationElement::GetTargetValue(TargetValue* target) const {
  OnGetTarget(target);
}

bool LayerAnimationElement::IsThreaded(LayerAnimationDelegate* delegate) const {
  return false;
}

void LayerAnimationElement::Abort(LayerAnimationDelegate* delegate) {
  OnAbort(delegate);
  first_frame_ = true;
}

void LayerAnimationElement::RequestEffectiveStart(
    LayerAnimationDelegate* delegate) {
  DCHECK(requested_start_time_ != base::TimeTicks());
  effective_start_time_ = requested_start_time_;
}

std::string LayerAnimationElement::ToString() const {
  // TODO(wkorman): Add support for subclasses to tack on more info
  // beyond just their name.
  return base::StringPrintf(
      "LayerAnimationElement{name=%s, id=%d, group=%d, "
      "last_progressed_fraction=%0.2f}",
      DebugName().c_str(), keyframe_model_id_, animation_group_id_,
      last_progressed_fraction_);
}

void LayerAnimationElement::UpdateKeyframeModelId() {
  keyframe_model_id_ = cc::AnimationIdProvider::NextKeyframeModelId();
}

std::string LayerAnimationElement::DebugName() const {
  return "Default";
}

// static
LayerAnimationElement::AnimatableProperty
LayerAnimationElement::ToAnimatableProperty(cc::TargetProperty::Type property) {
  switch (property) {
    case cc::TargetProperty::TRANSFORM:
      return TRANSFORM;
    case cc::TargetProperty::OPACITY:
      return OPACITY;
    default:
      NOTREACHED_IN_MIGRATION();
      return AnimatableProperty();
  }
}

// static
std::string LayerAnimationElement::AnimatablePropertiesToString(
    AnimatableProperties properties) {
  std::string str;
  int property_count = 0;
  for (unsigned i = FIRST_PROPERTY; i != SENTINEL; i = i << 1) {
    if (i & properties) {
      LayerAnimationElement::AnimatableProperty property =
          static_cast<LayerAnimationElement::AnimatableProperty>(i);
      if (property_count > 0)
        str.append("|");
      // TODO(wkorman): Consider reworking enum definition to follow
      // #define pattern that includes easier string output.
      switch (property) {
        case UNKNOWN:
          str.append("UNKNOWN");
          break;
        case TRANSFORM:
          str.append("TRANSFORM");
          break;
        case BOUNDS:
          str.append("BOUNDS");
          break;
        case OPACITY:
          str.append("OPACITY");
          break;
        case VISIBILITY:
          str.append("VISIBILITY");
          break;
        case BRIGHTNESS:
          str.append("BRIGHTNESS");
          break;
        case GRAYSCALE:
          str.append("GRAYSCALE");
          break;
        case COLOR:
          str.append("COLOR");
          break;
        case CLIP:
          str.append("CLIP");
          break;
        case ROUNDED_CORNERS:
          str.append("ROUNDED_CORNERS");
          break;
        case GRADIENT_MASK:
          str.append("GRADIENT_MASK");
          break;
        case SENTINEL:
          NOTREACHED_IN_MIGRATION();
          break;
      }
      property_count++;
    }
  }
  return str;
}

// static
base::TimeDelta LayerAnimationElement::GetEffectiveDuration(
    const base::TimeDelta& duration) {
  if (ScopedAnimationDurationScaleMode::duration_multiplier() == 0)
    return base::TimeDelta();

  return duration * ScopedAnimationDurationScaleMode::duration_multiplier();
}

// static
std::unique_ptr<LayerAnimationElement>
LayerAnimationElement::CreateTransformElement(const gfx::Transform& transform,
                                              base::TimeDelta duration) {
  return std::make_unique<ThreadedTransformTransition>(transform, duration);
}

// static
std::unique_ptr<LayerAnimationElement>
LayerAnimationElement::CreateInterpolatedTransformElement(
    std::unique_ptr<InterpolatedTransform> interpolated_transform,
    base::TimeDelta duration) {
  return std::make_unique<InterpolatedTransformTransition>(
      std::move(interpolated_transform), duration);
}

// static
std::unique_ptr<LayerAnimationElement>
LayerAnimationElement::CreateBoundsElement(const gfx::Rect& bounds,
                                           base::TimeDelta duration) {
  return std::make_unique<BoundsTransition>(bounds, duration);
}

// static
std::unique_ptr<LayerAnimationElement>
LayerAnimationElement::CreateOpacityElement(float opacity,
                                            base::TimeDelta duration) {
  return std::make_unique<ThreadedOpacityTransition>(opacity, duration);
}

// static
std::unique_ptr<LayerAnimationElement>
LayerAnimationElement::CreateVisibilityElement(bool visibility,
                                               base::TimeDelta duration) {
  return std::make_unique<VisibilityTransition>(visibility, duration);
}

// static
std::unique_ptr<LayerAnimationElement>
LayerAnimationElement::CreateBrightnessElement(float brightness,
                                               base::TimeDelta duration) {
  return std::make_unique<BrightnessTransition>(brightness, duration);
}

// static
std::unique_ptr<LayerAnimationElement>
LayerAnimationElement::CreateGrayscaleElement(float grayscale,
                                              base::TimeDelta duration) {
  return std::make_unique<GrayscaleTransition>(grayscale, duration);
}

// static
std::unique_ptr<LayerAnimationElement>
LayerAnimationElement::CreatePauseElement(AnimatableProperties properties,
                                          base::TimeDelta duration) {
  return std::make_unique<Pause>(properties, duration);
}

// static
std::unique_ptr<LayerAnimationElement>
LayerAnimationElement::CreateColorElement(SkColor color,
                                          base::TimeDelta duration) {
  return std::make_unique<ColorTransition>(color, duration);
}

std::unique_ptr<LayerAnimationElement>
LayerAnimationElement::CreateClipRectElement(const gfx::Rect& clip_rect,
                                             base::TimeDelta duration) {
  return std::make_unique<ClipRectTransition>(clip_rect, duration);
}

std::unique_ptr<LayerAnimationElement>
LayerAnimationElement::CreateRoundedCornersElement(
    const gfx::RoundedCornersF& rounded_corners,
    base::TimeDelta duration) {
  return std::make_unique<RoundedCornersTransition>(rounded_corners, duration);
}

std::unique_ptr<LayerAnimationElement>
LayerAnimationElement::CreateGradientMaskElement(
    const gfx::LinearGradient& gradient_mask,
    base::TimeDelta duration) {
  return std::make_unique<GradientMaskTransition>(gradient_mask, duration);
}

}  // namespace ui

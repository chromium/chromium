// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_ANIMATION_KEYFRAME_KEYFRAMED_ANIMATION_CURVE_INL_H_
#define UI_GFX_ANIMATION_KEYFRAME_KEYFRAMED_ANIMATION_CURVE_INL_H_

#define KEYFRAMED_ANIMATION_CURVE_DEFINITION(T, Name, BlendFn)              \
  std::unique_ptr<Name##Keyframe> Name##Keyframe::Create(                   \
      base::TimeDelta time, const T& value,                                 \
      std::unique_ptr<gfx::TimingFunction> timing_function) {               \
    return base::WrapUnique(                                                \
        new Name##Keyframe(time, value, std::move(timing_function)));       \
  }                                                                         \
  Name##Keyframe::Name##Keyframe(                                           \
      base::TimeDelta time, const T& value,                                 \
      std::unique_ptr<gfx::TimingFunction> timing_function)                 \
      : Keyframe(time, std::move(timing_function)), value_(value) {}        \
  Name##Keyframe::~Name##Keyframe() = default;                              \
  T Name##Keyframe::Value() const { return value_; }                        \
  std::unique_ptr<Name##Keyframe> Name##Keyframe::Clone() const {           \
    std::unique_ptr<gfx::TimingFunction> func;                              \
    if (timing_function())                                                  \
      func = timing_function()->Clone();                                    \
    return Name##Keyframe::Create(Time(), Value(), std::move(func));        \
  }                                                                         \
  void Keyframed##Name##AnimationCurve::AddKeyframe(                        \
      std::unique_ptr<Name##Keyframe> keyframe) {                           \
    size_t insert_before = keyframes_.size();                               \
    while (insert_before > 0 &&                                             \
           keyframe->Time() < keyframes_.at(insert_before - 1)->Time())     \
      insert_before--;                                                      \
    keyframes_.insert(keyframes_.begin() + insert_before,                   \
                      std::move(keyframe));                                 \
  }                                                                         \
  std::unique_ptr<Keyframed##Name##AnimationCurve>                          \
      Keyframed##Name##AnimationCurve::Create() {                           \
    return base::WrapUnique(new Keyframed##Name##AnimationCurve);           \
  }                                                                         \
  Keyframed##Name##AnimationCurve::Keyframed##Name##AnimationCurve()        \
      : scaled_duration_(1.0) {}                                            \
  Keyframed##Name##AnimationCurve::~Keyframed##Name##AnimationCurve() =     \
      default;                                                              \
  base::TimeDelta Keyframed##Name##AnimationCurve::Duration() const {       \
    return (keyframes_.back()->Time() - keyframes_.front()->Time()) *       \
           scaled_duration();                                               \
  }                                                                         \
  std::unique_ptr<gfx::AnimationCurve>                                      \
      Keyframed##Name##AnimationCurve::Clone() const {                      \
    std::unique_ptr<Keyframed##Name##AnimationCurve> to_return =            \
        Keyframed##Name##AnimationCurve::Create();                          \
    for (const auto& keyframe : keyframes_)                                 \
      to_return->AddKeyframe(keyframe->Clone());                            \
                                                                            \
    if (timing_function_)                                                   \
      to_return->SetTimingFunction(timing_function_->Clone());              \
                                                                            \
    to_return->set_scaled_duration(scaled_duration());                      \
                                                                            \
    return std::move(to_return);                                            \
  }                                                                         \
  T Keyframed##Name##AnimationCurve::GetValue(base::TimeDelta t) const {    \
    if (t <= (keyframes_.front()->Time() * scaled_duration()))              \
      return keyframes_.front()->Value();                                   \
                                                                            \
    if (t >= (keyframes_.back()->Time() * scaled_duration()))               \
      return keyframes_.back()->Value();                                    \
                                                                            \
    t = gfx::ComputeTransformedAnimationTime(                               \
        keyframes_.front()->Time(), keyframes_.back()->Time(),              \
        timing_function_, scaled_duration(), t);                            \
    DCHECK_GE(keyframes_.size(), 2ul);                                      \
    size_t i = 0;                                                           \
    while ((i < keyframes_.size() - 2) &&                                   \
           (t >= (keyframes_[i + 1]->Time() * scaled_duration())))          \
      ++i;                                                                  \
    double progress = gfx::ComputeKeyframeAnimationTimeValues(              \
                          keyframes_[i]->Time(), keyframes_[i + 1]->Time(), \
                          scaled_duration(), t)                             \
                          .progress;                                        \
    progress = keyframes_[i]->timing_function()                             \
                   ? keyframes_[i]->timing_function()->GetValue(progress)   \
                   : progress;                                              \
                                                                            \
    return BlendFn(progress, keyframes_[i]->Value(),                        \
                   keyframes_[i + 1]->Value());                             \
  }

#endif  // UI_GFX_ANIMATION_KEYFRAME_KEYFRAMED_ANIMATION_CURVE_INL_H_

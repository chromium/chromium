// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_ANIMATION_KEYFRAME_KEYFRAMED_ANIMATION_CURVE_INL_H_
#define UI_GFX_ANIMATION_KEYFRAME_KEYFRAMED_ANIMATION_CURVE_INL_H_

namespace {

template <class KeyframeType>
void InsertKeyframe(std::unique_ptr<KeyframeType> keyframe,
                    std::vector<std::unique_ptr<KeyframeType>>* keyframes) {
  // Usually, the keyframes will be added in order, so this loop would be
  // unnecessary and we should skip it if possible.
  if (!keyframes->empty() && keyframe->Time() < keyframes->back()->Time()) {
    for (size_t i = 0; i < keyframes->size(); ++i) {
      if (keyframe->Time() < keyframes->at(i)->Time()) {
        keyframes->insert(keyframes->begin() + i, std::move(keyframe));
        return;
      }
    }
  }

  keyframes->push_back(std::move(keyframe));
}

struct TimeValues {
  base::TimeDelta start_time;
  base::TimeDelta duration;
  double progress;
};

template <typename KeyframeType>
TimeValues GetTimeValues(const KeyframeType& start_frame,
                         const KeyframeType& end_frame,
                         double scaled_duration,
                         base::TimeDelta time) {
  TimeValues values;
  values.start_time = start_frame.Time() * scaled_duration;
  values.duration = (end_frame.Time() * scaled_duration) - values.start_time;
  const base::TimeDelta elapsed = time - values.start_time;
  values.progress = (elapsed.is_inf() || values.duration.is_zero())
                        ? 1.0
                        : (elapsed / values.duration);
  return values;
}

template <typename KeyframeType>
base::TimeDelta TransformedAnimationTime(
    const std::vector<std::unique_ptr<KeyframeType>>& keyframes,
    const std::unique_ptr<gfx::TimingFunction>& timing_function,
    double scaled_duration,
    base::TimeDelta time) {
  if (timing_function) {
    const auto values = GetTimeValues(*keyframes.front(), *keyframes.back(),
                                      scaled_duration, time);
    time = (values.duration * timing_function->GetValue(values.progress)) +
           values.start_time;
  }

  return time;
}

template <typename KeyframeType>
size_t GetActiveKeyframe(
    const std::vector<std::unique_ptr<KeyframeType>>& keyframes,
    double scaled_duration,
    base::TimeDelta time) {
  DCHECK_GE(keyframes.size(), 2ul);
  size_t i = 0;
  while ((i < keyframes.size() - 2) &&  // Last keyframe is never active.
         (time >= (keyframes[i + 1]->Time() * scaled_duration)))
    ++i;

  return i;
}

template <typename KeyframeType>
double TransformedKeyframeProgress(
    const std::vector<std::unique_ptr<KeyframeType>>& keyframes,
    double scaled_duration,
    base::TimeDelta time,
    size_t i) {
  const double progress =
      GetTimeValues(*keyframes[i], *keyframes[i + 1], scaled_duration, time)
          .progress;
  return keyframes[i]->timing_function()
             ? keyframes[i]->timing_function()->GetValue(progress)
             : progress;
}

int GetTimingFunctionSteps(const gfx::TimingFunction* timing_function) {
  DCHECK(timing_function &&
         timing_function->GetType() == gfx::TimingFunction::Type::STEPS);
  const gfx::StepsTimingFunction* steps_timing_function =
      reinterpret_cast<const gfx::StepsTimingFunction*>(timing_function);
  DCHECK(steps_timing_function);
  return steps_timing_function->steps();
}

template <class KeyframeType>
base::TimeDelta ComputeTickInterval(
    const std::unique_ptr<gfx::TimingFunction>& timing_function,
    double scaled_duration,
    const std::vector<std::unique_ptr<KeyframeType>>& keyframes) {
  // TODO(crbug.com/1140603): include animation progress in order to pinpoint
  // which keyframe's timing function is in effect at any point in time.
  DCHECK_LT(0u, keyframes.size());
  gfx::TimingFunction::Type timing_function_type =
      timing_function ? timing_function->GetType()
                      : gfx::TimingFunction::Type::LINEAR;
  // Even if the keyframe's have step timing functions, a non-linear
  // animation-wide timing function results in unevenly timed steps.
  switch (timing_function_type) {
    case gfx::TimingFunction::Type::LINEAR: {
      base::TimeDelta min_interval = base::TimeDelta::Max();
      // If any keyframe uses non-step "easing", return 0, except for the last
      // keyframe, whose "easing" is never used.
      for (size_t ii = 0; ii < keyframes.size() - 1; ++ii) {
        KeyframeType* keyframe = keyframes[ii].get();
        if (!keyframe->timing_function() ||
            keyframe->timing_function()->GetType() !=
                gfx::TimingFunction::Type::STEPS) {
          return base::TimeDelta();
        }
        KeyframeType* next_keyframe = keyframes[ii + 1].get();
        int steps = GetTimingFunctionSteps(keyframe->timing_function());
        DCHECK_LT(0, steps);
        base::TimeDelta interval = (next_keyframe->Time() - keyframe->Time()) *
                                   scaled_duration / steps;
        if (interval < min_interval)
          min_interval = interval;
      }
      return min_interval;
    }
    case gfx::TimingFunction::Type::STEPS: {
      return (keyframes.back()->Time() - keyframes.front()->Time()) *
             scaled_duration / GetTimingFunctionSteps(timing_function.get());
    }
    case gfx::TimingFunction::Type::CUBIC_BEZIER:
      break;
  }
  return base::TimeDelta();
}

}  // namespace

#endif  // UI_GFX_ANIMATION_KEYFRAME_KEYFRAMED_ANIMATION_CURVE_INL_H_

// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/animation/multi_animation.h"

#include <numeric>

#include "base/check_op.h"
#include "base/notreached.h"
#include "ui/gfx/animation/animation_delegate.h"

namespace gfx {

static base::TimeDelta TotalTime(const MultiAnimation::Parts& parts) {
  return std::accumulate(parts.cbegin(), parts.cend(), base::TimeDelta(),
                         [](base::TimeDelta total, const auto& part) {
                           return total + part.length;
                         });
}

// static
constexpr base::TimeDelta MultiAnimation::kDefaultTimerInterval;

MultiAnimation::MultiAnimation(const Parts& parts,
                               base::TimeDelta timer_interval)
    : Animation(timer_interval), parts_(parts), cycle_time_(TotalTime(parts)) {
  DCHECK(!parts_.empty());
}

MultiAnimation::~MultiAnimation() = default;

double MultiAnimation::GetCurrentValue() const {
  const Part& current_part = parts_[current_part_index_];
  return Tween::DoubleValueBetween(
      Tween::CalculateValue(current_part.type, current_part_state_),
      current_part.start_value, current_part.end_value);
}

void MultiAnimation::Step(base::TimeTicks time_now) {
  double last_value = GetCurrentValue();
  size_t last_index = current_part_index_;

  base::TimeDelta delta = time_now - start_time();
  bool should_stop = delta >= cycle_time_ && !continuous_;
  if (should_stop) {
    current_part_index_ = parts_.size() - 1;
    current_part_state_ = 1.0;
  } else {
    delta %= cycle_time_;
    const Part& part = GetPart(&delta, &current_part_index_);
    current_part_state_ = delta / part.length;
    DCHECK_LE(current_part_state_, 1);
  }

  if ((GetCurrentValue() != last_value || current_part_index_ != last_index) &&
      delegate()) {
    // Run AnimationProgressed() even if the animation will be stopped, so that
    // the animation runs its final frame.
    delegate()->AnimationProgressed(this);
  }
  if (should_stop)
    Stop();
}

void MultiAnimation::SetStartTime(base::TimeTicks start_time) {
  Animation::SetStartTime(start_time);
  current_part_state_ = 0.0;
  current_part_index_ = 0;
}

const MultiAnimation::Part& MultiAnimation::GetPart(base::TimeDelta* time,
                                                    size_t* part_index) {
  DCHECK_LT(*time, cycle_time_);

  for (size_t i = 0; i < parts_.size(); ++i) {
    if (*time < parts_[i].length) {
      *part_index = i;
      return parts_[i];
    }

    *time -= parts_[i].length;
  }
  NOTREACHED_IN_MIGRATION();
  return parts_[0];
}

}  // namespace gfx

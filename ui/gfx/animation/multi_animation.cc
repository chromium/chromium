// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/animation/multi_animation.h"

#include <numeric>

#include "base/logging.h"
#include "ui/gfx/animation/animation_delegate.h"

namespace gfx {

static base::TimeDelta TotalTime(const MultiAnimation::Parts& parts) {
  return std::accumulate(parts.cbegin(), parts.cend(), base::TimeDelta(),
                         [](base::TimeDelta total, const auto& part) {
                           return total + part.part_length;
                         });
}

// static
constexpr base::TimeDelta MultiAnimation::kDefaultTimerInterval;

MultiAnimation::MultiAnimation(const Parts& parts,
                               base::TimeDelta timer_interval)
    : Animation(timer_interval),
      parts_(parts),
      cycle_time_(TotalTime(parts)),
      current_value_(0),
      current_part_index_(0),
      continuous_(true) {
  DCHECK(!parts_.empty());
  for (const auto& part : parts)
    DCHECK_GE(part.total_length - part.part_start, part.part_length);
}

MultiAnimation::~MultiAnimation() = default;

double MultiAnimation::GetCurrentValue() const {
  return current_value_;
}

void MultiAnimation::Step(base::TimeTicks time_now) {
  double last_value = current_value_;
  size_t last_index = current_part_index_;

  base::TimeDelta delta = time_now - start_time();
  if (delta >= cycle_time_ && !continuous_) {
    current_part_index_ = parts_.size() - 1;
    current_value_ = Tween::CalculateValue(parts_[current_part_index_].type, 1);
    Stop();
    return;
  }
  delta %= cycle_time_;
  const Part& part = GetPart(&delta, &current_part_index_);
  double percent = (delta + part.part_start).InMillisecondsF() /
                   part.total_length.InMillisecondsF();
  DCHECK_LE(percent, 1);
  current_value_ = Tween::CalculateValue(part.type, percent);

  if ((current_value_ != last_value || current_part_index_ != last_index) &&
      delegate()) {
    delegate()->AnimationProgressed(this);
  }
}

void MultiAnimation::SetStartTime(base::TimeTicks start_time) {
  Animation::SetStartTime(start_time);
  current_value_ = 0;
  current_part_index_ = 0;
}

const MultiAnimation::Part& MultiAnimation::GetPart(base::TimeDelta* time,
                                                    size_t* part_index) {
  DCHECK_LT(*time, cycle_time_);

  for (size_t i = 0; i < parts_.size(); ++i) {
    if (*time < parts_[i].part_length) {
      *part_index = i;
      return parts_[i];
    }

    *time -= parts_[i].part_length;
  }
  NOTREACHED();
  return parts_[0];
}

}  // namespace gfx

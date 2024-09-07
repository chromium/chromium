// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/animation/keyframe/keyframe_model.h"

#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/time/time.h"

namespace gfx {
namespace {

// This should match the RunState enum.
static constexpr auto s_runStateNames = std::to_array<const char*>(
    {"WAITING_FOR_TARGET_AVAILABILITY", "WAITING_FOR_DELETION", "STARTING",
     "RUNNING", "PAUSED", "FINISHED", "ABORTED",
     "ABORTED_BUT_NEEDS_COMPLETION"});

static_assert(static_cast<int>(KeyframeModel::LAST_RUN_STATE) + 1 ==
                  std::size(s_runStateNames),
              "RunStateEnumSize should equal the number of elements in "
              "s_runStateNames");

}  // namespace

std::string KeyframeModel::ToString(RunState state) {
  return s_runStateNames[state];
}

std::unique_ptr<KeyframeModel> KeyframeModel::Create(
    std::unique_ptr<AnimationCurve> curve,
    int keyframe_model_id,
    int target_property_id) {
  return base::WrapUnique(new KeyframeModel(std::move(curve), keyframe_model_id,
                                            target_property_id));
}

KeyframeModel::KeyframeModel(std::unique_ptr<AnimationCurve> curve,
                             int keyframe_model_id,
                             int target_property_id)
    : curve_(std::move(curve)),
      id_(keyframe_model_id),
      target_property_(target_property_id),
      run_state_(WAITING_FOR_TARGET_AVAILABILITY),
      iterations_(1),
      iteration_start_(0),
      direction_(Direction::NORMAL),
      playback_rate_(1),
      fill_mode_(FillMode::BOTH) {}

KeyframeModel::~KeyframeModel() {
  if (run_state() == RUNNING || run_state() == PAUSED)
    SetRunState(ABORTED, base::TimeTicks());
}

int KeyframeModel::TargetProperty() const {
  return target_property_;
}

void KeyframeModel::SetRunState(RunState run_state,
                                base::TimeTicks monotonic_time) {
  if (run_state == RUNNING && run_state_ == PAUSED)
    total_paused_duration_ += (monotonic_time - pause_time_);
  else if (run_state == PAUSED)
    pause_time_ = monotonic_time;
  run_state_ = run_state;
}

void KeyframeModel::Pause(base::TimeDelta pause_offset) {
  // Convert pause offset which is in local time to monotonic time.
  // TODO(crbug.com/41430321): This should be scaled by playbackrate.
  base::TimeTicks monotonic_time = pause_offset +
                                   start_time_.value_or(base::TimeTicks()) +
                                   total_paused_duration_;
  SetRunState(PAUSED, monotonic_time);
}

KeyframeModel::Phase KeyframeModel::CalculatePhaseForTesting(
    base::TimeDelta local_time) const {
  return CalculatePhase(local_time);
}

KeyframeModel::Phase KeyframeModel::CalculatePhase(
    base::TimeDelta local_time) const {
  base::TimeDelta opposite_time_offset = time_offset_ == base::TimeDelta::Min()
                                             ? base::TimeDelta::Max()
                                             : -time_offset_;
  base::TimeDelta before_active_boundary_time =
      std::max(opposite_time_offset, base::TimeDelta());
  if ((local_time < before_active_boundary_time) ||
      (local_time == before_active_boundary_time && playback_rate_ < 0)) {
    return KeyframeModel::Phase::BEFORE;
  }
  // TODO(crbug.com/41428771): By spec end time = max(start delay + duration +
  // end delay, 0). The logic should be updated once "end delay" is supported.
  base::TimeDelta active_after_boundary_time = base::TimeDelta::Max();
  if (std::isfinite(iterations_)) {
    // Scaling the duration is against spec but needed to comply with the cc
    // implementation. By spec (in blink) the playback rate is an Animation
    // level concept but in cc it's per KeyframeModel. We grab the active time
    // calculated here and later scale it with the playback rate in order to get
    // a proper progress. Therefore we need to un-scale it here. This can be
    // fixed once we scale the local time by playback rate. See
    // https://crbug.com/912407.
    base::TimeDelta active_duration =
        curve_->Duration() * iterations_ / std::abs(playback_rate_);
    active_after_boundary_time =
        std::max(opposite_time_offset + active_duration, base::TimeDelta());
  }
  if ((local_time > active_after_boundary_time) ||
      (local_time == active_after_boundary_time && playback_rate_ > 0)) {
    return KeyframeModel::Phase::AFTER;
  }
  return KeyframeModel::Phase::ACTIVE;
}

std::optional<base::TimeDelta> KeyframeModel::CalculateActiveTime(
    base::TimeTicks monotonic_time) const {
  base::TimeDelta local_time = ConvertMonotonicTimeToLocalTime(monotonic_time);
  KeyframeModel::Phase phase = CalculatePhase(local_time);
  return CalculateActiveTime(local_time, phase);
}

std::optional<base::TimeDelta> KeyframeModel::CalculateActiveTime(
    base::TimeDelta local_time,
    KeyframeModel::Phase phase) const {
  DCHECK(playback_rate_);
  switch (phase) {
    case KeyframeModel::Phase::BEFORE:
      if (fill_mode_ == FillMode::BACKWARDS || fill_mode_ == FillMode::BOTH)
        return std::max(local_time + time_offset_, base::TimeDelta());
      return std::nullopt;
    case KeyframeModel::Phase::ACTIVE:
      return local_time + time_offset_;
    case KeyframeModel::Phase::AFTER:
      if (fill_mode_ == FillMode::FORWARDS || fill_mode_ == FillMode::BOTH) {
        DCHECK_NE(iterations_, std::numeric_limits<double>::infinity());
        base::TimeDelta active_duration =
            curve_->Duration() * iterations_ / std::abs(playback_rate_);
        return std::max(std::min(local_time + time_offset_, active_duration),
                        base::TimeDelta());
      }
      return std::nullopt;
    default:
      NOTREACHED_IN_MIGRATION();
      return std::nullopt;
  }
}

bool KeyframeModel::IsFinishedAt(base::TimeTicks monotonic_time) const {
  if (is_finished())
    return true;

  if (StartShouldBeDeferred())
    return false;

  if (playback_rate_ == 0)
    return false;

  return run_state_ == RUNNING && std::isfinite(iterations_) &&
         (curve_->Duration() * (iterations_ / std::abs(playback_rate_))) <=
             (ConvertMonotonicTimeToLocalTime(monotonic_time) + time_offset_);
}

bool KeyframeModel::HasActiveTime(base::TimeTicks monotonic_time) const {
  return CalculateActiveTime(monotonic_time).has_value();
}

bool KeyframeModel::StartShouldBeDeferred() const {
  return false;
}

base::TimeDelta KeyframeModel::TrimTimeToCurrentIteration(
    base::TimeTicks monotonic_time,
    TimingFunction::LimitDirection* limit_direction) const {
  DCHECK(playback_rate_);
  DCHECK_GE(iteration_start_, 0);

  DCHECK(HasActiveTime(monotonic_time));

  base::TimeDelta local_time = ConvertMonotonicTimeToLocalTime(monotonic_time);
  KeyframeModel::Phase phase = CalculatePhase(local_time);
  base::TimeDelta active_time = CalculateActiveTime(local_time, phase).value();
  base::TimeDelta start_offset = curve_->Duration() * iteration_start_;

  if (limit_direction) {
    if (phase == KeyframeModel::Phase::BEFORE) {
      *limit_direction = TimingFunction::LimitDirection::LEFT;
    } else {
      *limit_direction = TimingFunction::LimitDirection::RIGHT;
    }
  }

  DCHECK(!active_time.is_negative());

  // Always return zero if we have no iterations.
  if (!iterations_) {
    return base::TimeDelta();
  }

  // Don't attempt to trim if we have no duration.
  if (curve_->Duration() <= base::TimeDelta()) {
    return base::TimeDelta();
  }

  base::TimeDelta repeated_duration = std::isfinite(iterations_)
                                          ? (curve_->Duration() * iterations_)
                                          : base::TimeDelta::Max();

  // Calculate the scaled active time
  base::TimeDelta scaled_active_time;
  if (playback_rate_ < 0) {
    DCHECK(std::isfinite(iterations_));
    base::TimeDelta active_duration =
        repeated_duration / std::abs(playback_rate_);
    scaled_active_time =
        ((active_time - active_duration) * playback_rate_) + start_offset;
  } else {
    scaled_active_time = (active_time * playback_rate_) + start_offset;
  }

  // Calculate the iteration time
  base::TimeDelta iteration_time;
  bool has_defined_time_delta =
      (start_offset != scaled_active_time) ||
      !(start_offset.is_max() || start_offset.is_min());
  if (has_defined_time_delta &&
      scaled_active_time - start_offset == repeated_duration &&
      fmod(iterations_ + iteration_start_, 1) == 0)
    iteration_time = curve_->Duration();
  else
    iteration_time = scaled_active_time % curve_->Duration();

  // Calculate the current iteration
  int iteration;
  if (scaled_active_time <= base::TimeDelta())
    iteration = 0;
  else if (iteration_time == curve_->Duration())
    iteration = ceil(iteration_start_ + iterations_ - 1);
  else
    iteration = base::ClampFloor(scaled_active_time / curve_->Duration());

  // Check if we are running the keyframe model in reverse direction for the
  // current iteration
  bool reverse =
      (direction_ == Direction::REVERSE) ||
      (direction_ == Direction::ALTERNATE_NORMAL && iteration % 2 == 1) ||
      (direction_ == Direction::ALTERNATE_REVERSE && iteration % 2 == 0);

  // If we are running the keyframe model in reverse direction, reverse the
  // result
  if (reverse)
    iteration_time = curve_->Duration() - iteration_time;

  return iteration_time;
}

// TODO(crbug.com/41430321): Local time should be scaled by playback rate by
// spec.
base::TimeDelta KeyframeModel::ConvertMonotonicTimeToLocalTime(
    base::TimeTicks monotonic_time) const {
  // When waiting on receiving a start time, then our global clock is 'stuck' at
  // the initial state.
  if ((run_state_ == STARTING && !has_set_start_time()) ||
      StartShouldBeDeferred())
    return base::TimeDelta();

  // If we're paused, time is 'stuck' at the pause time.
  base::TimeTicks time = (run_state_ == PAUSED) ? pause_time_ : monotonic_time;
  return time - start_time_.value_or(base::TimeTicks()) -
         total_paused_duration_;
}

}  // namespace gfx
